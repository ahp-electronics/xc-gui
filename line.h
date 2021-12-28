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
#include "elemental.h"

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
                getMagnitudes()->clear();
                getPhases()->clear();
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
        void stretch(QLineSeries* series);
        void stackValue(QLineSeries* series, QMap<double, double>* stacked, double x, double y);


        inline bool hasMotorbBus()
        {
            return ahp_gt_is_connected();
        }
        inline bool queryMotor(int index, int axis = -1)
        {
            if(!hasMotorbBus())
                return false;
            ahp_gt_select_device(index);
            if(axis < 0) {
                ahp_gt_read_values(0);
                ahp_gt_read_values(1);
            } else
                ahp_gt_read_values(axis);
            return (ahp_gt_get_mc_version() > 0x31);
        }

        inline bool selectMotor(int index, int axis) {
            if(!hasMotorbBus())
                return false;
            return queryMotor(Motors[index], axis);
        }

        inline int addMotor(int address) {
            if(!hasMotorbBus())
                return -1;
            if(queryMotor(address))
                Motors.append(address);
            return Motors.count()-1;
        }

        inline double getMotorAxisPosition(int axis, int index = 0) { if(!hasMotorbBus()) return 0.0; selectMotor(axis, index); return ahp_gt_get_position(axis); }
        inline void setMotorAxisPosition(int axis, double value, int index = 0) { if(!hasMotorbBus()) return; selectMotor(axis, index); ahp_gt_set_position(axis, value); }
        inline void moveMotorAxisBy(int axis, double value, double speed, int index = 0) { if(!hasMotorbBus()) return; selectMotor(axis, index); ahp_gt_goto_relative(axis, value, speed); }
        inline void moveMotorAxisTo(int axis, double value, double speed, int index = 0) { if(!hasMotorbBus()) return; selectMotor(axis, index); ahp_gt_goto_absolute(axis, value, speed); }
        inline void stopMotorAxis(int axis, int index = 0) { if(!hasMotorbBus()) return; selectMotor(axis, index); ahp_gt_stop_motion(axis, false); }

    private:
        double *buf;
        double offset { 0.0 };
        double timespan { 1.0 };
        bool scalingDone { false };
        Elemental *elemental;
        QList<int> Motors;
        int motorIndex {0};
        double mx;
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
        bool scanning;
        int stop;
        double stack;
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
