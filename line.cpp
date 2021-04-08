#include "line.h"
#include "ui_line.h"
#include <QMapNode>

Line::Line(QString ln, int n, QSettings *s, QWidget *parent, QList<Line*> *p) :
    QWidget(parent),
    ui(new Ui::Line)
{
    setAccessibleName("Line");
    setActive(false);
    settings = s;
    percent = new double(0);
    parents = p;
    name = ln;
    series = new QLineSeries();
    series->setName(name);
    average = new QLineSeries();
    average->setName(name);
    line = n;
    flags = 0;
    ui->setupUi(this);
    connect(ui->flag0, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 0);
        flags |= ui->flag0->isChecked() << 0;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag0->text(), ui->flag0->isChecked());
    });
    connect(ui->flag1, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 1);
        flags |= ui->flag0->isChecked() << 1;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag1->text(), ui->flag1->isChecked());
    });
    connect(ui->flag2, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 2);
        flags |= ui->flag0->isChecked() << 2;
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
        if(mode == Autocorrelator||mode == Crosscorrelator)
            ui->Correlator->setEnabled(!isActive());
        else
            ui->Correlator->setEnabled(false);
        for(int x = 0; x < nodes.count(); x++)
            nodes[x]->setActive(nodes[x]->getLine1()->isActive()&&nodes[x]->getLine2()->isActive()&&mode==Crosscorrelator);

    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        if(ui->TakeDark->text() == "Apply Dark") {
            if(mode == Autocorrelator) {
                dark.clear();
                for(int x = ui->StartLine->value(), y = 0; x < ui->EndLine->value(); y++, x++) {
                    dark.insert(x, series->at(y).y());
                    saveSetting("Dark"+QString::number(x), series->at(y).y());
                }
                ui->TakeDark->setText("Clear Dark");
                series->setName("(residuals)");
            } else {
                for(int x = 0; x < nodes.count(); x++)
                {
                    for(int x = 0; x < nodes[x]->getDots()->count(); x++) {
                        nodes[x]->getDark()->clear();
                        nodes[x]->getDots()->setName(nodes[x]->getName()+" (residuals)");
                        nodes[x]->getDark()->append(nodes[x]->getDots()->at(x).y());
                    }
                    ui->TakeDark->setText("Clear Dark");
                }
            }
        } else {
            if(mode == Autocorrelator) {
                int x = 0;
                while(x < ui->EndLine->maximum()) {
                    removeSetting("Dark"+QString::number(x++));
                }
                dark.clear();
                ui->TakeDark->setText("Apply Dark");
                series->setName(name);
            } else {
                for(int x = 0; x < nodes[x]->getDots()->count(); x++) {
                    nodes[x]->getDark()->clear();
                    nodes[x]->getDots()->setName(nodes[x]->getName());
                }
            }
        }
    });
    connect(ui->StartLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()) {
            ui->EndLine->setValue(ui->StartLine->value()+1);
        }
        saveSetting("StartLine", ui->StartLine->value());
    });
    connect(ui->EndLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()) {
            ui->StartLine->setValue(ui->EndLine->value()-1);
        }
        saveSetting("EndLine", ui->EndLine->value());
    });
    connect(ui->Clear, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
         clearCorrelations();
         series->clear();
         average->clear();
         for(int x = 0; x < nodes.count(); x++)
         {
             nodes[x]->getDots()->clear();
             nodes[x]->getAverage()->clear();
         }
    });
    ui->StartLine->setRange(0, ahp_xc_get_delaysize()-2);
    ui->EndLine->setRange(1, ahp_xc_get_delaysize()-1);
    setFlag(0, readBool(ui->flag0->text(), false));
    setFlag(1, readBool(ui->flag1->text(), false));
    setFlag(2, readBool(ui->flag2->text(), false));
    setFlag(3, readBool(ui->flag3->text(), false));
    ui->test->setChecked(readBool(ui->test->text(), false));
    ui->StartLine->setValue(readInt("StartLine", 0));
    ui->EndLine->setValue(readInt("EndLine", ui->EndLine->maximum()));
    dark.clear();
    int x = 0;
    while(haveSetting("Dark"+QString::number(x++))) {
        dark.insert(x, readSetting("Dark"+QString::number(x), 0.0).toDouble());
    }
    if(dark.count() > 0) {
        ui->TakeDark->setText("Clear Dark");
    }
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

void Line::setMode(Mode m)
{
    mode = m;
    series->clear();
    average->clear();
    if(mode == Autocorrelator) {
        stack = 0.0;
    }
    if(!isActive())
        ui->Correlator->setEnabled(mode == Autocorrelator||mode == Crosscorrelator);
}

Scale Line::getYScale()
{
    return (Scale)ui->YScale->currentIndex();
}

void Line::setPercent()
{
    stop = !isActive();
    if(ui->Progress != nullptr) {
        if((*percent)<ui->Progress->maximum())
        ui->Progress->setValue(*percent);
        update();
    }
}

void Line::stackCorrelations()
{
    ahp_xc_sample *spectrum;
    int start = ui->StartLine->value();
    int end = ui->EndLine->value();
    int len = end-start;
    if(!isActive())
        return;
    stop = 0;
    int nread = ahp_xc_scan_autocorrelations(line, &spectrum, start, len, &stop, percent);
    if(nread != len)
        return;
    if(spectrum != nullptr) {
        double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/ahp_xc_get_frequency();
        double value;
        values.clear();
        stack += 1.0;
        for (int x = start; x < end; x++) {
            if(spectrum[x].correlations[0].counts >= spectrum[x].correlations[0].correlations && spectrum[x].correlations[0].counts > 0)
                value = spectrum[x].correlations[0].coherence;
            switch(ui->YScale->currentIndex()) {
            case 1:
                value = sqrt(value);
                break;
            case 2:
                value = log10(value);
                break;
            default:
                break;
            }
            value /= stack;
            if(average->count() > x)
                value += average->at(x).y()*(stack-1)/stack;
            values.insert(x, value);
        }
        series->clear();
        average->clear();
        for (int x = start; x < end; x++) {
            series->append(x*timespan, values.at(x-start) - dark.value(x, 0));
            average->append(x*timespan, values.at(x-start));
        }
    };
}

Line::~Line()
{
    setActive(false);
    delete series;
    delete average;
    delete ui;
}
