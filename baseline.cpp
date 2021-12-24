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
    average = new QLineSeries();
    average->setName(name);
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
    ahp_xc_set_channel_cross((uint32_t)Index, (off_t)s * (ahp_xc_get_frequency() >> ahp_xc_get_frequency_divider()));
}

void Baseline::setMode(Mode m)
{
    mode = m;
    series->clear();
    average->clear();
}

Baseline::~Baseline()
{
    threadRunning = false;
}
