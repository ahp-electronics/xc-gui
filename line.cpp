#include "line.h"
#include "ui_line.h"
#include <fftw3.h>
#include <QMapNode>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>

Line::Line(QString ln, int n, QSettings *s, QWidget *parent, QList<Line*> *p) :
    QWidget(parent),
    ui(new Ui::Line)
{
    setAccessibleName("Line");
    settings = s;
    percent = 0;
    parents = p;
    name = ln;
    average = new QMap<double, double>();
    dark = new QMap<double, double>();
    autodark = new QMap<double, double>();
    crossdark = new QMap<double, double>();
    series = new QLineSeries();
    counts = new QLineSeries();
    autocorrelations = new QLineSeries();
    getDots()->setName(name+" coherence");
    getCounts()->setName(name+" (counts)");
    getAutocorrelations()->setName(name+" (autocorrelations)");
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    line = n;
    flags = 0;
    ui->setupUi(this);
    ui->SpectralLine->setRange(0, ahp_xc_get_delaysize()-7);
    ui->StartLine->setRange(0, ahp_xc_get_delaysize()-7);
    ui->EndLine->setRange(5, ahp_xc_get_delaysize()-1);
    ui->x_location->setRange(-ahp_xc_get_delaysize()*1000, ahp_xc_get_delaysize()*1000);
    ui->y_location->setRange(-ahp_xc_get_delaysize()*1000, ahp_xc_get_delaysize()*1000);
    ui->z_location->setRange(-ahp_xc_get_delaysize()*1000, ahp_xc_get_delaysize()*1000);
    ui->StartLine->setValue(readInt("StartLine", ui->StartLine->minimum()));
    ui->EndLine->setValue(readInt("EndLine", ui->EndLine->maximum()));
    ui->SpectralLine->setValue(readInt("SpectralLine", ui->SpectralLine->minimum()));
    ui->IDFT->setChecked(readBool("IDFT", false));
    ui->x_location->setValue(readDouble("x_location", 0)/1000.0);
    ui->y_location->setValue(readDouble("y_location", 0)/1000.0);
    ui->z_location->setValue(readDouble("z_location", 0)/1000.0);
    int start = ui->StartLine->value();
    int end = ui->EndLine->value();
    int len = end-start;
    ac = (double*)malloc(sizeof(double)*len);
    dft = (fftw_complex*)malloc(sizeof(fftw_complex)*len);
    plan = fftw_plan_dft_c2r_1d(len, dft, ac, FFTW_ESTIMATE);
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
                for(int x = 0, y = 0; x < getDots()->count(); y++, x++) {
                    output << "'"+QString::number(getDots()->at(y).x())+"';'"+QString::number(getDots()->at(y).y())+"'\n";
                }
            }
        }
        data.close();
    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        if(ui->TakeDark->text() == "Apply Dark") {
            getDark()->clear();
            for(int x = 0; x < getDots()->count(); x++) {
                getDark()->insert(getDots()->at(x).x(), getDots()->at(x).y());
                QString darkstring = readString("Dark", "");
                if(!darkstring.isEmpty())
                    saveSetting("Dark", darkstring+";");
                darkstring = readString("Dark", "");
                saveSetting("Dark", darkstring+QString::number(getDots()->at(x).x())+","+QString::number(getDots()->at(x).y()));
            }
            ui->TakeDark->setText("Clear Dark");
            getDots()->setName(name+" coherence (residuals)");
        } else {
            removeSetting("Dark");
            getDark()->clear();
            ui->TakeDark->setText("Apply Dark");
            getDots()->setName(name+" coherence");
        }
    });
    connect(ui->IDFT, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        saveSetting("IDFT", ui->IDFT->isChecked());
    });
    connect(ui->StartLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()-5) {
            ui->EndLine->setValue(ui->StartLine->value()+1);
        }
        int start = ui->StartLine->value();
        int end = ui->EndLine->value();
        int len = end-start;
        ac = (double*)realloc(ac, sizeof(double)*len);
        dft = (fftw_complex*)realloc(dft, sizeof(fftw_complex)*len);
        fftw_destroy_plan(plan);
        plan = fftw_plan_dft_c2r_1d(len, dft, ac, FFTW_ESTIMATE);
        saveSetting("StartLine", ui->StartLine->value());
    });
    connect(ui->EndLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()-5) {
            ui->StartLine->setValue(ui->EndLine->value()-1);
        }
        int start = ui->StartLine->value();
        int end = ui->EndLine->value();
        int len = end-start;
        ac = (double*)realloc(ac, sizeof(double)*len);
        dft = (fftw_complex*)realloc(dft, sizeof(fftw_complex)*len);
        fftw_destroy_plan(plan);
        plan = fftw_plan_dft_c2r_1d(len, dft, ac, FFTW_ESTIMATE);
        saveSetting("EndLine", ui->EndLine->value());
    });
    connect(ui->Clear, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
         clearCorrelations();
         getDots()->clear();
         getAverage()->clear();
         emit activeStateChanged(this);
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
    connect(ui->LineDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        ahp_xc_set_lag_cross(line, value);
        saveSetting("SpectralLine", value);
    });
    connect(ui->x_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        getLocation()->xyz.x = (double)value/1000.0;
        saveSetting("x_location", value);
    });
    connect(ui->y_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        getLocation()->xyz.y = (double)value/1000.0;
        saveSetting("y_location", value);
    });
    connect(ui->z_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        getLocation()->xyz.z = (double)value/1000.0;
        saveSetting("z_location", value);
    });
    setFlag(0, readBool(ui->flag0->text(), false));
    setFlag(1, readBool(ui->flag1->text(), false));
    setFlag(2, readBool(ui->flag2->text(), false));
    setFlag(3, readBool(ui->flag3->text(), false));
    ui->test->setChecked(readBool(ui->test->text(), false));
    ui->Counts->setChecked(readBool(ui->Counts->text(), false));
    ui->Autocorrelations->setChecked(readBool(ui->Autocorrelations->text(), false));
    if(haveSetting("Dark")) {
        QStringList darkstring = readString("Dark", "").split(";");
        for(int x = 0; x < darkstring.length(); x++) {
            QStringList lag_value = darkstring[x].split(",");
            if(lag_value.length()==2)
                getDark()->insert(lag_value[0].toDouble(), lag_value[1].toDouble());
        }
        if(getDark()->count() > 0) {
            ui->TakeDark->setText("Clear Dark");
            getDots()->setName(name+" coherence (residuals)");
        }
    }
    getDark()->clear();
    setActive(false);
    setMode(Counter);
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
    settings->sync();
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

void Line::setMode(Mode m)
{
    mode = m;
    getDark()->clear();
    getDots()->clear();
    getAverage()->clear();
    getCounts()->clear();
    getAutocorrelations()->clear();
    if(mode == Autocorrelator) {
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
        }
    }
}

void Line::insertValue(double x, double y)
{
    y += getAverage()->value(x, 0)*(stack-1)/stack;
    getAverage()->insert(x, y);
    getDots()->append(x, y - getDark()->value(x, 0));
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
        getDots()->clear();
        if(ui->IDFT->isChecked()) {
            for (int z = 0; z < len; z++) {
                dft[z][0] = spectrum[z].correlations[0].coherence;
                dft[z][1] = 0;
            }
            fftw_execute(plan);
        }
        for (int x = start+1, z = 1; x < end && z < len; x++, z++) {
            double y;
            y = (double)x*timespan;
            if(ui->IDFT->isChecked()) {
                value = ac[z] / stack;
            } else {
                value = spectrum[z].correlations[0].coherence / stack;
            }
            insertValue(y, value);
        }
        emit activeStateChanged(this);
    }
    scanning = false;
}

Line::~Line()
{
    fftw_destroy_plan(plan);
    setActive(false);
    delete ui;
}
