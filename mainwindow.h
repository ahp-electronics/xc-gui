#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <thread>
#include <QThread>
#include <QMainWindow>
#include <ahp_xc.h>
#include "graph.h"
#include "line.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    int percent, finished;
    bool threadsRunning;
    void resizeEvent(QResizeEvent* event);
    QList<Line*> Lines;
    inline double getTimeRange() { return TimeRange; }
    inline ahp_xc_packet * createPacket() { ahp_xc_packet *packet = ahp_xc_alloc_packet(); return packet; }
    inline void freePacket(ahp_xc_packet *packet) { ahp_xc_free_packet(packet); }
    inline Graph *getGraph() { return graph; }

private:
    bool connected;
    std::thread readThread;
    double TimeRange;
    Graph *graph;
    static void ReadThread(MainWindow *wnd);
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
