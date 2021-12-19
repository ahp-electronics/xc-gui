/*
    MIT License

    libahp_xc library to drive the AHP XC correlators
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

#include "graph.h"
#include <cfloat>
#include <QTextFormat>
#include "ahp_xc.h"

Graph::Graph(QWidget *parent, QString name) :
    QWidget(parent)
{
    plot_w = 1024;
    plot_h = 1024;
    setAccessibleName("Graph");
    chart = new QChart();
    chart->setTitle(name);
    chart->setVisible(true);
    chart->createDefaultAxes();
    chart->legend()->hide();
    view = new QChartView(chart, this);
    view->setRubberBand(QChartView::HorizontalRubberBand);
    view->setRenderHint(QPainter::Antialiasing);
    correlator = new QGroupBox(this);
    correlator->setVisible(false);
    coverage = initGrayPicture(getPlotWidth(), getPlotHeight());
    coverageView = new QLabel(correlator);
    coverageView->setVisible(true);
    raw = initGrayPicture(getPlotWidth(), getPlotHeight());
    rawView = new QLabel(correlator);
    rawView->setVisible(true);
    idft = initGrayPicture(getPlotWidth(), getPlotHeight());
    idftView = new QLabel(correlator);
    idftView->setVisible(true);
    QPalette palette = coverageView->palette();
    palette.setColor(correlator->foregroundRole(), Qt::white);
    coverageLabel = new QLabel(coverageView);
    coverageLabel->setVisible(true);
    coverageLabel->setPalette(palette);
    coverageLabel->setText("Coverage");
    rawLabel = new QLabel(rawView);
    rawLabel->setVisible(true);
    rawLabel->setPalette(palette);
    rawLabel->setText("Raw");
    idftLabel = new QLabel(idftView);
    idftLabel->setVisible(true);
    idftLabel->setPalette(palette);
    idftLabel->setText("IDFT");
}

Graph::~Graph()
{
    chart->~QChart();
    view->~QChartView();
}

void Graph::setMode(Mode m)
{
    mode = m;
    if(mode == Crosscorrelator)
    {
        correlator->setVisible(true);
        chart->setVisible(false);
    }
    else
    {
        correlator->setVisible(false);
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

void Graph::paint()
{
    if(mode == Crosscorrelator)
    {
        coverageView->setPixmap(QPixmap::fromImage(coverage.scaled(coverageView->geometry().size())));
        rawView->setPixmap(QPixmap::fromImage(raw.scaled(rawView->geometry().size())));
        idftView->setPixmap(QPixmap::fromImage(idft.scaled(idftView->geometry().size())));
    }
    else
    {
        if(chart == nullptr)
            return;
        if(chart->series().length() == 0)
            return;
        double mn = DBL_MAX;
        double mx = DBL_MIN;
        for(int x = 0; x < chart->series().length(); x++)
        {
            QLineSeries *series = (QLineSeries*)chart->series()[x];
            if(series->count() == 0)
                continue;
            for(int y = 0; y < series->count(); y++)
            {
                mn = (mn < series->at(y).x()) ? mn : series->at(y).x();
                mx = (mx > series->at(y).x()) ? mx : series->at(y).x();
            }
        }
        if(DBL_MIN == mx && DBL_MAX == mn)
        {
            mx = 1;
            mn = 0;
        }
        if(chart->axes().count() < 2)
            return;
        QValueAxis* axis = static_cast<QValueAxis*>(chart->axes()[0]);
        if(axis == nullptr)
            return;
        axis->setRange(mn, mx);
        mn = DBL_MAX;
        mx = DBL_MIN;
        for(int x = 0; x < chart->series().length(); x++)
        {
            QLineSeries *series = (QLineSeries*)chart->series()[x];
            if(series->count() == 0)
                continue;
            for(int y = 0; y < series->count(); y++)
            {
                mn = (mn < series->at(y).y()) ? mn : series->at(y).y();
                mx = (mx > series->at(y).y()) ? mx : series->at(y).y();
            }
        }
        if(DBL_MIN == mx && DBL_MAX == mn)
        {
            mx = 1;
            mn = 0;
        }
        double diff = mx - mn;
        axis = static_cast<QValueAxis*>(chart->axes()[1]);
        if(axis == nullptr)
            return;
        axis->setRange(mn - diff * 0.2, mx + diff * 0.2);
    }
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
    correlator->setGeometry(0, 0, width(), height());
    int size = correlator->width() / 3 - 25;
    coverageView->setGeometry(5, 40, size, size);
    idftView->setGeometry(correlator->width() / 3  + 10, 40, size, size);
    rawView->setGeometry(correlator->width() * 2 / 3 + 15, 40, size, size);
    coverageLabel->setGeometry(0, 0, size, 30);
    rawLabel->setGeometry(0, 0, size, 30);
    idftLabel->setGeometry(0, 0, size, 30);
}
