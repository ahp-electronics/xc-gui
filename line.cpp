#include "line.h"
#include "ui_line.h"
#include <QMapNode>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>

Line::Line(QString ln, int n, QSettings *s, QWidget *parent, QList<Line*> *p) :
    QWidget(parent),
    ui(new Ui::Line)
{
    setAccessibleName("Line");
    setActive(false);
    settings = s;
    percent = 0;
    parents = p;
    name = ln;
    series.setName(name+" coherence");
    counts.setName(name+" (counts)");
    autocorrelations.setName(name+" (autocorrelations)");
    crosscorrelations.setName(name+" (crosscorrelations)");
    line = n;
    flags = 0;
    ui->setupUi(this);
    ui->SpectralLine->setRange(0, ahp_xc_get_delaysize()-7);
    ui->StartLine->setRange(0, ahp_xc_get_delaysize()-7);
    ui->EndLine->setRange(5, ahp_xc_get_delaysize()-1);
    ui->CrossStart->setRange(-ahp_xc_get_delaysize()+1, ahp_xc_get_delaysize()-1);
    ui->CrossEnd->setRange(-ahp_xc_get_delaysize()+6, ahp_xc_get_delaysize()-1);
    old_index2 = readInt("Index2", (line == 0 ? 2 : (line == ahp_xc_get_nlines()-1 ? ahp_xc_get_nlines()-1 : line )));
    ui->Index2->setRange(1, ahp_xc_get_nlines());
    ui->Index2->setValue(old_index2);
    ui->StartLine->setValue(readInt("StartLine", ui->StartLine->minimum()));
    ui->EndLine->setValue(readInt("EndLine", ui->EndLine->maximum()));
    ui->CrossStart->setValue(readInt("CrossStart", ui->CrossStart->minimum()));
    ui->CrossEnd->setValue(readInt("CrossEnd", ui->CrossEnd->maximum()));
    ui->SpectralLine->setValue(readInt("SpectralLine", ui->SpectralLine->minimum()));
    connect(ui->flag0, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 0);
        flags |= ui->flag0->isChecked() << 0;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag0->text(), ui->flag0->isChecked());
    });
    connect(ui->flag1, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 1);
        flags |= ui->flag1->isChecked() << 1;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag1->text(), ui->flag1->isChecked());
    });
    connect(ui->flag2, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 2);
        flags |= ui->flag2->isChecked() << 2;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag2->text(), ui->flag2->isChecked());
    });
    connect(ui->flag3, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 3);
        flags |= ui->flag3->isChecked() << 3;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag3->text(), ui->flag3->isChecked());
    });
    connect(ui->test, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        ui->test->isChecked() ? ahp_xc_set_test(line, TEST_SIGNAL) : ahp_xc_clear_test(line, TEST_SIGNAL);
        saveSetting(ui->test->text(), ui->test->isChecked());
    });
    connect(ui->Run, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        if(ui->Run->text() == "Run") {
            setActive(true);
            ui->Run->setText("Stop");
        } else {
            setActive(false);
            ui->Run->setText("Run");
        }
        ui->Counter->setEnabled(false);
        ui->Autocorrelator->setEnabled(false);
        ui->Crosscorrelator->setEnabled(false);
        if(mode == Autocorrelator) {
            ui->Autocorrelator->setEnabled(!isActive());
        } else if(mode == Crosscorrelator) {
            ui->Crosscorrelator->setEnabled(!isActive());
        } else {
            ui->Counter->setEnabled(!isActive());
        }
        for(int x = 0; x < nodes.count(); x++)
            nodes[x]->setActive(nodes[x]->getLine1()->isActive()&&nodes[x]->getLine2()->isActive()&&mode==Crosscorrelator);

    });
    connect(ui->Save, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        QString filename = QFileDialog::getSaveFileName(this, "DialogTitle", "filename.csv", "CSV files (.csv);;Zip files (.zip, *.7z)", 0, 0); // getting the filename (full path)
        QFile data(filename);
        if(data.open(QFile::WriteOnly |QFile::Truncate))
        {
            QTextStream output(&data);
            double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/(((ahp_xc_get_test(line)&TEST_SIGNAL)?AHP_XC_PLL_FREQUENCY:0)+ahp_xc_get_frequency());
            if(mode == Autocorrelator || mode == Crosscorrelator) {
                output << "'notes:';'" << ui->Notes->toPlainText() << "'\n";
                output << "'lag (ns)';'coherence degree'\n";
                for(int x = 0, y = 0; x < series.count(); y++, x++) {
                    output << "'"+QString::number(series.at(y).x())+"';'"+QString::number(series.at(y).y())+"'\n";
                }
            } else {
                output << "time (s);counts;autocorrelations;crosscorrelations\n";
                for(int x = 0, y = 0; x < series.count(); y++, x++) {
                    output << ""+QString::number(counts.at(y).x())+";"+QString::number(counts.at(y).y())+";"+QString::number(autocorrelations.at(y).y())+";"+QString::number(crosscorrelations.at(y).y())+"\n";
                }
            }
        }
        data.close();
    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        if(ui->TakeDark->text() == "Apply Dark") {
            dark.clear();for(int x = 0; x < series.count(); x++) {
                dark.insert(series.at(x).x(), series.at(x).y());
                QString darkstring = readString("Dark", "");
                if(!darkstring.isEmpty())
                    saveSetting("Dark", darkstring+";");
                darkstring = readString("Dark", "");
                saveSetting("Dark", darkstring+QString::number(series.at(x).x())+","+QString::number(series.at(x).y()));
            }
            ui->TakeDark->setText("Clear Dark");
            series.setName(name+" coherence (residuals)");
        } else {
            removeSetting("Dark");
            dark.clear();
            ui->TakeDark->setText("Apply Dark");
            series.setName(name+" coherence");
        }
    });
    connect(ui->CrossDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        if(ui->CrossDark->text() == "Apply Dark") {
            dark.clear();
            double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/(((ahp_xc_get_test(line)&TEST_SIGNAL)?AHP_XC_PLL_FREQUENCY:0)+ahp_xc_get_frequency());
            for(int x = ui->StartLine->value(), y = 0; x < ui->EndLine->value(); y++, x++) {
                dark.insert(x*timespan, series.at(y).y());
                QString darkstring = readString("CrossDark", "");
                if(!darkstring.isEmpty())
                    saveSetting("CrossDark", darkstring+";");
                darkstring = readString("CrossDark", "");
                saveSetting("CrossDark", darkstring+QString::number(x*timespan)+","+QString::number(series.at(y).y()));
            }
            ui->CrossDark->setText("Clear Dark");
            series.setName(name+" coherence (residuals)");
        } else {
            removeSetting("CrossDark");
            dark.clear();
            ui->CrossDark->setText("Apply Dark");
            series.setName(name+" coherence");
        }
    });
    connect(ui->StartLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()-5) {
            ui->EndLine->setValue(ui->StartLine->value()+1);
        }
        saveSetting("StartLine", ui->StartLine->value());
    });
    connect(ui->EndLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()-5) {
            ui->StartLine->setValue(ui->EndLine->value()-1);
        }
        saveSetting("EndLine", ui->EndLine->value());
    });
    connect(ui->CrossStart, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->CrossStart->value()>=ui->CrossEnd->value()-5) {
            ui->CrossEnd->setValue(ui->CrossStart->value()+1);
        }
        saveSetting("CrossStart", ui->CrossStart->value());
    });
    connect(ui->CrossEnd, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->CrossStart->value()>=ui->CrossEnd->value()-5) {
            ui->CrossStart->setValue(ui->CrossEnd->value()-1);
        }
        saveSetting("CrossEnd", ui->CrossEnd->value());
    });
    connect(ui->Clear, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
         clearCorrelations();
         series.clear();
         average.clear();
         emit activeStateChanged(this);
    });
    connect(ui->ClearCross, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
         clearCorrelations();
         series.clear();
         average.clear();
         for(int x = 0; x < nodes.count(); x++)
         {
             nodes[x]->getDots()->clear();
             nodes[x]->getAverage()->clear();
         }
    });
    connect(ui->Index2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        int idx = line+1;
        int new_value = value;
        if(new_value==idx) {
            if(idx == ui->Index2->minimum() || idx == ui->Index2->maximum())
                new_value = old_index2;
            else {
                if(old_index2 > new_value)  {
                    new_value = idx-1;
                } else if(old_index2 < new_value)  {
                    new_value = idx+1;
                }
            }
        }
        old_index2 = new_value;
        ui->Index2->setValue(new_value);
        saveSetting("Index2", new_value);
    });
    connect(ui->Counts, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [=](bool checked) {
        saveSetting(ui->Counts->text(), ui->Counts->isChecked());
    });
    connect(ui->Autocorrelations, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [=](bool checked) {
        saveSetting(ui->Autocorrelations->text(), ui->Autocorrelations->isChecked());
    });
    connect(ui->SpectralLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        ahp_xc_set_lag_auto(line, value);
        saveSetting("SpectralLine", value);
    });
    setFlag(0, readBool(ui->flag0->text(), false));
    setFlag(1, readBool(ui->flag1->text(), false));
    setFlag(2, readBool(ui->flag2->text(), false));
    setFlag(3, readBool(ui->flag3->text(), false));
    ui->test->setChecked(readBool(ui->test->text(), false));
    ui->Counts->setChecked(readBool(ui->Counts->text(), false));
    ui->Autocorrelations->setChecked(readBool(ui->Autocorrelations->text(), false));
    dark.clear();
    if(haveSetting("Dark")) {
        QStringList darkstring = readString("Dark", "").split(";");
        for(int x = 0; x < darkstring.length(); x++) {
            QStringList lag_value = darkstring[x].split(",");
            if(lag_value.length()==2)
                dark.insert(lag_value[0].toDouble(), lag_value[1].toDouble());
        }
        if(dark.count() > 0) {
            ui->TakeDark->setText("Clear Dark");
            series.setName(name+" coherence (residuals)");
        }
    }
    crossdark.clear();
    if(haveSetting("CrossDark")) {
        QStringList darkstring = readString("CrossDark", "").split(";");
        for(int x = 0; x < darkstring.length(); x++) {
            QStringList lag_value = darkstring[x].split(",");
            if(lag_value.length()==2)
                crossdark.insert(lag_value[0].toDouble(), lag_value[1].toDouble());
        }
        if(crossdark.count() > 0) {
            ui->CrossDark->setText("Clear Dark");
            series.setName(name+" coherence (residuals)");
        }
    }
}

