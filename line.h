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
#include "graph.h"
#include "types.h"
#include "baseline.h"
#include "elemental.h"
#include "series.h"

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
        bool idft();
        bool Align();
        bool Differential();
        bool scanActive();
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
        inline Graph* getGraph() { return graph; }
        inline void setGraph(Graph* g) { graph = g; }
        inline Graph* gethistogram() { return histogram; }
        inline void sethistogram(Graph* h) { histogram = h; }
        bool getFlag(int flag);
        bool showCounts();
        bool showAutocorrelations();
        bool showCrosscorrelations();
        void Initialize();

        QString getName()
        {
            return name;
        }
        void setMode(Mode m);
        Mode getMode()
        {
            return mode;
        }
        Scale getYScale();
        void stackCorrelations(ahp_xc_sample *spectrum = nullptr);
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
            stack_index = 1;
            getSpectrum()->clear();
        }
        inline void clearCounts()
        {
            getCounts()->clear();
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
        inline void setSpectrumSize(size_t size)
        {
            getSpectrum()->getElemental()->setStreamSize(size);
        }
        inline size_t getSpectrumSize()
        {
            return getSpectrum()->getElemental()->getStreamSize();
        }
        inline Series* getSpectrum()
        {
            return spectrum;
        }
        inline QMap<double, double>* getDark()
        {
            return getSpectrum()->getDark();
        }
        inline int smooth()
        {
            return _smooth;
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
            return getStream()->location;
        }
        inline dsp_location *targetLocation()
        {
            return &target_location;
        }
        void setLocation(int value = 0);

        void updateLocation();
        void updateRa();
        void updateDec();
        bool isRailBusy();
        bool isMountBusy();
        double *percent;
        double localpercent;
        void setPercent();
        void smoothBuffer(QXYSeries* buf, QList<double>* raw, int offset, int len);
        void smoothBuffer(double* buf, int len);
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
        inline int getStartChannel()
        {
            return channel_start;
        }
        inline int getEndChannel()
        {
            return channel_end;
        }
        inline int getChannelBandwidth()
        {
            return channel_len;
        }
        inline int getScanStep()
        {
            return channel_step;
        }
        inline int getStartLag()
        {
            return lag_start;
        }
        inline int getEndLag()
        {
            return lag_end;
        }
        inline int getLagBandwidth()
        {
            return lag_len;
        }
        inline int getLagStep()
        {
            return lag_step;
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
        inline void setElevation(double elevation)
        {
            Elevation = elevation;
        }
        inline double getMinFrequency()
        {
            return minfreq;
        }
        inline double getMaxFrequency()
        {
            return maxfreq;
        }
        inline void setMinFrequency(double value)
        {
            minfreq = value;
        }
        inline void setMaxFrequency(double value)
        {
            maxfreq = value;
        }
        void TakeDark(Line* sender);
        bool DarkTaken();
        void GetDark();
        void setBufferSizes();
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

        inline Series* getCounts()
        {
            return counts;
        }
        inline int getResolution()
        {
            return Resolution;
        }

        inline double getPacketTime() { return packetTime; }
        void addCount(double starttime, ahp_xc_packet *packet = nullptr);
        inline double getTimeRange() { return timeRange; }
        inline void setTimeRange(double range) { timeRange = range; }
        inline ahp_xc_packet* getPacket() { return packet; }
        inline void setPacket(ahp_xc_packet* p) { packet = p; }
        void unloadPositionChart(bool checked);
        void loadPositionChart(bool checked);
    private:
        QWidget *parent;
        ahp_xc_packet* packet;
        double timeRange { 10.0 };
        double packetTime { 0.0 };
        double Ra, Dec;
        static QMutex motor_mutex;
        QMutex mutex;
        double Frequency { LIGHTSPEED };
        void stretch(QLineSeries* series);

        fftw_plan plan { 0 };
        QList<int> Motors;
        Ui::Line *ui { nullptr };
        dsp_stream_p stream { nullptr };
        QString name;
        int nsamples { 0 };
        int _smooth { 5 };
        int *stop;
        int localstop { 1 };
        int RailMotorIndex {1};
        int MountMotorIndex {1};
        Mode mode { Counter };
        QSettings *settings { nullptr };
        QList<Line*> *parents { nullptr };
        QList<Baseline*> nodes { nullptr };
        Series* spectrum { nullptr };
        Series* counts { nullptr };
        QChar dir_separator { '/' };
        QList <dsp_location> xyz_locations;
        dsp_location target_location;
        int current_location { 0 };
        double stack_index { 1.0 };
        Graph* graph;
        Graph* histogram;
        unsigned int line { 0 };
        int flags { 0x8 };
        bool radix_x { false };
        bool radix_y { false };
        double lag_start { 0 };
        double lag_end { 1 };
        double lag_len { 1 };
        double lag_step { 1 };
        double channel_max { 10000 };
        double channel_start { 0 };
        double channel_end { 1 };
        double channel_len { 1 };
        double channel_step { 1 };
        double Resolution { 1024 };
        double AutoChannel { 1 };
        double CrossChannel { 0 };
        double base_x;
        double base_y;
        double maxfreq { 100000000 };
        double minfreq { 0 };
        double averageBottom { 0 };
        double averageTop { 1.0 };
        double offset { 0.0 };
        double timespan { 1.0 };
        double Latitude { 0.0 };
        double Longitude { 0.0 };
        double Elevation { 0.0 };
        double MinValue { 0.0 };
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
        void scanActiveStateChanging(Line*);
        void scanActiveStateChanged(Line*);
        void activeStateChanging(Line*);
        void activeStateChanged(Line*);
        void updateBufferSizes();
        void savePlot();
        void takeDark(Line* sender);
        void clear();
};

#endif // LINE_H
