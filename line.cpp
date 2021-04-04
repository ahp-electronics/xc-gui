#include "line.h"
#include "ui_line.h"

Line::Line(QString name, int n, QWidget *parent, QList<Line*> *p) :
    QWidget(parent),
    ui(new Ui::Line)
{
    percent = new double(0);
    parents = p;
    runThread = std::thread(Line::RunThread, this);
    runThread.detach();
    series = new QLineSeries();
    series->setName(name);
    average = new QLineSeries();
    average->setName(name);
    line = n;
    flags = 0;
    ui->setupUi(this);
    ui->StartLine->setRange(0, ahp_xc_get_delaysize()-2);
    ui->EndLine->setRange(1, ahp_xc_get_delaysize()-1);
    ui->EndLine->setValue(ui->EndLine->maximum());
    connect(ui->flag0, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 0);
        flags |= ((state >> 1) & 0x1) << 0;
        ahp_xc_set_leds(line, flags);
        ui->Correlator->setEnabled(state==0&&(mode == Autocorrelator||mode == Crosscorrelator));
        for(int x = 0; x < nodes.count(); x++)
            nodes[x]->setActive(nodes[x]->getLine1()->isActive()&&nodes[x]->getLine2()->isActive()&&mode==Crosscorrelator);
    });
    connect(ui->flag1, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 1);
        flags |= ((state >> 1) & 0x1) << 1;
        ahp_xc_set_leds(line, flags);
    });
    connect(ui->flag2, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 2);
        flags |= ((state >> 1) & 0x1) << 2;
        ahp_xc_set_leds(line, flags);
    });
    connect(ui->flag3, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 3);
        flags |= ((state >> 1) & 0x1) << 3;
        ahp_xc_set_leds(line, flags);
    });
    connect(ui->test, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        (state == 2) ? ahp_xc_set_test(line, TEST_SIGNAL) : ahp_xc_clear_test(line, TEST_SIGNAL);
    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [=](bool checked) {
        if(ui->TakeDark->text() == "Apply Dark") {
            if(mode == Autocorrelator) {
                dark.clear();
                for(int x = 0; x < series->count(); x++)
                    dark.append(series->at(x).y());
                ui->TakeDark->setText("Clear Dark");
                series->setName(name+" (residuals)");
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
    });
    connect(ui->EndLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        if(ui->StartLine->value()>=ui->EndLine->value()) {
            ui->StartLine->setValue(ui->EndLine->value()-1);
        }
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
}

void Line::setActive(bool a)
{
    setFlag(0, false);
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
    ui->Correlator->setEnabled((mode == Autocorrelator||mode == Crosscorrelator)&&!isActive());
}

void Line::RunThread(Line *wnd)
{
    wnd->threadRunning = true;
    while (wnd->threadRunning) {
        QThread::msleep(100);
        if(wnd->isActive()) {
            wnd->stop = 0;
            wnd->setPercent();
        } else {
            wnd->stop = 1;
        }
    }
}

Scale Line::getYScale()
{
    return (Scale)ui->YScale->currentIndex();
}

void Line::setPercent()
{
    if(ui->Progress != nullptr) {
        ui->Progress->setValue(*percent);
        update();
    }
}

void Line::stackCorrelations()
{
    scanning = true;
    ahp_xc_sample *spectrum;
    int len = ui->EndLine->value()-ui->StartLine->value();
    stop = !isActive();
    if(ahp_xc_scan_autocorrelations(line, &spectrum, ui->StartLine->value(), ui->EndLine->value(), &stop, percent) != len)
        return;
    if(spectrum != nullptr) {
        double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/ahp_xc_get_frequency();
        double value;
        values.clear();
        int start = ui->StartLine->value();
        int end = ui->EndLine->value();
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
            if(average->count() > x-start)
                value += average->at(x-start).y()*(stack-1)/stack;
            values.append(value);
        }
        series->clear();
        average->clear();
        for (int x = start; x < end; x++) {
            if(dark.count() > x-start)
                series->append(x*timespan, values.at(x-start) - dark.at(x-start));
            else
                series->append(x*timespan, values.at(x-start));
            average->append(x*timespan, values.at(x-start));
        }
    };
    scanning = false;
}

Line::~Line()
{
    setActive(false);
    threadRunning = false;
    QThread::msleep(200);
    delete series;
    delete average;
    delete ui;
}
