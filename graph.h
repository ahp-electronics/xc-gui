#ifndef GRAPH_H
#define GRAPH_H

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

    void addSeries(QLineSeries *series);
    void clearSeries();
    void Update();
    void resizeEvent(QResizeEvent *event);

private:
    QChart *chart;
    QChartView *view;
};

#endif // GRAPH_H
