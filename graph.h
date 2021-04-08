#ifndef GRAPH_H
#define GRAPH_H

#include <QThread>
#include <QWidget>
#include <QChart>
#include <QDateTime>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include "threads.h"

using namespace QtCharts;

namespace Ui {
class Graph;
}

class Graph : public QWidget
{
    Q_OBJECT

public:
    Graph(QWidget *parent = nullptr, QString name="");
    ~Graph();

    void addSeries(QAbstractSeries *series);
    void clearSeries();
    void resizeEvent(QResizeEvent *event);
    bool threadRunning;
    void Update();

private:
    QValueAxis *axisX;
    QValueAxis *axisY;
    QChart *chart;
    QChartView *view;

};

#endif // GRAPH_H
