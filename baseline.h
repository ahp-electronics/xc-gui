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
#include <QMapNode>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <cmath>
#include "types.h"
#include "elemental.h"

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
        }
        inline QString getName()
        {
            return name;
        }
        inline QLineSeries* getMagnitudes()
        {
            return magnitudes;
        }
        inline QLineSeries* getPhases()
        {
            return phases;
        }
        inline void setMagnitudeSize(size_t size)
        {
            if(magnitude_buf != nullptr)
                magnitude_buf = (double*)realloc(magnitude_buf, sizeof(double) * (size+1));
            else
                magnitude_buf = (double*)malloc(sizeof(double) * (size+1));
        }
        inline void setPhaseSize(size_t size)
        {
            if(phase_buf != nullptr)
                phase_buf = (double*)realloc(phase_buf, sizeof(double) * (size+1));
            else
                phase_buf = (double*)malloc(sizeof(double) * (size+1));
        }
        inline void setDftSize(size_t size)
        {
            if(dft != nullptr)
                dft = (fftw_complex*)realloc(dft, sizeof(fftw_complex) * (size+1));
            else
                dft = (fftw_complex*)malloc(sizeof(fftw_complex) * (size+1));
        }
        inline QLineSeries* getMagnitude()
        {
            return magnitude;
        }
        inline QLineSeries* getPhase()
        {
            return phase;
        }
        inline QList<double>* getCounts()
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
        inline QMap<double, double>* getDark()
        {
            return dark;
        }
        void setMode(Mode m);
        inline Mode getMode() { return mode; }
        inline void setDelay(double s);
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

        bool isActive();

        void stackCorrelations();
        void plot(bool success, double o, double s);
        void SavePlot();
        void TakeDark(Line* sender);
        bool haveSetting(QString setting);
        void removeSetting(QString setting);
        void saveSetting(QString name, QVariant value);
        QVariant readSetting(QString name, QVariant defaultValue);
        inline QString readString(QString setting, QString defaultValue)
        {
            return readSetting(setting, defaultValue).toString();
        }
        inline double readDouble(QString setting, double defaultValue)
        {
            return readSetting(setting, defaultValue).toDouble();
        }
        inline int readInt(QString setting, int defaultValue)
        {
            return readSetting(setting, defaultValue).toInt();
        }
        inline bool readBool(QString setting, bool defaultValue)
        {
            return readSetting(setting, defaultValue).toBool();
        }

        void addToVLBIContext(int index = -1);
        void removeFromVLBIContext(int index = -1);
        inline dsp_stream_p getStream()
        {
            return stream;
        }

        inline void setStream(dsp_stream_p s)
        {
            stream = s;
        }

        inline void* getVLBIContext(int index = 0)
        {
            return context[index];
        }

        inline void setVLBIContext(void* ctx, int index = 0)
        {
            context[index] = ctx;
        }

    private:
        bool running { false };
        void updateBufferSizes();
        void stretch(QLineSeries* series);
        void stackValue(QLineSeries* series, QMap<double, double>* stacked, int index, double x, double y);

        dsp_stream_p stream;
        double stack {0.0};
        fftw_plan plan;
        fftw_complex *dft { nullptr };
        double *magnitude_buf { nullptr };
        double *phase_buf { nullptr };
        double offset { 0.0 };
        double timespan { 1.0 };
        int Index;
        int start1 {0};
        int start2 {0};
        int end1 {0};
        int end2 {0};
        int len {1};
        QSettings *settings;
        QString name;
        bool scanning;
        bool threadRunning;
        bool oldstate;
        int stop;
        double percent;
        Mode mode;
        void* context[vlbi_total_contexts];
        Elemental *elemental;
        QMap<double, double>* dark;
        QMap<double, double>* magnitudeStack;
        QMap<double, double>* phaseStack;
        QLineSeries* magnitude;
        QLineSeries* phase;
        QLineSeries* magnitudes;
        QLineSeries* phases;
        QList<double>* complex;
        Line* line1;
        Line* line2;
};

#endif // BASELINE_H
