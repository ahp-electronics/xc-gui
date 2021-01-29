#include "baseline.h"

Baseline::Baseline(QString n, int n1, int n2, QWidget *parent) :
    QWidget(parent)
{
    name = n;
    runThread = std::thread(Baseline::RunThread, this);
    runThread.detach();
    series = new QLineSeries();
    series->setName(name);
    average = new QLineSeries();
    average->setName(name);
    line1 = n1;
    line2 = n2;
}

void Baseline::setMode(Mode m)
{
    mode = m;
    series->clear();
    average->clear();
    if(mode == Crosscorrelator) {
        stack = 0.0;
    }
}

void Baseline::RunThread(Baseline *line)
{
    line->threadRunning = true;
    while (line->threadRunning) {
        QThread::sleep(100);
        if(line->isActive()) {
            line->stop = 0;
        } else {
            line->stop = 1;
        }
    }
}

void Baseline::stackCorrelations(Scale scale)
{
    scanning = true;
    ahp_xc_sample *spectrum;
    stop = 0;
    ahp_xc_scan_crosscorrelations(line1, line2, &spectrum, &stop, &percent);
    if(spectrum != nullptr) {
        double timespan = pow(2, ahp_xc_get_frequency_divider())*1000000000.0/ahp_xc_get_frequency();
        double value;
        values.clear();
        stack += 1.0;
        for (int x = 0; x < ahp_xc_get_delaysize()*2-1; x++) {
            if(spectrum[x].correlations[0].counts >= spectrum[x].correlations[0].correlations && spectrum[x].correlations[0].counts > 0)
                value = spectrum[x].correlations[0].coherence;
            switch(scale) {
            case 1:
                value = sqrt(value);
                break;
            case 2:
                value = pow(value, 0.1);
                break;
            default:
                break;
            }
            value /= stack;
            if(average->count() > x)
                value += average->at(x).y()*(stack-1)/stack;
            values.append(value);
        }
        series->clear();
        average->clear();
        for (int x = 0; x < ahp_xc_get_delaysize()*2-1; x++) {
            if(dark.count() > x)
                series->append((x-ahp_xc_get_delaysize())*timespan, values.at(x) - dark.at(x));
            else
                series->append((x-ahp_xc_get_delaysize())*timespan, values.at(x));
            average->append((x-ahp_xc_get_delaysize())*timespan, values.at(x));
        }
    };
    scanning = false;
}

Baseline::~Baseline()
{
    threadRunning = false;
}
