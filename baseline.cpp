#include "baseline.h"
#include "line.h"
Baseline::Baseline(QString n, Line *n1, Line *n2, QSettings *s, QWidget *parent) :
    QWidget(parent)
{
    setAccessibleName("Baseline");
    settings = s;
    name = n;
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

void Baseline::stackCorrelations(Scale scale)
{
    scanning = true;
    ahp_xc_sample *spectrum;
    stop = 0;
    ahp_xc_scan_crosscorrelations(line1->getLineIndex(), line2->getLineIndex(), &spectrum, &stop, &percent);
    line1->setPercentAddr(&percent);
    line2->setPercentAddr(&percent);
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
    setActive(false);
    threadRunning = false;
}
