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
#include <ctime>
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
        bool Histogram();
        bool dft();
        bool Align();
        bool Differential();
        bool isEnabled();
        inline bool isActive()
        {
            return running;
        }
        void setActive(bool a);
        inline int getFlags()
        {
            return flags;
        }
        inline double getOffset()
        {
            return offset;
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
        inline QString getLastName()
        {
            return name + "_" + QString::number(floor(vlbi_time_timespec_to_J2000time(getStream()->starttimeutc)));
        }
        void setMode(Mode m);
        Mode getMode()
        {
            return mode;
        }
        Scale getYScale();
        void stackCorrelations(ahp_xc_sample *spectrum = nullptr, int npackets = 0);
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
        inline int getStop()
        {
            return *stop;
        }
        inline void setStopPtr(int *ptr)
        {
            stop = ptr;
        }
        inline void resetStopPtr()
        {
            stop = &localstop;
        }
        inline double isScanning()
        {
            return !stop;
        }
        inline void Stop()
        {
            *stop = 1;
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
                magnitude_buf = (double*)realloc(magnitude_buf, sizeof(double) * (size + 1));
            else
                magnitude_buf = (double*)malloc(sizeof(double) * (size + 1));
        }
        inline void setPhaseSize(size_t size)
        {
            if(phase_buf != nullptr)
                phase_buf = (double*)realloc(phase_buf, sizeof(double) * (size + 1));
            else
                phase_buf = (double*)malloc(sizeof(double) * (size + 1));
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
        void updateLocation();
        void updateRa();
        void updateDec();
        bool isRailBusy();
        bool isMountBusy();
        double *percent;
        double localpercent;
        void setPercent();

        void addToVLBIContext();
        void removeFromVLBIContext();

        inline vlbi_context getVLBIContext(int index = -1)
        {
            if(index < 0) {
                index = getMode() - HolographII;
                if(index < 0)
                    return nullptr;
            }
            return context[index];
        }

        inline int getMountMotorIndex()
        {
            return MountMotorIndex;
        }

        inline int getRailMotorIndex()
        {
            return RailMotorIndex;
        }

        inline int getStartLine()
        {
            return start;
        }
        inline int getEndLine()
        {
            return len;
        }
        inline double getLatitude()
        {
            return Latitude;
        }
        inline double getLongitude()
        {
            return Longitude;
        }
        inline void setLatitude(double latitude)
        {
            Latitude = latitude;
        }
        inline void setLongitude(double longitude)
        {
            Longitude = longitude;
        }
        void TakeDark(Line* sender);
        bool DarkTaken();
        void GetDark();
        void UpdateBufferSizes();
        void runClicked(bool checked = false);
        void resetTimestamp();
        inline bool isForkMount() { return fork; }
        void lock()
        {
            while(!mutex.tryLock());
        }
        void unlock()
        {
            mutex.unlock();
        }
        static void motor_lock();
        static void motor_unlock();
        int getRailIndex() {
            return RailMotorIndex;
        }
        int getMountIndex() {
            return MountMotorIndex;
        }
        void flipMount(bool flip) {
            flipped = flip;
        }
        bool isFlipped() {
            return flipped;
        }
        void gotoRaDec(double ra, double dec);
        void startSlewing(double ra_rate, double dec_rate);
        void startTracking();
        void haltMotors();
        double getRa() { return Ra; }
        double getDec() { return Dec; }
        void setRa(double ra) { Ra = ra; }
        void setDec(double dec) { Dec = dec; }

    private:
        double Ra, Dec;
        static QMutex motor_mutex;
        QMutex mutex;
        double Frequency { LIGHTSPEED };
        void stretch(QLineSeries* series);
        void stackValue(QLineSeries* series, QMap<double, double>* stacked, int index, double x, double y);

        fftw_plan plan { 0 };
        double *magnitude_buf { nullptr };
        double *phase_buf { nullptr };
        Elemental *elemental { nullptr };
        QList<int> Motors;
        Ui::Line *ui { nullptr };
        dsp_stream_p stream { nullptr };
        dsp_location location { 0 };
        QString name;
        int *stop;
        int localstop { 1 };
        int RailMotorIndex {1};
        int MountMotorIndex {1};
        Mode mode { Counter };
        QSettings *settings { nullptr };
        QList<Line*> *parents { nullptr };
        QList<Baseline*> nodes { nullptr };
        QMap<double, double>* dark { nullptr };
        QMap<double, double>* magnitudeStack { nullptr };
        QMap<double, double>* phaseStack { nullptr };
        QLineSeries* magnitude { nullptr };
        QLineSeries* phase { nullptr };
        QLineSeries* magnitudes { nullptr };
        QLineSeries* phases { nullptr };
        QLineSeries* counts { nullptr };
        unsigned int line { 0 };
        int flags { 0x8 };
        int start { 0 };
        int end { 1 };
        int len { 1 };
        double mx { 0.0 };
        double averageBottom { 0 };
        double averageTop { 1.0 };
        double offset { 0.0 };
        double timespan { 1.0 };
        double stack { 0.0 };
        double Latitude { 0.0 };
        double Longitude { 0.0 };
        timespec starttime { 0 };
        bool applysigmaclipping { false };
        bool applymedian { false };
        bool running { false };
        bool scanning { false };
        bool flipped { false };
        bool scalingDone { false };
        bool fork { false };
        void getMinMax();
        void plot(bool success, double o, double s);
        void SavePlot();

    signals:
        void activeStateChanging(Line*);
        void activeStateChanged(Line*);
        void updateBufferSizes();
        void savePlot();
        void takeDark(Line* sender);
        void clearCrosscorrelations();
};

#endif // LINE_H
