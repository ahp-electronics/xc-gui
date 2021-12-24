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

#ifndef BASELINE_H
#define BASELINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QSettings>
#include <QLineSeries>
#include <QSplineSeries>
#include <ahp_xc.h>
#include <cmath>
#include "types.h"
#include "threads.h"

using namespace QtCharts;
class Line;
class Baseline : public QWidget
{
        Q_OBJECT

    public:
        Baseline(QString name, int index, Line* n1, Line* n2, QSettings *settings = nullptr, QWidget *parent = nullptr);
        ~Baseline();

        inline void setName(QString n)
        {
            name = n;
            series->setName(n);
        }
        inline QString getName()
        {
            return name;
        }
        inline QList<double>* getDark()
        {
            return &dark;
        }
        inline QLineSeries* getDots()
        {
            return series;
        }
        inline QLineSeries* getAverage()
        {
            return average;
        }
        inline QList<double> *getMagnitude()
        {
            return &magnitude;
        }
        inline QList<double> *getPhase()
        {
            return &phase;
        }
        void setMode(Mode m);
        inline void setDelay(double s);
        inline bool isActive()
        {
            return (active == 3);
        }
        inline double getPercent()
        {
            return percent;
        }
        inline double isScanning()
        {
            return scanning;
        }
        inline Line* getLine1()
        {
            return line1;
        }
        inline Line* getLine2()
        {
            return line2;
        }

    private:
        int Index;
        QSettings *settings;
        QString name;
        int active;
        bool scanning;
        bool threadRunning;
        int stop;
        double percent;
        Mode mode;
        QList<double> dark;
        QList<double> magnitude;
        QList<double> phase;
        QLineSeries *series;
        QLineSeries *average;
        Line* line1;
        Line* line2;
};

#endif // BASELINE_H
