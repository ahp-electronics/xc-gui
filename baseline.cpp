/*
    MIT License

    libahp_xc library to drive the AHP XC correlators
    Copyright (C) 2020  Ilia Platone

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "baseline.h"
#include "line.h"
Baseline::Baseline(QString n, int index, Line *n1, Line *n2, QSettings *s, QWidget *parent) :
    QWidget(parent)
{
    setAccessibleName("Baseline");
    settings = s;
    name = n;
    Index = index;
    series = new QLineSeries();
    series->setName(name);
    average = new QMap<double, double>();
    line1 = n1;
    line2 = n2;
    connect(line1, static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged), this,
            [ = ](Line * sender)
    {
        active &= ~1;
        active |= sender->isActive();
        stop = !isActive();
    });
    connect(line2, static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged), this,
            [ = ](Line * sender)
    {
        active &= ~2;
        active |= sender->isActive() << 1;
        stop = !isActive();
    });

}

void Baseline::setDelay(double s)
{
    ahp_xc_set_channel_cross((uint32_t)Index, (off_t)s * (ahp_xc_get_frequency() >> ahp_xc_get_frequency_divider()), 0);
}

void Baseline::setMode(Mode m)
{
    mode = m;
    series->clear();
    average->clear();
}

void Baseline::stackValue(QLineSeries* series, QMap<double, double>* stacked, int idx, double x, double y)
{
    y /= stack;
    if(stacked->count() > idx) {
        y += stacked->values().at(idx) * (stack - 1) / stack;
        stacked->keys().replace(idx, x);
        stacked->values().replace(idx, x);
    } else {
        stacked->insert(x, y);
    }
    series->append(x, y);
}

void Baseline::stretch(QLineSeries* series)
{
    double mx = DBL_MIN;
    double mn = DBL_MAX;
    for (int x = 0; x < series->count(); x++) {
        mn = fmin(series->at(x).y(), mn);
        mx = fmax(series->at(x).y(), mx);
    }
    for (int x = 0; x < series->count(); x++) {
        QPointF p = series->at(x);
        series->replace(x, p.x(), (p.y()-mn) * M_PI * 2.0/(mx-mn));
    }
}

void Baseline::stackCorrelations()
{
    scanning = true;
    ahp_xc_sample *spectrum = nullptr;
    int start = getLine1()->getOffset();
    int end = getLine2()->getOffset();
    int len = end - start;
    stop = 0;
    int npackets = ahp_xc_scan_crosscorrelations(getLine1()->getLineIndex(), getLine2()->getLineIndex(), &spectrum, start1, start2, start2-start1, &stop, &percent);
    if(spectrum != nullptr && npackets == len) {
        stack += 1.0;
        getMagnitude()->clear();
        getPhase()->clear();
        if(false)
        {
            for (int x = 0; x < npackets; x++)
            {
                if(spectrum[x].correlations[0].magnitude > 0) {
                    dft[x][0] = spectrum[x].correlations[0].real;
                    dft[x][1] = spectrum[x].correlations[0].imaginary;
                }
            }
            fftw_execute(plan);
            for (int x = 0; x < npackets; x++)
            {
                double y = x * timespan + offset;
                stackValue(getMagnitude(), getMagnitudeStack(), x, y, correlations[x]);
            };
            stretch(getMagnitude());
        } else {
            npackets--;
            buf = (double*)realloc(buf, sizeof(double)*npackets);
            if(mode==Spectrograph) {
                for(int x = 0; x < npackets; x++)
                    buf[x] = (double)spectrum[x+1].correlations[0].magnitude;
            } else  {
                for(int x = 0; x < npackets; x++) {
                    double m = fmax(spectrum[x+1].correlations[0].real, spectrum[x+1].correlations[0].imaginary) / 2.0;
                    spectrum[x+1].correlations[0].real = spectrum[x+1].correlations[0].real - m;
                    spectrum[x+1].correlations[0].imaginary = spectrum[x+1].correlations[0].imaginary - m;
                    spectrum[x+1].correlations[0].magnitude = sqrt(pow(spectrum[x+1].correlations[0].real, 2.0)+pow(spectrum[x+1].correlations[0].imaginary, 2.0));
                    buf[x] = (double)spectrum[x+1].correlations[0].magnitude/pow((double)spectrum[x+1].correlations[0].real+(double)spectrum[x+1].correlations[0].imaginary, 2);
                }
            }
        }
        free(spectrum);
    }
    scanning = false;
}

Baseline::~Baseline()
{
    threadRunning = false;
}
