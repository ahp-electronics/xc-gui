#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QThread>
#include <QMainWindow>
#include <QTcpSocket>
#include <QSettings>
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
    inline double getFrequency() { return 1.0/wavelength; }
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
    inline QList<int> getGTAddresses() { return gt_addresses; }
    inline void * getVLBIContext() { return vlbi_context; }
    inline void setMode(Mode m) {
        mode = m;
        if(connected){
            if(mode == Counter) {
                start = QDateTime::currentDateTimeUtc();
                ahp_xc_set_capture_flag(CAP_ENABLE);
            } else {
                ahp_xc_clear_capture_flag(CAP_ENABLE);
            }
        }
    }
    QDateTime start;
    progressThread *readThread;
    progressThread *uiThread;

private:
    double Ra, Dec;
    double wavelength;
    void *vlbi_context;
    ahp_xc_packet *packet;
    QSettings *settings;
    QTcpSocket socket;
    QFile file;
    Mode mode;
    bool connected;
    double TimeRange;
    Graph *graph;
    static void ReadThread(QWidget *wnd);
    static void UiThread(QWidget *wnd);
    static void VLBIThread(QWidget *sender);
    static void GTThread(QWidget *sender);
    QList<int> gt_addresses;
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
