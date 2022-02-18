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
    magnitudeLabel->setText("Magnitude");
    phaseLabel = new QLabel(phaseView);
    phaseLabel->setVisible(true);
    phaseLabel->setText("Phase");
    idftLabel = new QLabel(idftView);
    idftLabel->setVisible(true);
    idftLabel->setText("IDFT");
    infoLabel = new QLabel(correlator);
    infoLabel->setVisible(true);
    editRa = new QLineEdit(correlator);
    editRa->setVisible(true);
    connect(editRa, static_cast<void (QLineEdit::*)(const QString &)>(&QLineEdit::textChanged), [ = ](const QString &text)
    {
        QString str = text;
        str.replace(":", "");
        if(str.length() < 6) {
            str = str.append("000000");
            str.remove(6, str.length()-6);
        }
        editRa->setText(str.remove(2,str.length()-2)+":"+str.remove(0,2).remove(2,str.length()-2)+":"+str.remove(4,str.length()-4));
        Ra = fromHMSorDMS(editRa->text());
    });
    editDec = new QLineEdit(correlator);
    editDec->setVisible(true);
    connect(editDec, static_cast<void (QLineEdit::*)(const QString &)>(&QLineEdit::textChanged), [ = ](const QString &text)
    {
        QString str = text;
        str.replace(":", "");
        if(str.length() < 6) {
            str = str.append("000000");
            str.remove(6, str.length()-6);
        }
        editDec->setText(str.remove(2,str.length()-2)+":"+str.remove(0,2).remove(2,str.length()-2)+":"+str.remove(4,str.length()-4));
        Dec = fromHMSorDMS(editDec->text());
    });
    btnGoto = new QPushButton(correlator);
    btnGoto->setText("Slew");
    btnGoto->setVisible(true);
    connect(btnGoto, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool clicked)
    {
        emit gotoRaDec(Ra, Dec);
    });
    labelRa = new QLabel(correlator);
    labelRa->setVisible(true);
    labelRa->setText("Right Ascension");
    labelDec = new QLabel(correlator);
    labelDec->setVisible(true);
    labelDec->setText("Declination");
    labelGoto = new QLabel(correlator);
    labelGoto->setVisible(true);
    labelGoto->setText("Slew to coordinates");
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
    return QString::number(d) + QString(":") + QString::number(m) + QString(":") + QString::number(s);
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

double Graph::fromHMSorDMS(QString dms)
{
    double d;
    QStringList deg = dms.split(":");
    d = deg[0].toDouble();
    d += deg[1].toDouble() / 60.0;
    d += deg[2].toDouble() / 3600.0;
    return d;
}

void Graph::updateInfo()
{
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
    infoLabel->setText(label);
}

void Graph::setMode(Mode m)
{
    mode = m;
    if(mode == Holograph)
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
    if(mode == Holograph)
    {
        coverageView->setPixmap(QPixmap::fromImage(coverage.scaled(coverageView->geometry().size())));
        magnitudeView->setPixmap(QPixmap::fromImage(magnitude.scaled(magnitudeView->geometry().size())));
        phaseView->setPixmap(QPixmap::fromImage(phase.scaled(phaseView->geometry().size())));
        idftView->setPixmap(QPixmap::fromImage(idft.scaled(idftView->geometry().size())));
        updateInfo();
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

double Graph::getJ2000Time() {
    QDateTime now = QDateTime::currentDateTimeUtc();
    timespec ts = vlbi_time_string_to_timespec ((char*)now.toString(Qt::DateFormat::ISODate).toStdString().c_str());
    return vlbi_time_timespec_to_J2000time(ts);
}

double Graph::getLST() {
    return vlbi_time_J2000time_to_lst(getJ2000Time(), Longitude);
}

double Graph::getAltitude() {
    double alt, az;
    double ha = vlbi_astro_get_local_hour_angle(getLST(), Ra);
    vlbi_astro_get_alt_az_coordinates(ha, Dec, Latitude, &alt, &az);
    return alt;
}

double Graph::getAzimuth() {
    double alt, az;
    double ha = vlbi_astro_get_local_hour_angle(getLST(), Ra);
    vlbi_astro_get_alt_az_coordinates(ha, Dec, Latitude, &alt, &az);
    return az;
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
    correlator->setGeometry(0, 0, width(), height());
    int size = (correlator->width() - 270) / 4;
    int n = 0;
    coverageView->setGeometry   (size * n + 5 + 5 * n, 40, size, size);
    n++;
    magnitudeView->setGeometry  (size * n + 5 + 5 * n, 40, size, size);
    n++;
    phaseView->setGeometry      (size * n + 5 + 5 * n, 40, size, size);
    n++;
    idftView->setGeometry       (size * n + 5 + 5 * n, 40, size, size);
    n++;
    coverageLabel->setGeometry(0,0, size, 30);
    magnitudeLabel->setGeometry(0,0, size, 30);
    phaseLabel->setGeometry(0,0, size, 30);
    idftLabel->setGeometry(0,0, size, 30);
    infoLabel->setGeometry(correlator->width() - 240, 40, 230, 150);
    labelRa->setGeometry(correlator->width() - 240, 200, 140, 23);
    labelDec->setGeometry(correlator->width() - 240, 225, 140, 23);
    labelGoto->setGeometry(correlator->width() - 240, 250, 140, 23);
    editRa->setGeometry(correlator->width() - 90, 200, 75, 23);
    editDec->setGeometry(correlator->width() - 90, 225, 75, 23);
    btnGoto->setGeometry(correlator->width() - 90, 250, 75, 23);
}
