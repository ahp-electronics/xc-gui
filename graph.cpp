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
    coverageLabel = new QLabel(coverageView);
    coverageLabel->setVisible(true);
    coverageLabel->setText("Coverage");
    rawLabel = new QLabel(rawView);
    rawLabel->setVisible(true);
    rawLabel->setText("Raw");
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
{
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
    infoLabel->setText(label);
}

bool Graph::initGPS()
{
    if(!uGnssInit()) {
        setGnssHandle(uGnssAdd(U_GNSS_MODULE_TYPE_M8, U_GNSS_TRANSPORT_NMEA_UART, getGnssPortHandle(), -1, false));
        return true;
    }
    return false;
}

void Graph::deinitGPS()
{
    uGnssRemove(getGnssHandle());
    uGnssDeinit();
}

void Graph::setMode(Mode m)
{
    mode = m;
    if(mode == Crosscorrelator) {
        correlator->setVisible(true);
        chart->setVisible(false);
    } else {
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
    if(mode == Crosscorrelator) {
        updateInfo();
        coverageView->setPixmap(QPixmap::fromImage(coverage.scaled(coverageView->geometry().size())));
        rawView->setPixmap(QPixmap::fromImage(raw.scaled(rawView->geometry().size())));
        idftView->setPixmap(QPixmap::fromImage(idft.scaled(idftView->geometry().size())));
    } else {
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
            if(series->count() == 0)
                continue;
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
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
    correlator->setGeometry(0, 0, width(), height());
    int size = (correlator->width() - 260) / 3;
    coverageView->setGeometry(5, 40, size, size);
    idftView->setGeometry(size + 10, 40, size, size);
    rawView->setGeometry(size * 2 + 15, 40, size, size);
    coverageLabel->setGeometry(0,0, size, 30);
    rawLabel->setGeometry(0,0, size, 30);
    idftLabel->setGeometry(0,0, size, 30);
    infoLabel->setGeometry(correlator->width() - 240, 40, 230, 150);
}
