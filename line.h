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

#ifndef LINE_H
#define LINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QSettings>
#include <QScatterSeries>
#include <QSplineSeries>
#include <QLineSeries>
#include <ahp_xc.h>
#include <complex.h>
#include <fftw3.h>
#include <cmath>
#include <vlbi.h>
#include <ahp_gt.h>
#include "types.h"
#include "threads.h"
#include "baseline.h"

using namespace QtCharts;

namespace Ui
{
class Line;
}

class Line : public QWidget
{
        Q_OBJECT

    public:
        explicit Line(QString name, int n, QSettings *settings = nullptr, QWidget *parent = nullptr, QList<Line*> *p = nullptr);
        ~Line();

        void paint();

        inline bool isActive()
        {
            return running;
        }
        void setActive(bool a)
        {
            running = a;
            if(!running)
            {
                getCounts()->clear();
                getAutocorrelations()->clear();
            }
            emit activeStateChanged(this);
        }
        inline int getFlags()
        {
            return flags;
        }
        void setFlag(int flag, bool value);
        bool getFlag(int flag);
        bool showCounts();
        bool showAutocorrelations();
        bool showCrosscorrelations();
        inline QString getName()
        {
            return name;
        }

        void setMode(Mode m);
        Scale getYScale();
        void stackCorrelations();
        inline bool applyMedian()
        {
            return applymedian;
        }
        inline bool applySigmaClipping()
        {
            return applysigmaclipping;
        }
        inline void clearCorrelations()
        {
            stack = 0.0;
        }
        inline double getPercent()
        {
            return percent;
        }
        inline double isScanning()
        {
            return !stop;
        }
        inline void addBaseline(Baseline* b)
        {
            nodes.append(b);
        }
        inline unsigned int getLineIndex()
        {
            return line;
        }
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
        inline bool isRunning()
        {
            return running;
        }

        inline int getDivider()
        {
            return divider;
        }
        inline QMap<double, double>* getAverage()
        {
            return average;
        }
        inline QList<double>* getBuffer()
        {
            return &buffer;
        }
        inline QMap<double, double>* getDark()
        {
            return mode == Crosscorrelator ? (crossdark) : (mode == Autocorrelator ? (autodark) : dark);
        }
        inline QLineSeries* getCounts()
        {
            return counts;
        }
        inline QLineSeries* getAutocorrelations()
        {
            return autocorrelations;
        }
        inline QLineSeries* getMagnitude()
        {
            return magnitude;
        }
        inline QLineSeries* getPhase()
        {
            return phase;
        }
        inline QScatterSeries* getSpectrum()
        {
            return spectrum;
        }
        inline QList<double*> getCrosscorrelations()
        {
            return crosscorrelations;
        }
        inline dsp_stream_p getStream()
        {
            return stream;
        }
        inline double getAverageBottom()
        {
            return averageBottom;
        }
        inline double getAverageTop()
        {
            return averageTop;
        }
        inline dsp_location *getLocation()
        {
            return &location;
        }
        void setLocation(dsp_location location);
        double percent;
        void setPercent();
        void insertValue(double x, double y);
        void sumValue(double x, double y);

    private:
        int buffersize { 3 };
        int divider;
        Ui::Line *ui;
        bool applysigmaclipping;
        bool applymedian;
        dsp_stream_p stream;
        dsp_location location;
        double *ac;
        fftw_plan plan;
        fftw_complex *dft;
        bool running;
        QString name;
        QSettings *settings;
        QList<Line*> *parents;
        QList<Baseline*> nodes;
        QList<double> buffer;
        bool scanning;
        int stop;
        double stack;
        Mode mode;
        QMap<double, double>* dark;
        QMap<double, double>* autodark;
        QMap<double, double>* crossdark;
        QMap<double, double>* average;
        QScatterSeries* spectrum;
        QLineSeries* series;
        QLineSeries* magnitude;
        QLineSeries* phase;
        QLineSeries* counts;
        QLineSeries* autocorrelations;
        QList<double*> crosscorrelations;
        unsigned int line;
        int flags;
        double averageBottom { 0 };
        double averageTop { 1.0 };
        void getMinMax();

    signals:
        void activeStateChanged(Line* line);
};

#endif // LINE_H