unsigned int Line::getLine2() {
    return ui->Index2->value()-1;
}

bool Line::haveSetting(QString setting) {
    return settings->contains(QString(name+"_"+setting).replace(' ', ""));
}

void Line::removeSetting(QString setting) {
    settings->remove(QString(name+"_"+setting).replace(' ', ""));
}

void Line::saveSetting(QString setting, QVariant value)
{
    settings->setValue(QString(name+"_"+setting).replace(' ', ""), value);
}

QVariant Line::readSetting(QString setting, QVariant defaultValue)
{
    return settings->value(QString(name+"_"+setting).replace(' ', ""), defaultValue);
}

void Line::setFlag(int flag, bool value)
{
    switch (flag) {
    case 0:
        ui->flag0->setChecked(value);
        break;
    case 1:
        ui->flag1->setChecked(value);
        break;
    case 2:
        ui->flag2->setChecked(value);
        break;
    case 3:
        ui->flag3->setChecked(value);
        break;
    default:
        break;
    }
}

bool Line::getFlag(int flag)
{
    switch (flag) {
    case 0:
        return ui->flag0->checkState();
    case 1:
        return ui->flag1->checkState();
    case 2:
        return ui->flag2->checkState();
    case 3:
        return ui->flag3->checkState();
    default:
        return false;
    }
}

