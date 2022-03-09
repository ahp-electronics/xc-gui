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

#include <QSettings>
#include <QThread>
#include <QWidget>
#include <QChart>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QDateTime>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QVBoxLayout>
#include "types.h"

using namespace QtCharts;

namespace Ui
{
class Graph;
class Inputs;
}

class Graph : public QWidget
{
        Q_OBJECT

    public:
        Graph(QSettings *s, QWidget *parent = nullptr, QString name = "Graph");
        ~Graph();

        void resizeEvent(QResizeEvent *event) override;
        void paint();

        bool haveSetting(QString setting);
        void removeSetting(QString setting);
        void saveSetting(QString name, QVariant value);
        QVariant readSetting(QString name, QVariant defaultValue);
        inline QString readString(QString setting, QString defaultValue)
        {
            return readSetting(setting, defaultValue).toString();
        }
        inline double readDouble(QString setting, double defaultValue)
        {
            return readSetting(setting, defaultValue).toDouble();
        }
        inline int readInt(QString setting, int defaultValue)
        {
            return readSetting(setting, defaultValue).toInt();
        }
        inline bool readBool(QString setting, bool defaultValue)
        {
            return readSetting(setting, defaultValue).toBool();
        }

        void updateInfo();
        void clearSeries();
        bool threadRunning;
        void addSeries(QAbstractSeries* series);
        void removeSeries(QAbstractSeries* series);
        void setMode(Mode m);
        inline Mode getMode()
        {
            return mode;
        }
        inline int getPlotSize()
        {
            return plot_w;
        }
        inline void setPlotSize(int value)
        {
            plot_w = value;
            idft = idft.scaled(plot_w, plot_w);
            coverage = coverage.scaled(plot_w, plot_w);
            magnitude = coverage.scaled(plot_w, plot_w);
            phase = phase.scaled(plot_w, plot_w);
        }
        inline void setPlotHeight(int value)
        {
            plot_w = value;
        }
        inline QImage *getMagnitude()
        {
            return &magnitude;
        }
        inline QImage *getPhase()
        {
            return &phase;
        }
        inline QImage *getCoverage()
        {
            return &coverage;
        }
        inline QImage *getIdft()
        {
            return &idft;
        }
        inline int getMotorFD()
        {
            return motorFD;
        }
        inline void setMotorFD(int fd)
        {
            motorFD = fd;
        }
        inline int getControlFD()
        {
            return controlFD;
        }
        inline void setControlFD(int fd)
        {
            controlFD = fd;
        }

        double getJ2000Time();
        double getLST();
        double getAltitude();
        double getAzimuth();

        inline double getFrequency()
        {
            return Frequency;
        }
        inline double getRa()
        {
            return Ra;
        }
        inline double getDec()
        {
            return Dec;
        }
        inline double getLatitude()
        {
            return Latitude;
        }
        inline double getLongitude()
        {
            return Longitude;
        }
        inline double getElevation()
        {
            return Elevation;
        }

        inline void setFrequency(double freq)
        {
            Frequency = freq;
        }
        inline void setRa(double ra)
        {
            Ra = ra;
        }
        inline void setDec(double dec)
        {
            Dec = dec;
        }
        inline void setLatitude(double lat)
        {
            Latitude = lat;
        }
        inline void setLongitude(double lon)
        {
            Longitude = lon;
        }
        inline void setElevation(double el)
        {
            Elevation = el;
        }
        void loadSettings();

        double* toDms(double d);
        QString toHMS(double hms);
        QString toDMS(double dms);
        double fromHMSorDMS(QString dms);

    private:
        Ui::Inputs *inputs;
        Mode mode { Counter };
        inline QImage initGrayPicture(int w, int h)
        {
            QVector<QRgb> palette;
            QImage image = QImage(w, h, QImage::Format::Format_Grayscale8);
            image.fill(0);
            return image;
        }
        double Latitude, Longitude, Elevation;
        double Ra, Dec;
        double Frequency;
        QGroupBox *infos;
        QImage idft;
        QImage magnitude;
        QImage phase;
        QImage coverage;
        QGroupBox *correlator;
        QLabel *idftLabel;
        QLabel *magnitudeLabel;
        QLabel *phaseLabel;
        QLabel *coverageLabel;
        QLabel *idftView;
        QLabel *magnitudeView;
        QLabel *phaseView;
        QLabel *coverageView;
        int plot_w { 256 };
        QValueAxis *axisX;
        QValueAxis *axisY;
        QChart *chart;
        QChartView *view;
        QString name;
        int motorFD;
        int controlFD;
        QSettings *settings;

    signals:
        void frequencyUpdated(double);
        void locationUpdated(double, double, double);
        void coordinatesUpdated(double, double, double);
        void connectMotors();
        void disconnectMotors();
        void gotoRaDec(double, double);

};

#endif // GRAPH_H
