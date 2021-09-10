#include "graph.h"
#include <cfloat>
#include "ahp_xc.h"

Graph::Graph(QWidget *parent, QString name) :
    QWidget(parent)
{
    setAccessibleName("Graph");
    chart = new QChart();
    chart->setTitle(name);
    chart->setVisible(true);
    chart->createDefaultAxes();
    chart->legend()->hide();
    view = new QChartView(chart, this);
    view->setRubberBand(QChartView::HorizontalRubberBand);
    view->setRenderHint(QPainter::Antialiasing);
    plot_w = 512;
    plot_h = 512;
    plot = initGrayPicture(getPlotWidth(), getPlotHeight());
    idft = initGrayPicture(getPlotWidth(), getPlotHeight());
    PlotView = new QGraphicsView(this);
    Plot = new QGraphicsScene(this);
    Plot->addPixmap(QPixmap::fromImage(plot));
    Plot->setSceneRect(plot.rect());
    PlotView->setVisible(false);
    PlotView->setScene(Plot);
    IDFTView = new QGraphicsView(this);
    IDFT = new QGraphicsScene(this);
    IDFT->addPixmap(QPixmap::fromImage(idft));
    IDFT->setSceneRect(idft.rect());
    IDFTView->setVisible(false);
    IDFTView->setScene(IDFT);
}

Graph::~Graph()
{
    chart->~QChart();
    view->~QChartView();
}

void Graph::setMode(Mode m)
{
    mode = m;
    if(mode == Crosscorrelator) {
        PlotView->setVisible(true);
        IDFTView->setVisible(true);
        chart->setVisible(false);
    } else {
        PlotView->setVisible(false);
        IDFTView->setVisible(false);
        chart->setVisible(true);
    }
}

void Graph::addSeries(QAbstractSeries *series)
{
    if(!chart->series().contains(series))
        chart->addSeries(series);
    chart->createDefaultAxes();
}

void Graph::removeSeries(QAbstractSeries *series)
{
    if(chart->series().contains(series))
        chart->removeSeries(series);
}

void Graph::clearSeries()
{
    chart->series().clear();
}

void Graph::Update()
{
    PlotView->update();
    IDFTView->update();
    if(chart == nullptr)
        return;
    if(chart->series().length() == 0)
        return;
    double mn = DBL_MAX;
    double mx = DBL_MIN;
    for(int x = 0; x < chart->series().length(); x++) {
        QLineSeries *series = (QLineSeries*)chart->series()[x];
        if(series->count() == 0)
            continue;
        for(int y = 0; y < series->count(); y++) {
            mn = (mn < series->at(y).x()) ? mn : series->at(y).x();
            mx = (mx > series->at(y).x()) ? mx : series->at(y).x();
        }
    }
    if(DBL_MIN == mx && DBL_MAX == mn) {
        mx = 1;
        mn = 0;
    }
    if(chart->axes().count()<2)
        return;
    QValueAxis* axis = static_cast<QValueAxis*>(chart->axes()[0]);
    if(axis == nullptr)
        return;
    axis->setRange(mn, mx);
    mn = DBL_MAX;
    mx = DBL_MIN;
    for(int x = 0; x < chart->series().length(); x++) {
        QLineSeries *series = (QLineSeries*)chart->series()[x];
        for(int y = 0; y < series->count(); y++) {
            mn = (mn < series->at(y).y()) ? mn : series->at(y).y();
            mx = (mx > series->at(y).y()) ? mx : series->at(y).y();
        }
    }
    if(DBL_MIN == mx && DBL_MAX == mn) {
        mx = 1;
        mn = 0;
    }
    double diff = mx-mn;
    axis = static_cast<QValueAxis*>(chart->axes()[1]);
    if(axis == nullptr)
        return;
    axis->setRange(mn-diff*0.2, mx+diff*0.2);
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
    PlotView->setGeometry(5, 5, height() - 10, height() - 10);
    IDFTView->setGeometry(width()/2 + 5, 5, height() - 10, height() - 10);
}