bool Line::showCounts()
{
    return ui->Counts->isChecked();
}

bool Line::showAutocorrelations()
{
    return ui->Autocorrelations->isChecked();
}

bool Line::showCrosscorrelations()
{
    return ui->Crosscorrelations->isChecked();
}

void Line::setMode(Mode m)
{
    mode = m;
    series.clear();
    average.clear();
    counts.clear();
    autocorrelations.clear();
    crosscorrelations.clear();
    if(mode == Autocorrelator  || mode == Crosscorrelator) {
        stack = 0.0;
    }
    if(!isActive()) {
        ui->Counter->setEnabled(mode == Counter);
        ui->Autocorrelator->setEnabled(mode == Autocorrelator);
        ui->Crosscorrelator->setEnabled(mode == Crosscorrelator);
    }
}

void Line::setPercent()
{
    stop = !isActive();
    if(ui->Progress != nullptr) {
        if(mode == Autocorrelator) {
            if(percent>ui->Progress->minimum() && percent<ui->Progress->maximum())
                ui->Progress->setValue(percent);
        } else if (mode == Crosscorrelator) {
            if(percent>ui->Progress2->minimum() && percent<ui->Progress2->maximum())
                ui->Progress2->setValue(percent);
        } else {
            ui->Progress->setValue(0);
            ui->Progress2->setValue(0);
        }
    }
}

