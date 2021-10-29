#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QThread>
#include <QCoreApplication>
#include <QMainWindow>
#include <QTcpSocket>
#include <QSerialPort>
#include <QSettings>
#include <QMutex>
#include <ahp_xc.h>
#include <vlbi.h>
#include "graph.h"
#include "line.h"
#include "baseline.h"
#include "types.h"
#include "threads.h"

QT_BEGIN_NAMESPACE
namespace Ui {
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
    inline double getFrequency() { return ahp_xc_get_frequency()>>ahp_xc_get_frequency_divider(); }
    inline double getRa() { return Ra; }
    inline double getDec() { return Dec; }
    inline void setRa(double value) { Ra = value; }
    inline void setDec(double value) { Dec = value; }
    inline double getTimeRange() { return TimeRange; }
    inline ahp_xc_packet * createPacket() {  packet = ahp_xc_alloc_packet(); return packet; }
    inline ahp_xc_packet * getPacket() { return packet; }
    inline void freePacket() { ahp_xc_free_packet(packet); }
    inline Graph *getGraph() { return graph; }
    inline Mode getMode() { return mode; }
    inline void* getVLBIContext() { return vlbi_context; }
    inline double getStartTime() { return J2000_starttime; }
    inline void setMode(Mode m) {
        mode = m;
        if(connected) {
            getGraph()->setMode(m);
            ahp_xc_set_capture_flag(CAP_ENABLE);
            for(int i = 0; i < Lines.count(); i++)
                ahp_xc_set_lag_cross(i, 0);
            ahp_xc_clear_capture_flag(CAP_ENABLE);
            for(int i = 0; i < Baselines.count(); i++)
                Baselines[i]->getValues()->clear();
            if(mode != Autocorrelator)
            {
                ahp_xc_set_capture_flag(CAP_ENABLE);
            }
        }
    }
    inline void resetTimestamp() {
        ahp_xc_set_capture_flag(CAP_RESET_TIMESTAMP);
        start = QDateTime::currentDateTimeUtc();
        lastpackettime = 0;
        ahp_xc_clear_capture_flag(CAP_RESET_TIMESTAMP);
        timespec ts = vlbi_time_string_to_timespec ((char*)start.toString(Qt::DateFormat::ISODate).toStdString().c_str());
        J2000_starttime = vlbi_time_timespec_to_J2000time(ts);
        for(int i = 0; i < Lines.count(); i++)
            Lines[i]->getStream()->starttimeutc = ts;

    }
    QDateTime start;
    Thread *readThread;
    Thread *uiThread;
    Thread *vlbiThread;
    Thread *motorThread;

    inline int getMotorHandle() { return motorFD; }
    inline void setMotorHandle(int fd) { motorFD = fd; }
    inline QList<double> getMotorPositionMultipliers() { return position_multipliers; }
    inline QList<int> getMotorAddresses() { return gt_addresses; }
    inline void setMotorAddress(int index, int address) { if(index < getMotorAddresses().count()) getMotorAddresses()[index] = address; }
    inline int getMotorAddress(int index) { if(index < getMotorAddresses().count()) return getMotorAddresses()[index]; return 0; }
    inline void setMotorPositionMultiplier(int index, double value) { if(index < getMotorPositionMultipliers().count()) getMotorPositionMultipliers()[index] = value; }
    inline double getMotorPositionMultiplier(int index) { if(index < getMotorPositionMultipliers().count()) return getMotorPositionMultipliers()[index]; return 0.0; }
    inline double getMotorAxisPosition(int index, int axis) { ahp_gt_select_device(getMotorAddress(index)); return ahp_gt_get_position(axis) * getMotorPositionMultiplier(index); }
    inline void setMotorAxisPosition(int index, int axis, double value) { ahp_gt_select_device(getMotorAddress(index)); ahp_gt_set_position(axis, value / getMotorPositionMultiplier(index)); }
    inline void setMotorAxisVelocity(int index, int axis, double speed) { ahp_gt_select_device(getMotorAddress(index)); ahp_gt_start_motion(axis, speed); }
    inline void moveMotorAxisBy(int index, int axis, double value, double speed) { ahp_gt_select_device(getMotorAddress(index)); ahp_gt_goto_relative(axis, value / getMotorPositionMultiplier(index), speed); }
    inline void moveMotorAxisTo(int index, int axis, double value, double speed) { ahp_gt_select_device(getMotorAddress(index)); ahp_gt_goto_absolute(axis, value / getMotorPositionMultiplier(index), speed); }
    inline void stopMotorAxis(int index, int axis) { ahp_gt_select_device(getMotorAddress(index)); ahp_gt_stop_motion(axis); }

private:
    void stopThreads();
    void startThreads();
    int motorFD;
    int gpsFD;
    int xcFD;
    double lastpackettime;
    QMutex vlbi_mutex;
    double Ra, Dec;
    double wavelength;
    void* vlbi_context;
    ahp_xc_packet *packet;
    QSettings *settings;
    QTcpSocket xc_socket;
    QTcpSocket motor_socket;
    QTcpSocket gps_socket;
    QSerialPort serial;
    QFile file;
    Mode mode;
    bool connected;
    double TimeRange;
    Graph *graph;
    Ui::MainWindow *ui;
    double J2000_starttime;

    int gt_address;
    QList<double> position_multipliers;
    QList<int> gt_addresses;
};
#endif // MAINWINDOW_H
