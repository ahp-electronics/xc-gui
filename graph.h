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
#include <QVBoxLayout>/*
#include <u_error_common.h>
#include <u_gnss_type.h>
#include <u_gnss_module_type.h>
#include <u_gnss_util.h>
#include <u_gnss_cfg.h>
#include <u_gnss_pos.h>
#include <u_gnss.h>*/
#include "types.h"

using namespace QtCharts;

namespace Ui
{
class Graph;
}

class Graph : public QWidget
{
        Q_OBJECT

    public:
        Graph(QWidget *parent = nullptr, QString name = "");
        ~Graph();

        void resizeEvent(QResizeEvent *event) override;
        void paint();

        bool initGPS();
        void deinitGPS();
        void updateInfo();
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
        inline QImage *getMagnitude() { return &magnitude; }
        inline QImage *getPhase() { return &phase; }
        inline QImage *getCoverage() { return &coverage; }
        inline QImage *getIdft() { return &idft; }
        //inline void setGnssPortFD(int fd) { gnssFD.uart = fd; }
        //inline int getGnssPortFD() { return gnssFD.uart; }
        //inline uGnssTransportHandle_t getGnssPortHandle() { return gnssFD; }
        inline void setGnssHandle(int handle) { gnssHandle = handle; }
        inline int getGnssHandle() { return gnssHandle; }

        double getJ2000Time();
        double getLST();
        double getAltitude();
        double getAzimuth();
        inline void setRa(double ra) { Ra = ra; }
        inline void setDec(double dec) { Dec = dec; }

        QString toHMS(double hms);
        QString toDMS(double dms);

    private:
        //uGnssTransportHandle_t gnssFD { .uart = -1 };
        int gnssHandle { -1 };
        Mode mode { Counter };
        inline QImage initGrayPicture(int w, int h) {
            QVector<QRgb> palette;
            QImage image = QImage(w, h, QImage::Format::Format_Grayscale8);
            image.fill(255);
            return image;
        }
        double Longitude, Latitude, Elevation;
        double Ra, Dec;
        QImage idft;
        QImage magnitude;
        QImage phase;
        QImage coverage;
        QGroupBox *correlator;
        QLabel *infoLabel;
        QLabel *idftLabel;
        QLabel *magnitudeLabel;
        QLabel *phaseLabel;
        QLabel *coverageLabel;
        QLabel *idftView;
        QLabel *magnitudeView;
        QLabel *phaseView;
        QLabel *coverageView;
        int plot_w, plot_h;
        QValueAxis *axisX;
        QValueAxis *axisY;
        QChart *chart;
        QChartView *view;

};

#endif // GRAPH_H
