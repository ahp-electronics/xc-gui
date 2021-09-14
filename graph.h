#ifndef GRAPH_H
#define GRAPH_H

#include <QThread>
#include <QWidget>
#include <QChart>
#include <QLabel>
#include <QGroupBox>
#include <QDateTime>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QVBoxLayout>
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
    inline void setPlotWidth(int value) {  plot_w = value; }
    inline void setPlotHeight(int value) { plot_h = value; }
    inline QImage *getRaw() { return &raw; }
    inline QImage *getCoverage() { return &coverage; }
    inline QImage *getIdft() { return &idft; }

private:
    Mode mode;
    inline QImage initGrayPicture(int w, int h) {
        QVector<QRgb> palette;
        QImage image = QImage(w, h, QImage::Format::Format_Grayscale8);
        image.fill(0);
        return image;
    }
    QImage idft;
    QImage raw;
    QImage coverage;
    QGroupBox *correlator;
    QLabel *idftLabel;
    QLabel *rawLabel;
    QLabel *coverageLabel;
    QLabel *idftView;
    QLabel *rawView;
    QLabel *coverageView;
    int plot_w, plot_h;
    QValueAxis *axisX;
    QValueAxis *axisY;
    QChart *chart;
    QChartView *view;

};

#endif // GRAPH_H