void Line::stackCorrelations()
{
    scanning = true;
    ahp_xc_sample *spectrum = nullptr;
    int start = ui->StartLine->value();
    int end = ui->EndLine->value();
    int len = end-start;
    if(!isActive())
        return;
    stop = 0;
    int nread = ahp_xc_scan_autocorrelations(line, &spectrum, start, len, &stop, &percent);
    if(nread != len)
        return;
    if(spectrum != nullptr) {
        double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/(((ahp_xc_get_test(line)&TEST_SIGNAL)?AHP_XC_PLL_FREQUENCY:0)+ahp_xc_get_frequency());
        double value;
        stack += 1.0;
        series.clear();
        for (int x = start, z = 0; x < end && z < len; x++, z++) {
            double y = (double)x*timespan;
            value = spectrum[z].correlations[0].coherence / stack;
            value += average.value(y, 0)*(stack-1)/stack;
            series.append(y, value - dark.value(y, 0));
            if(average.count() > z)
                if(average.contains(y))
                   average.remove(y);
            average.insert(y, value);
        }
        emit activeStateChanged(this);
    }
    scanning = false;
}

void Line::stackCorrelations(unsigned int line2)
{
    scanning = true;
    ahp_xc_sample *spectrum = nullptr;
    stop = 0;
    off_t start1, start2;
    int size = (ui->CrossEnd->value() - ui->CrossStart->value());
    start1 = ui->CrossStart->value() + size;
    start2 = ui->CrossEnd->value() - size;
    start1 = start1 > 0 ? 0 : -start1;
    start2 = start2 < 0 ? 0 : start2;
    int nread = ahp_xc_scan_crosscorrelations(line, line2, &spectrum, start1, start2, (size_t)size, &stop, &percent);
    if(nread != size)
        return;
    if(spectrum != nullptr) {
        int size = (ui->CrossEnd->value() - ui->CrossStart->value());
        double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/(((ahp_xc_get_test(line)&TEST_SIGNAL)?AHP_XC_PLL_FREQUENCY:0)+ahp_xc_get_frequency());
        double value;
        stack += 1.0;
        series.clear();
        for (long x = start2, z = 1; x > start1 && z < size-1; x--, z++) {
            double y = (double)x*timespan;
            value = spectrum[z].correlations[ahp_xc_get_crosscorrelator_lagsize()-1].coherence/stack;
            value += average.value(y, 0)*(stack-1)/stack;
            series.append(y, value - crossdark.value(y, 0));
            if(average.contains(y))
                average.remove(y);
            average.insert(y, value);
        }
        emit activeStateChanged(this);
    }
    scanning = false;
}

Line::~Line()
{
    setActive(false);
    delete ui;
}
