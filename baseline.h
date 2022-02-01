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
#include <QList>
#include <QWidget>
#include <QSettings>
#include <QLineSeries>
#include <QSplineSeries>
#include <ahp_xc.h>
#include <fftw3.h>
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
        inline QLineSeries* getDots()
        {
            return series;
        }
        inline QLineSeries* getMagnitudes()
        {
            return magnitudes;
        }
        inline QLineSeries* getPhases()
        {
            return phases;
        }
        inline QLineSeries* getMagnitude()
        {
            return magnitude;
        }
        inline QLineSeries* getPhase()
        {
            return phase;
        }
        inline QList<double>* getBuffer()
        {
            return complex;
        }
        inline QMap<double, double>* getMagnitudeStack()
        {
            return magnitudeStack;
        }
        inline QMap<double, double>* getPhaseStack()
        {
            return phaseStack;
        }
        inline QMap<double, double>* getAverage()
        {
            return average;
        }
        inline QMap<double, double>* getDark()
        {
            return mode == Crosscorrelator ? (crossdark) : (mode == Autocorrelator ? (autodark) : dark);
        }
        inline QLineSeries* getCounts()
        {
            return counts;
        }
        inline QList<double*> getCrosscorrelations()
        {
            return crosscorrelations;
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

        void stackCorrelations();

    private:
        fftw_plan plan;
        fftw_complex *dft;
        void stretch(QLineSeries* series);
        void stackValue(QLineSeries* series, QMap<double, double>* stacked, int index, double x, double y);

        double *buf;
        double offset { 0.0 };
        double timespan { 1.0 };
        double stack {0.0};
        int Index;
        int start1 {0};
        int start2 {0};
        QSettings *settings;
        QString name;
        int active;
        bool scanning;
        bool threadRunning;
        int stop;
        double percent;
        double *correlations;
        Mode mode;
        QMap<double, double>* dark;
        QMap<double, double>* autodark;
        QMap<double, double>* crossdark;
        QMap<double, double>* average;
        QMap<double, double>* magnitudeStack;
        QMap<double, double>* phaseStack;
        QLineSeries* series;
        QLineSeries* magnitude;
        QLineSeries* phase;
        QLineSeries* counts;
        QLineSeries* magnitudes;
        QLineSeries* phases;
        QList<double>* complex;
        QList<double*> crosscorrelations;
        Line* line1;
        Line* line2;
};

#endif // BASELINE_H
