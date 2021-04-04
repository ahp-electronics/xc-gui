#include "graph.h"
#include <cfloat>
#include "ahp_xc.h"

Graph::Graph(QWidget *parent, QString name) :
    QWidget(parent)
{
    chart = new QChart();
    chart->setTitle(name);
    chart->setVisible(true);
    chart->legend()->hide();
    view = new QChartView(chart, this);
    view->setRubberBand(QChartView::RectangleRubberBand);
    view->setRenderHint(QPainter::Antialiasing);
    update();
}

Graph::~Graph()
{
    clearSeries();
    chart->~QChart();
    view->~QChartView();
}

void Graph::addSeries(QAbstractSeries *series)
{
    chart->addSeries(series);
    chart->createDefaultAxes();
}

void Graph::clearSeries()
{
    for(int x = 0; x < chart->series().length(); x++)
        delete chart->series()[x];
    chart->series().clear();
    update();
}

void Graph::Update()
{
    double mn = DBL_MAX;
    double mx = DBL_MIN;
    for(int x = 0; x < chart->series().length(); x++) {
        QLineSeries *series = (QLineSeries*)chart->series()[x];
        for(int y = 0; y < series->count(); y++) {
            mn = (mn < series->at(y).x()) ? mn : series->at(y).x();
            mx = (mx > series->at(y).x()) ? mx : series->at(y).x();
        }
    }
    if(DBL_MIN == mx || DBL_MAX == mn) {
        mn = 0;
        mx = 1;
    }
    chart->axes()[0]->setRange(mn, mx);
    mn = DBL_MAX;
    mx = DBL_MIN;
    for(int x = 0; x < chart->series().length(); x++) {
        QLineSeries *series = (QLineSeries*)chart->series()[x];
        for(int y = 0; y < series->count(); y++) {
            mn = (mn < series->at(y).y()) ? mn : series->at(y).y();
            mx = (mx > series->at(y).y()) ? mx : series->at(y).y();
        }
    }
    if(DBL_MIN == mx || DBL_MAX == mn) {
        mn = 0;
        mx = 1;
    }
    double diff = mx-mn;
    chart->axes()[1]->setRange(mn-diff*0.2, mx+diff*0.2);
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
}
