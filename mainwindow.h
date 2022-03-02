/*
    MIT License

    xc-gui GUI for libahp_xc and OpenVLBI using the AHP XC correlators
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <cmath>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <QThread>
#include <QCoreApplication>
#include <QMainWindow>
#include <QTcpSocket>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QMutex>
#include <QIODevice>
#include <QStandardPaths>
#include <QDateTime>
#include <QTcpSocket>
#include <QThreadPool>
#include <QDir>
#include "graph.h"
#include "line.h"
#include "baseline.h"
#include "types.h"
#define NUM_CONTEXTS 4

#define fdclose(fd, mode) (fclose(fdopen(fd, mode)))

QT_BEGIN_NAMESPACE
namespace Ui
{
class Graph;
class Baseline;
class Line;
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
        Q_OBJECT

    public:
        MainWindow(QWidget *parent = nullptr);
        ~MainWindow();
        int percent, finished;
        void resizeEvent(QResizeEvent* event);
        QList<Line*> Lines;
        QList<Baseline*> Baselines;
        inline double getFrequency()
        {
            return ahp_xc_get_frequency() >> ahp_xc_get_frequency_divider();
        }
        inline double getRa()
        {
            return Ra;
        }
        inline double getDec()
        {
            return Dec;
        }
        inline void setRa(double value)
        {
            Ra = value;
        }
        inline void setDec(double value)
        {
            Dec = value;
        }
        inline double getTimeRange()
        {
            return TimeRange;
        }
        inline ahp_xc_packet * createPacket()
        {
            packet = ahp_xc_alloc_packet();
            return packet;
        }
        inline ahp_xc_packet * getPacket()
        {
            return packet;
        }
        inline void freePacket()
        {
            ahp_xc_free_packet(packet);
        }
        inline Graph *getGraph()
        {
            return graph;
        }
        inline Mode getMode()
        {
            return mode;
        }
        inline void* getVLBIContext(int index = 0)
        {
            return context[index];
        }
        inline double getStartTime()
        {
            return J2000_starttime;
        }
        inline void setMode(Mode m)
        {
            if(connected)
            {
                if(!ahp_xc_has_crosscorrelator() && (m == CrosscorrelatorI || m == CrosscorrelatorIQ || m == HolographIQ))
                    return;
                mode = m;
                stopThreads();
                xc_capture_flags cur = ahp_xc_get_capture_flags();
                ahp_xc_set_capture_flags((xc_capture_flags)((cur&~CAP_ENABLE)|CAP_RESET_TIMESTAMP));
                for(int i = 0; i < Baselines.count(); i++)
                {
                    Baselines[i]->getMagnitude()->clear();
                    Baselines[i]->getPhase()->clear();
                    Baselines[i]->getMagnitudes()->clear();
                    Baselines[i]->getPhases()->clear();
                    Baselines[i]->getCounts()->clear();
                }
                for(int i = 0; i < Lines.count(); i++)
                {
                    Lines[i]->getMagnitude()->clear();
                    Lines[i]->getPhase()->clear();
                    Lines[i]->getMagnitudes()->clear();
                    Lines[i]->getPhases()->clear();
                    Lines[i]->getCounts()->clear();
                }
                if(mode == Counter || mode == HolographIQ || mode == HolographII) {
                    for(int i = 0; i < Lines.count(); i++) {
                        if(ahp_xc_has_crosscorrelator())
                            ahp_xc_set_channel_cross(i, 0, 0);
                        ahp_xc_set_channel_auto(i, 0, 0);
                    }
                    ahp_xc_set_capture_flags((xc_capture_flags)(cur|CAP_ENABLE));
                    resetTimestamp();
                }
                for(int i = 0; i < Lines.count(); i++)
                    Lines[i]->setMode(mode);
                for(int i = 0; i < Baselines.count(); i++)
                    Baselines[i]->setMode(mode);
                getGraph()->setMode(m);
                if(mode == HolographIQ || mode == HolographII)
                    vlbiThread->start();
                startThreads();
            }
        }
        void setVoltage(unsigned char level);
        void resetTimestamp();
        QDateTime start;
        Thread *readThread;
        Thread *uiThread;
        Thread *vlbiThread;
        Thread *motorThread;

        inline int getMotorHandle() { return motorFD; }
        inline void setMotorHandle(int fd) { motorFD = fd; }
        inline QList<double> getMotorPositionMultipliers() { return position_multipliers; }
        inline QList<int> getMotorAddresses() { return gt_addresses; }

        inline int getMotorFD() { return motorFD; }
        inline int getControlFD() { return controlFD; }
        inline int getXcFD() { return xcFD; }
        void plotVLBI(char *model, QImage *picture, double ra, double dec, vlbi_func2_t delegate);
        void QImageFromModel(QImage* picture, char* model);
    private:
        void stopThreads();
        void startThreads();
        int motorFD;
        int controlFD;
        int xcFD;
        double lastpackettime;
        QMutex vlbi_mutex;
        double Ra, Dec;
        double wavelength;
        void* context[vlbi_total_contexts];
        ahp_xc_packet *packet;
        QSettings *settings;
        QTcpSocket xc_socket;
        QTcpSocket motor_socket;
        QTcpSocket control_socket;
        QSerialPort serial;
        QFile file;
        Mode mode;
        bool connected;
        double TimeRange;
        Graph *graph;
        Ui::MainWindow *ui;
        double J2000_starttime;
        timespec starttime;

        int gt_address;
        QList<double> position_multipliers;
        QList<int> gt_addresses;
};
#endif // MAINWINDOW_H
