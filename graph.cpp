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
    magnitude = initGrayPicture(getPlotWidth(), getPlotHeight());
    magnitudeView = new QLabel(correlator);
    magnitudeView->setVisible(true);
    phase = initGrayPicture(getPlotWidth(), getPlotHeight());
    phaseView = new QLabel(correlator);
    phaseView->setVisible(true);
    idft = initGrayPicture(getPlotWidth(), getPlotHeight());
    idftView = new QLabel(correlator);
    idftView->setVisible(true);
    coverageLabel = new QLabel(coverageView);
    coverageLabel->setVisible(true);
    coverageLabel->setText("Coverage");
    magnitudeLabel = new QLabel(magnitudeView);
    magnitudeLabel->setVisible(true);
    magnitudeLabel->setText("Raw");
    phaseLabel = new QLabel(phaseView);
    phaseLabel->setVisible(true);
    phaseLabel->setText("Raw");
    idftLabel = new QLabel(idftView);
    idftLabel->setVisible(true);
    idftLabel->setText("IDFT");
    infoLabel = new QLabel(correlator);
    idftLabel->setVisible(true);
}

Graph::~Graph()
{
    chart->~QChart();
    view->~QChartView();
}

QString Graph::toDMS(double dms)
{
    double d, m, s;
    dms = fabs(dms);
    d = floor(dms);
    dms -= d;
    dms *= 60.0;
    m = floor(dms);
    dms -= m;
    dms *= 60.0;
    s = floor(dms)/1000.0;
    return QString::number(d) + QString("°") + QString::number(m) + QString("'") + QString::number(s) + QString("\"");
}

QString Graph::toHMS(double hms)
{
    double h, m, s;
    hms = fabs(hms);
    h = floor(hms);
    hms -= h;
    hms *= 60.0;
    m = floor(hms);
    hms -= m;
    hms *= 60000.0;
    s = floor(hms)/1000.0;
    return QString::number(h) + QString(":") + QString::number(m) + QString(":") + QString::number(s);
}

void Graph::updateInfo()
{/*
    if(getGnssHandle() > -1) {
        int32_t latitude, longitude, elevation;
        uGnssPosGet(getGnssHandle(), &latitude, &longitude, &elevation, NULL, NULL, NULL, NULL, NULL);
        Latitude = (double)latitude / 10000000.0;
        Longitude = (double)longitude / 10000000.0;
        Elevation = (double)elevation / 10000000.0;
    }
    QString label = "";
    label += QString("UTC: ") + QDateTime::currentDateTimeUtc().toString(Qt::DateFormat::ISODate);
    label += "\n";
    label += QString("Latitude: ") + toDMS(Latitude);
    label += QString(Latitude < 0 ? "S" : "N");
    label += "\n";
    label += QString("Longitude: ") + toDMS(Longitude);
    label += QString(Longitude < 0 ? "W" : "E");
    label += "\n";
    label += QString("Elevation: ") + Elevation;
    label += "\n";
    label += QString("Altitude: ") + QString(getAltitude() < 0 ? "-" : "+") + toDMS(getAltitude());
    label += "\n";
    label += QString("Azimuth: ") + toDMS(getAzimuth());
    label += "\n";
    label += QString("LST: ") + toHMS(getLST());
    label += "\n";
    infoLabel->setText(label);*/
}

bool Graph::initGPS()
{/*
    if(!uGnssInit()) {
        setGnssHandle(uGnssAdd(U_GNSS_MODULE_TYPE_M8, U_GNSS_TRANSPORT_NMEA_UART, getGnssPortHandle(), -1, false));
        uGnssCfgSetDynamic(getGnssHandle(), U_GNSS_DYNAMIC_STATIONARY);
        uGnssCfgSetFixMode(getGnssHandle(), U_GNSS_FIX_MODE_3D);
        union {
            struct {
                unsigned char Header[2];
                unsigned char Class;
                unsigned char Id;
                unsigned short Length;
                unsigned char headers;
                unsigned char tpIdx;
                unsigned char version;
                unsigned char reserved1[2];
                short antCableDelay;
                short rfGroupDelay;
                unsigned int freqPeriod;
                unsigned int freqPeriodLock;
                unsigned int pulseLenRatio;
                unsigned int pulseLenRatioLock;
                int userConfigDelay;
                unsigned int flags;
                unsigned char Checksum[2];
            } msg;
            struct {
                unsigned char Header[2];
                unsigned char Payload[36];
                unsigned char Checksum[2];
            } blocks;
        } cmd;
        cmd.msg.Header[0] = 0xB5;
        cmd.msg.Header[1] = 0x62;
        cmd.msg.Class = 0x06;
        cmd.msg.Id = 0x31;
        cmd.msg.Length = 32;
        cmd.msg.tpIdx = 0;
        cmd.msg.version = 0;
        cmd.msg.freqPeriod = 1;
        cmd.msg.freqPeriodLock = 10000000;
        cmd.msg.pulseLenRatio = 0;
        cmd.msg.pulseLenRatioLock = 0;
        cmd.msg.userConfigDelay = 0;
        cmd.msg.flags = 0x3f;
        cmd.blocks.Checksum[0] = 0, cmd.blocks.Checksum[1] = 0;
        for(int i=0;i<36;i++)
        {
            cmd.blocks.Checksum[0] = cmd.blocks.Checksum[0] + cmd.blocks.Payload[i];
            cmd.blocks.Checksum[1] = cmd.blocks.Checksum[1] + cmd.blocks.Checksum[0];
        }
        if(uGnssUtilUbxTransparentSendReceive(getGnssHandle(), (char*)&cmd, sizeof(cmd), (char*)&cmd, sizeof(cmd)) > -1)
            return true;
    }*/
    return false;
}

void Graph::deinitGPS()
{/*
    uGnssRemove(getGnssHandle());
    uGnssDeinit();*/
}

void Graph::setMode(Mode m)
{
    mode = m;
    if(mode == Interferometer)
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
    if(mode == Interferometer)
    {
        coverageView->setPixmap(QPixmap::fromImage(coverage.scaled(coverageView->geometry().size())));
        magnitudeView->setPixmap(QPixmap::fromImage(magnitude.scaled(magnitudeView->geometry().size())));
        phaseView->setPixmap(QPixmap::fromImage(phase.scaled(phaseView->geometry().size())));
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
    update(rect());
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
    correlator->setGeometry(0, 0, width(), height());
    int size = (correlator->width() - 260) / 3;
    coverageView->setGeometry(5, 40, size, size);
    magnitudeView->setGeometry(size + 10, 40, size, size);
    phaseView->setGeometry(size + 10, 40, size, size);
    idftView->setGeometry(size * 2 + 15, 40, size, size);
    coverageLabel->setGeometry(0,0, size, 30);
    magnitudeLabel->setGeometry(0,0, size, 30);
    phaseLabel->setGeometry(0,0, size, 30);
    idftLabel->setGeometry(0,0, size, 30);
    infoLabel->setGeometry(correlator->width() - 240, 40, 230, 150);
}
