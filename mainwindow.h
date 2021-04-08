#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QThread>
#include <QMainWindow>
#include <QTcpSocket>
#include <QSettings>
#include <ahp_xc.h>
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
    inline double getTimeRange() { return TimeRange; }
    inline ahp_xc_packet * createPacket() {  packet = ahp_xc_alloc_packet(); return packet; }
    inline ahp_xc_packet * getPacket() { return packet; }
    inline void freePacket() { ahp_xc_free_packet(packet); }
    inline Graph *getGraph() { return graph; }
    inline Mode getMode() { return mode; }
    inline void setMode(Mode m) { mode = m; if(socket.isOpen()||file.isOpen()){ if(mode == Counter) { start = QDateTime::currentDateTimeUtc(); ahp_xc_enable_capture(true); } else ahp_xc_enable_capture(false); } }
    QDateTime start;
    progressThread *readThread;
    progressThread *uiThread;
private:
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
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
