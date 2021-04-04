#ifndef GRAPH_H
#define GRAPH_H

#include <QThread>
#include <QWidget>
#include <QChart>
#include <QDateTime>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>

using namespace QtCharts;

class Graph : public QWidget
{
    Q_OBJECT

public:
    explicit Graph(QWidget *parent = nullptr, QString name="");
    ~Graph();

    void addSeries(QAbstractSeries *series);
    void clearSeries();
    void Update();
    void resizeEvent(QResizeEvent *event);
    static void RunThread(Graph *wnd);
    bool threadRunning;
private:
    std::thread runThread;
    QValueAxis *axisX;
    QValueAxis *axisY;
    QChart *chart;
    QChartView *view;
};

#endif // GRAPH_H
