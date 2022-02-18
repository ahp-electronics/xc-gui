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
#include "types.h"
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
            activeStateChanged(this);
        }
        inline int getFlags()
        {
            return flags;
        }
        inline double getOffset() { return offset; }
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
            return *percent;
        }
        inline void setPercentPtr(double *ptr)
        {
            percent = ptr;
        }
        inline void resetPercentPtr()
        {
            percent = &localpercent;
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

        inline QLineSeries* getMagnitudes()
        {
            return magnitudes;
        }
        inline QLineSeries* getPhases()
        {
            return phases;
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
        inline QLineSeries* getCounts()
        {
            return counts;
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
        double *percent;
        double localpercent;
        void setPercent();


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

        inline int getStartLine() { return start; }
        inline int getEndLine() { return end; }
        void TakeDark(Line* sender);
        bool DarkTaken();

        void gotoRaDec(double ra, double dec);
        void startTracking(double ra_rate, double dec_rate);

    private:
        void stretch(QLineSeries* series);
        void stackValue(QLineSeries* series, QMap<double, double>* stacked, int index, double x, double y);

        double stack;
        double *magnitude_buf { nullptr };
        double *phase_buf { nullptr };
        double offset { 0.0 };
        double timespan { 1.0 };
        bool scalingDone { false };
        int start { 0 };
        int end { 1 };
        int len { 1 };

        Elemental *elemental;
        QList<int> Motors;
        int motorIndex {0};
        double mx;
        Ui::Line *ui;
        bool applysigmaclipping;
        bool applymedian;
        dsp_stream_p stream;
        dsp_location location;
        fftw_plan plan;
        fftw_complex *dft { nullptr };
        bool running;
        QString name;
        QSettings *settings;
        QList<Line*> *parents;
        QList<Baseline*> nodes;
        bool scanning;
        int stop;
        Mode mode;
        QMap<double, double>* dark;
        QMap<double, double>* magnitudeStack;
        QMap<double, double>* phaseStack;
        QLineSeries* magnitude;
        QLineSeries* phase;
        QLineSeries* magnitudes;
        QLineSeries* phases;
        QLineSeries* counts;
        unsigned int line;
        int flags { 0x8 };
        double averageBottom { 0 };
        double averageTop { 1.0 };
        void getMinMax();
        void plot(bool success, double o, double s);
        void SavePlot();
    signals:
        void activeStateChanged(Line*);
        void updateBufferSizes();
        void savePlot();
        void takeDark(Line* sender);
        void clearCrosscorrelations();
};

#endif // LINE_H
