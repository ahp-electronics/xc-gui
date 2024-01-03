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
#include "series.h"
#include "types.h"
#include "elemental.h"

using namespace QtCharts;
class Line;
class Graph;
class Baseline : public QWidget
{
        Q_OBJECT

    public:
        Baseline(QString name, int index, QList<Line*> nodes, QSettings *settings = nullptr, QWidget *parent = nullptr);
        ~Baseline();

        inline void setName(QString n)
        {
            name = n;
        }
        inline QString getName()
        {
            return name;
        }
        inline Series* getCounts()
        {
            return counts;
        }
        inline Series* getSpectrum()
        {
            return spectrum;
        }
        inline void setSpectrumSize(size_t size)
        {
            getSpectrum()->getElemental()->setStreamSize(size);
        }
        inline size_t getSpectrumSize()
        {
            return getSpectrum()->getElemental()->getStreamSize();
        }
        QMap<double, double>* getDark();
        void setMode(Mode m);
        inline Mode getMode()
        {
            return mode;
        }
        inline unsigned int getLineIndex()
        {
            return Index;
        }
        inline void setDelay(double s);
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
            return scanning;
        }
        inline Line* getLine(int index)
        {
            return lines.at(index);
        }
        inline QList<Line*> getLines()
        {
            return lines;
        }
        void setCorrelationOrder(int order);
        inline int getCorrelationOrder()
        {
            return correlation_order;
        }

        bool isActive(bool atleast1 = false);
        bool scanActive(bool atleast1 = false);

        void stackCorrelations();
        void plot(bool success, double o, double s);
        void SavePlot();
        void TakeMeanValue(Line *sender);
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

        inline vlbi_context getVLBIContext(int index = -1)
        {
            if(index < 0) {
                index = getMode() - HolographII;
                if(index < 0)
                    return nullptr;
            }
            return context[index];
        }
        bool idft();
        int smooth();

        void lock()
        {
            while(!mutex.tryLock());
        }
        void unlock()
        {
            mutex.unlock();
        }
        inline double getPacketTime() { return packetTime; }
        void addCount(double starttime, ahp_xc_packet *packet = nullptr);
        inline double getTimeRange() { return timeRange; }
        inline void setTimeRange(double range) { timeRange = range; }
        inline ahp_xc_packet* getPacket() { return packet; }
        inline void setPacket(ahp_xc_packet* p) { packet = p; }
        inline Graph *getGraph() { return graph; }
        inline void setGraph(Graph * g) { graph = g; }
        inline Graph* gethistogram() { return histogram; }
        inline void sethistogram(Graph* h) { histogram = h; }
        inline int getStartLag()
        {
            return lag_head_size;
        }
        inline int getEndLag()
        {
            return lag_tail_size;
        }
        inline int getLagBandwidth()
        {
            return lag_size_2nd;
        }
        inline int getLagStep()
        {
            return lag_step_size;
        }
        inline int getScanStep()
        {
            return step_size;
        }

    private:
        int localstop { 0 };
        int *stop { nullptr };
        double *percent { nullptr };
        double localpercent { 0 };
        Graph *graph;
        Graph *histogram;
        ahp_xc_packet* packet;
        double timeRange { 10.0 };
        double packetTime { 0.0 };
        QMutex mutex;
        bool running { false };
        void setBufferSizes();
        void stretch(Series* series);

        dsp_stream_p stream { nullptr };
        fftw_plan plan;
        double MinValue { 0.0 };
        size_t magnitude_size { 0 };
        size_t phase_size { 0 };
        double offset { 0.0 };
        double timespan { 1.0 };
        int Index;
        int *start;
        int *end;
        int *step;
        int *size;
        double *lag_start;
        double *lag_end;
        double *lag_step;
        double *lag_size;
        int size_2nd {3};
        int step_size {1};
        int tail_size {1};
        int head_size {1};
        int len {1};
        int lag_size_2nd {3};
        int lag_step_size {1};
        int lag_tail_size {1};
        int lag_head_size {1};
        QSettings *settings;
        QString name;
        bool scanning;
        bool threadRunning;
        bool oldstate;
        Mode mode;
        Series* counts;
        Series* spectrum;
        QList<Line*> Nodes;
        int correlation_order {2};
        QList<Line*> lines;
        QList<int> indexes;
signals:
        void activeStateChanging(Baseline*);
        void activeStateChanged(Baseline*);
};

#endif // BASELINE_H
