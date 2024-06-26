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
#include <fcntl.h>
#include <QThread>
#include <QStatusBar>
#include <QTimer>
#include <QTemporaryFile>
#include <QCoreApplication>
#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSettings>
#include <QFile>
#include <QMutex>
#include <QIODevice>
#include <QStandardPaths>
#include <QDateTime>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThreadPool>
#include <QQueue>
#include <QDir>
#include <QMessageBox>
#include "graph.h"
#include "line.h"
#include "polytope.h"
#include "types.h"
#define NUM_CONTEXTS 4

#define fdclose(fd, mode) (fclose(fdopen(fd, mode)))

QT_BEGIN_NAMESPACE
namespace Ui
{
class Graph;
class Polytope;
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
        double percent;
        void resizeEvent(QResizeEvent* event);
        QList<Line*> Lines;
        QList<Polytope*> Polytopes;
        inline ahp_xc_packet* getPacket()
        {
            return (packet == nullptr) ? createPacket() : packet;
        }
        inline double getFrequency()
        {
            return ahp_xc_get_frequency();
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
            for(Line* line : Lines)
                line->setPacket (packet);
            for(Polytope* line : Polytopes)
                line->setPacket (packet);
            return packet;
        }
        inline void freePacket()
        {
            ahp_xc_enable_intensity_crosscorrelator(false);
            ahp_xc_free_packet(packet);
        }
        inline Graph *getGraph()
        {
            return graph;
        }
        inline Graph *getHistogram()
        {
            return histogram;
        }
        inline Mode getMode()
        {
            return mode;
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
        inline double getStartTime()
        {
            return J2000_starttime;
        }
        inline void setMode(Mode m)
        {
            if(connected)
            {
                stopThreads();
                freePacket();
                mode = m;
                if(mode == CrosscorrelatorII || mode == HolographII)
                    ahp_xc_enable_intensity_crosscorrelator(true);
                else
                    ahp_xc_enable_intensity_crosscorrelator(false);
                if(mode == HolographII || mode == HolographIQ)
                    getHistogram()->hide();
                else
                    getHistogram()->show();
                xc_capture_flags cur = ahp_xc_get_capture_flags();
                ahp_xc_set_capture_flags((xc_capture_flags)((cur & ~CAP_ENABLE) | CAP_RESET_TIMESTAMP));
                resetTimestamp();
                getGraph()->setMode(m);
                getHistogram()->setMode(m);
                createPacket();
                for(int i = 0; i < Lines.count(); i++)
                {
                    Lines[i]->setMode(mode);
                }
                for(int i = 0; i < Polytopes.count(); i++)
                {
                    Polytopes[i]->setMode(mode);
                }
                for(int x = 0; x < Lines.count(); x++) {
                    Lines[x]->setActive(false);
                }
                resizeEvent(nullptr);
                startThreads();
            }
        }
        void updateOrder();
        void runClicked(bool checked);
        void setVoltage(int level);
        void resetTimestamp();
        void VlbiThread(Thread * thread);
        QDateTime start;
        QQueue <ahp_xc_packet*> packetFifo;
        Thread *readThread;
        Thread *uiThread;
        Thread *vlbiThread;
        Thread *motorThread;
        Thread *controlThread;

        inline int getMotorHandle()
        {
            return motorFD;
        }
        inline void setMotorHandle(int fd)
        {
            motorFD = fd;
        }
        inline QList<double> getMotorPositionMultipliers()
        {
            return position_multipliers;
        }
        inline QList<int> getMotorAddresses()
        {
            return gt_addresses;
        }

        inline int getMotorFD()
        {
            return motorFD;
        }
        inline int getControlFD()
        {
            return controlFD;
        }
        inline int getXcFD()
        {
            return xcFD;
        }

        static QMutex vlbi_mutex;
        static bool lock_vlbi()
        {
            return vlbi_mutex.tryLock(1);
        }
        static void unlock_vlbi()
        {
            vlbi_mutex.unlock();
        }

        void lock()
        {
            while(!mutex.tryLock());
        }
        void unlock()
        {
            mutex.unlock();
        }

        QString homedir;
        QString bsdl_filename;
        QString svf_filename;
        QString dfu_filename;
        QString stdout_filename;
        int fd_stdout;

signals:
        void newPacket(ahp_xc_packet*);
        void repaint();
        void scanStarted();
        void scanFinished(bool complete);

    private:
        int lastlog_pos { 0 };
        bool enable_vlbi {false};
        bool has_svf_firmware {false};
        bool has_bsdl {false};
        QStringList CheckFirmware(QString url, int timeout_ms = 30000);
        bool DownloadFirmware(QString url, QString svf, QString bsdl, QSettings *settings, int timeout_ms = 30000);
        QString base64;
        int currentVoltage {0};
        FILE *f_stdout;
        int32_t threadsStopped { 1 };
        void plotModel(QImage* picture, char* model);
        QMutex mutex;
        void stopThreads();
        void startThreads();
        int motorFD;
        int controlFD;
        int xcFD;
        bool xc_local_port { false };
        double lastpackettime;
        double Ra, Dec, Distance;
        double Latitude, Longitude, Elevation;
        double wavelength;
        int current_context { 0 };
        ahp_xc_packet *packet;
        QSettings *settings;
        QTcpSocket xc_socket;
        QUdpSocket motor_socket;
        QUdpSocket control_socket;
        QSerialPort serial;
        QFile file;
        Mode mode;
        bool connected;
        double TimeRange;
        int Order;
        Graph *graph;
        Graph *histogram;
        Ui::MainWindow *ui;
        double J2000_starttime;
        timespec starttime;
        int spectrum_resolution { 512 };

        int gt_address;
        QList<double> position_multipliers;
        QList<int> gt_addresses;

        void readThreadCallback(Thread* sender);
        void uiThreadCallback(Thread* sender);
        void vlbiThreadCallback(Thread* sender);
        void motorThreadCallback(Thread* sender);
};
#endif // MAINWINDOW_H
