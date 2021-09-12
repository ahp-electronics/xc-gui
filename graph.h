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

    void resizeEvent(QResizeEvent *event) override;
    void paint();

    void clearSeries();
    bool threadRunning;
    void addSeries(QAbstractSeries* series);
    void removeSeries(QAbstractSeries* series);
    void setMode(Mode m);
    inline Mode getMode() { return mode; }
    inline int getPlotWidth() { return plot_w; }
    inline int getPlotHeight() { return plot_h; }
    inline void setPlotWidth(int value) {  plot_w = value; plot = plot.scaled(getPlotWidth(), getPlotHeight()); idft = plot.scaled(getPlotWidth(), getPlotHeight()); }
    inline void setPlotHeight(int value) { plot_h = value; plot = plot.scaled(getPlotWidth(), getPlotHeight()); idft = idft.scaled(getPlotWidth(), getPlotHeight()); }
    inline QImage *getPlot() { return &plot; }
    inline QImage *getIDFT() { return &idft; }

private:
    Mode mode;
    inline QImage initGrayPicture(int w, int h) {
        QVector<QRgb> palette;
        for(int c = 0; c < 256; c++)
            palette.append((QRgb)(c|(c<<8)|(c<<16)));
        QImage image = QImage(w, h, QImage::Format::Format_Indexed8);
        image.setColorTable(palette);
        return image;
    }
    QGraphicsView *PlotView;
    QGraphicsView *IDFTView;
    QGraphicsScene *Plot;
    QGraphicsScene *IDFT;
    QImage plot;
    QImage idft;
    int plot_w, plot_h;
    QValueAxis *axisX;
    QValueAxis *axisY;
    QChart *chart;
    QChartView *view;

};

#endif // GRAPH_H
