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
        inline QLineSeries* getMagnitude()
        {
            return magnitude;
        }
        inline QLineSeries* getPhase()
        {
            return phase;
        }
        inline void setMagnitudeSize(size_t size)
        {
            if(magnitude_size == size) return;
            magnitude_size = size;
            if(magnitude_buf != nullptr)
                magnitude_buf = (double*)realloc(magnitude_buf, sizeof(double) * (size + 1));
            else
                magnitude_buf = (double*)malloc(sizeof(double) * (size + 1));
        }
        inline void setPhaseSize(size_t size)
        {
            if(phase_size == size) return;
            phase_size = size;
            if(phase_buf != nullptr)
                phase_buf = (double*)realloc(phase_buf, sizeof(double) * (size + 1));
            else
                phase_buf = (double*)malloc(sizeof(double) * (size + 1));
        }
        inline Elemental *getElemental() {
            return elemental;
        }
        inline QLineSeries* getCounts()
        {
            return counts;
        }
        inline QMap<double, double>* getCountStack()
        {
            return countStack;
        }
        inline Elemental *getCountElemental() {
            return elementalCounts;
        }
        inline QLineSeries* getPhases()
        {
            return phases;
        }
        inline QMap<double, double>* getPhaseStack()
        {
            return phaseStack;
        }
        inline Elemental *getPhaseElemental() {
            return elementalPhase;
        }
        inline QLineSeries* getMagnitudes()
        {
            return magnitudes;
        }
        inline QMap<double, double>* getMagnitudeStack()
        {
            return magnitudeStack;
        }
        inline Elemental *getMagnitudeElemental() {
            return elementalMagnitude;
        }
        inline QMap<double, double>* getDark()
        {
            return dark;
        }
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
        bool dft();
        int smooth();
        void smoothBuffer(QLineSeries* buf, int offset, int len);
        void smoothBuffer(double* buf, int len);
        void stackValue(QLineSeries* series, QMap<double, double>* stacked, double x, double y);

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

    private:
        int localstop { 0 };
        int *stop { nullptr };
        double *percent { nullptr };
        double localpercent { 0 };
        Graph *graph;
        ahp_xc_packet* packet;
        double timeRange { 10.0 };
        double packetTime { 0.0 };
        QMutex mutex;
        bool running { false };
        void setBufferSizes();
        void stretch(QLineSeries* series);

        dsp_stream_p stream { nullptr };
        fftw_plan plan;
        double MinValue { 0.0 };
        double *magnitude_buf { nullptr };
        double *phase_buf { nullptr };
        size_t magnitude_size { 0 };
        size_t phase_size { 0 };
        double offset { 0.0 };
        double timespan { 1.0 };
        int Index;
        int *start;
        int *end;
        int *len;
        int *step;
        int *size;
        int size_2nd;
        int tail_size {1};
        int head_size {1};
        QSettings *settings;
        QString name;
        bool scanning;
        bool threadRunning;
        bool oldstate;
        Mode mode;
        Elemental *elemental;
        Elemental* elementalCounts { nullptr };
        Elemental* elementalPhase { nullptr };
        Elemental* elementalMagnitude { nullptr };
        QMap<double, double>* countStack;
        QMap<double, double>* phaseStack;
        QMap<double, double>* magnitudeStack;
        QMap<double, double>* dark;
        QLineSeries* counts;
        QLineSeries* phases;
        QLineSeries* magnitudes;
        QLineSeries* magnitude;
        QLineSeries* phase;
        QList<Line*> Nodes;
        double stack_index { 1.0 };
        int correlation_order {2};
        QList<Line*> lines;
signals:
        void activeStateChanging(Baseline*);
        void activeStateChanged(Baseline*);
};

#endif // BASELINE_H
