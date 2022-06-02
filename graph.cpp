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
#include "ui_inputs.h"
#include <cfloat>
#include <QTextFormat>
#include "ahp_xc.h"

Graph::Graph(QSettings *s, QWidget *parent, QString n) :
    QWidget(parent),
    inputs(new Ui::Inputs())
{
    settings = s;
    setAccessibleName("Graph");
    chart = new QChart();
    name = n;
    chart->setVisible(true);
    chart->createDefaultAxes();
    chart->legend()->hide();
    view = new QChartView(chart, this);
    view->setRubberBand(QChartView::HorizontalRubberBand);
    view->setRenderHint(QPainter::Antialiasing);
    correlator = new QGroupBox(this);
    correlator->setVisible(false);
    inputs->setupUi(correlator);
    infos = inputs->infos;
    infos->setParent(correlator);
    coverage = initGrayPicture(getPlotSize(), getPlotSize());
    coverageView = new QLabel(correlator);
    coverageView->setVisible(true);
    magnitude = initGrayPicture(getPlotSize(), getPlotSize());
    magnitudeView = new QLabel(correlator);
    magnitudeView->setVisible(true);
    phase = initGrayPicture(getPlotSize(), getPlotSize());
    phaseView = new QLabel(correlator);
    phaseView->setVisible(true);
    idft = initGrayPicture(getPlotSize(), getPlotSize());
    idftView = new QLabel(correlator);
    idftView->setVisible(true);
    coverageLabel = new QLabel(correlator);
    coverageLabel->setVisible(true);
    coverageLabel->setText("Coverage");
    magnitudeLabel = new QLabel(correlator);
    magnitudeLabel->setVisible(true);
    magnitudeLabel->setText("Magnitude");
    phaseLabel = new QLabel(correlator);
    phaseLabel->setVisible(true);
    phaseLabel->setText("Phase");
    idftLabel = new QLabel(correlator);
    idftLabel->setVisible(true);
    idftLabel->setText("IDFT");
    setPlotSize(256);
    setRaRate(1.0);
    setDecRate(0.0);

    connect(inputs->Ra_0, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Ra_0->setValue(23);
        if(value > 23)
            inputs->Ra_0->setValue(0);
        Ra = fromHMSorDMS(QString::number(inputs->Ra_0->value()) + ":" + QString::number(inputs->Ra_1->value()) + ":" +
                          QString::number(inputs->Ra_2->value()));
        saveSetting("Ra", Ra);
        emit coordinatesUpdated(Ra, Dec, 0);
    });
    connect(inputs->Ra_1, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Ra_1->setValue(59);
        if(value > 59)
            inputs->Ra_1->setValue(0);
        Ra = fromHMSorDMS(QString::number(inputs->Ra_0->value()) + ":" + QString::number(inputs->Ra_1->value()) + ":" +
                          QString::number(inputs->Ra_2->value()));
        saveSetting("Ra", Ra);
        emit coordinatesUpdated(Ra, Dec, 0);
    });
    connect(inputs->Ra_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Ra_2->setValue(59);
        if(value > 59)
            inputs->Ra_2->setValue(0);
        Ra = fromHMSorDMS(QString::number(inputs->Ra_0->value()) + ":" + QString::number(inputs->Ra_1->value()) + ":" +
                          QString::number(inputs->Ra_2->value()));
        saveSetting("Ra", Ra);
        emit coordinatesUpdated(Ra, Dec, 0);
    });
    connect(inputs->Dec_0, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        Dec = fromHMSorDMS(QString::number(inputs->Dec_0->value()) + ":" + QString::number(inputs->Dec_1->value()) + ":" +
                           QString::number(inputs->Dec_2->value()));
        saveSetting("Dec", Dec);
        emit coordinatesUpdated(Ra, Dec, 0);
    });
    connect(inputs->Dec_1, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Dec_1->setValue(59);
        if(value > 59)
            inputs->Dec_1->setValue(0);
        Dec = fromHMSorDMS(QString::number(inputs->Dec_0->value()) + ":" + QString::number(inputs->Dec_1->value()) + ":" +
                           QString::number(inputs->Dec_2->value()));
        saveSetting("Dec", Dec);
        emit coordinatesUpdated(Ra, Dec, 0);
    });
    connect(inputs->Dec_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Dec_2->setValue(59);
        if(value > 59)
            inputs->Dec_2->setValue(0);
        Dec = fromHMSorDMS(QString::number(inputs->Dec_0->value()) + ":" + QString::number(inputs->Dec_1->value()) + ":" +
                           QString::number(inputs->Dec_2->value()));
        saveSetting("Dec", Dec);
        emit coordinatesUpdated(Ra, Dec, 0);
    });
    connect(inputs->Lat_0, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        Latitude = fromHMSorDMS(QString::number(inputs->Lat_0->value()) + ":" + QString::number(
                                    inputs->Lat_1->value()) + ":" + QString::number(inputs->Lat_2->value()));
        saveSetting("Latitude", Latitude);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->Lat_1, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Lat_1->setValue(59);
        if(value > 59)
            inputs->Lat_1->setValue(0);
        Latitude = fromHMSorDMS(QString::number(inputs->Lat_0->value()) + ":" + QString::number(
                                    inputs->Lat_1->value()) + ":" + QString::number(inputs->Lat_2->value()));
        saveSetting("Latitude", Latitude);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->Lat_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Lat_2->setValue(59);
        if(value > 59)
            inputs->Lat_2->setValue(0);
        Latitude = fromHMSorDMS(QString::number(inputs->Lat_0->value()) + ":" + QString::number(
                                    inputs->Lat_1->value()) + ":" + QString::number(inputs->Lat_2->value()));
        saveSetting("Latitude", Latitude);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->Lon_0, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Lon_0->setValue(359);
        if(value > 359)
            inputs->Lon_0->setValue(0);
        Longitude = fromHMSorDMS(QString::number(inputs->Lon_0->value()) + ":" + QString::number(
                                     inputs->Lon_1->value()) + ":" + QString::number(inputs->Lon_2->value()));
        saveSetting("Longitude", Longitude);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->Lon_1, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Lon_1->setValue(59);
        if(value > 59)
            inputs->Lon_1->setValue(0);
        Longitude = fromHMSorDMS(QString::number(inputs->Lon_0->value()) + ":" + QString::number(
                                     inputs->Lon_1->value()) + ":" + QString::number(inputs->Lon_2->value()));
        saveSetting("Longitude", Longitude);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->Lon_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(value < 0)
            inputs->Lon_2->setValue(59);
        if(value > 59)
            inputs->Lon_2->setValue(0);
        Longitude = fromHMSorDMS(QString::number(inputs->Lon_0->value()) + ":" + QString::number(
                                     inputs->Lon_1->value()) + ":" + QString::number(inputs->Lon_2->value()));
        saveSetting("Longitude", Longitude);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->El, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        Elevation = inputs->El->value();
        saveSetting("Elevation", Elevation);
        emit locationUpdated(Latitude, Longitude, Elevation);
    });
    connect(inputs->Frequency, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        setFrequency(inputs->Frequency->value() * 1000000.0);
        saveSetting("Frequency", Frequency);
        emit frequencyUpdated(getFrequency());
    });
    connect(inputs->Goto, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit gotoRaDec(getRa(), getDec());
    });
    connect(inputs->Halt, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit haltMotors();
    });
    connect(inputs->Track, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit startTracking();
    });
}

Graph::~Graph()
{
    chart->~QChart();
    view->~QChartView();
}

void Graph::loadSettings()
{

    setRa(readDouble("Ra", 0.0));
    setDec(readDouble("Dec", 0.0));
    setLatitude(readDouble("Latitude", 0.0));
    setLongitude(readDouble("Longitude", 0.0));
    setElevation(readDouble("Elevation", 0.0));
    setFrequency(readDouble("Frequency", 1420000000.0));

    double* ra = toDms(getRa());
    double* dec = toDms(getDec());
    double* lat = toDms(getLatitude());
    double* lon = toDms(getLongitude());

    inputs->Ra_0->setValue(ra[0]);
    inputs->Dec_0->setValue(dec[0]);
    inputs->Lat_0->setValue(lat[0]);
    inputs->Lon_0->setValue(lon[0]);
    inputs->Ra_1->setValue(ra[1]);
    inputs->Dec_1->setValue(dec[1]);
    inputs->Lat_1->setValue(lat[1]);
    inputs->Lon_1->setValue(lon[1]);
    inputs->Ra_2->setValue(ra[2]);
    inputs->Dec_2->setValue(dec[2]);
    inputs->Lat_2->setValue(lat[2]);
    inputs->Lon_2->setValue(lon[2]);

    inputs->El->setValue(getElevation());
    inputs->Frequency->setValue(getFrequency() / 1000000.0);
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
    dms *= 60000.0;
    s = floor(dms) / 1000.0;
    return QString::number(d) + QString(":") + QString::number(m) + QString(":") + QString::number(s);
}

double* Graph::toDms(double d)
{
    double* dms = (double*)malloc(sizeof(double) * 3);
    dms[0] = floor(d);
    d -= dms[0];
    d *= 60.0;
    dms[1] = floor(d);
    d -= dms[1];
    d *= 60.0;
    dms[2] = d;
    return dms;
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
    s = floor(hms) / 1000.0;
    return QString::number(h) + QString(":") + QString::number(m) + QString(":") + QString::number(s);
}

double Graph::fromHMSorDMS(QString dms)
{
    double d;
    double m;
    double s;
    QStringList deg = dms.split(":");
    d = deg[0].toDouble();
    m = deg[1].toDouble() / 60.0 * (d < 0 ? -1 : 1);
    s = deg[2].toDouble() / 3600.0 * (d < 0 ? -1 : 1);
    return d + m + s;
}

void Graph::updateInfo()
{
    QString label = "";
    label += QString("UTC: ") + QDateTime::currentDateTimeUtc().toString(Qt::DateFormat::ISODate);
    label += "\n";
    label += QString("LST: ") + toHMS(getLST());
    label += "\n";
    label += QString("Latitude: ") + toDMS(getLatitude());
    label += QString(getLatitude() < 0 ? "S" : "N");
    label += "\n";
    label += QString("Longitude: ") + toDMS(getLongitude());
    label += QString(getLongitude() < 0 ? "W" : "E");
    label += "\n";
    label += QString("Elevation: ") + QString::number(getElevation());
    label += "\n";
    label += QString("Altitude: ") + QString(getAltitude() < 0 ? "-" : "+") + toDMS(getAltitude());
    label += "\n";
    label += QString("Azimuth: ") + toDMS(getAzimuth());
    label += "\n";
    label += QString("Right Ascension: ") + toDMS(getRa());
    label += "\n";
    label += QString("Declination: ") + QString(getDec() < 0 ? "-" : "+") + toDMS(getDec());
    label += "\n";
    inputs->infoLabel->setText(label);
}

void Graph::setMode(Mode m)
{
    emit modeChanging(m);
    mode = m;
    if(mode == HolographIQ || mode == HolographII)
    {
        correlator->setVisible(true);
        chart->setVisible(false);
    }
    else
    {
        correlator->setVisible(false);
        chart->setVisible(true);
    }
    emit modeChanged(m);
}

void Graph::addSeries(QAbstractSeries *series)
{
    if(!chart->series().contains(series)) {
        chart->addSeries(series);
        chart->createDefaultAxes();
    }
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

void Graph::setPixmap(QImage *picture, QLabel *view)
{
    lock();
    if(picture != nullptr)
        view->setPixmap(QPixmap::fromImage(picture->scaled(view->geometry().size())));
    unlock();
}

void Graph::plotModel(QImage* picture, char* model)
{
    unsigned char* pixels = (unsigned char*)picture->bits();
    dsp_stream_p stream = vlbi_get_model(getVLBIContext(), model);
    if(stream == nullptr) return;
    lock();
    dsp_stream_p data = dsp_stream_copy(stream);
    dsp_buffer_stretch(data->buf, data->len, 0xff, 0.0);
    dsp_buffer_copy(data->buf, pixels, data->len);
    dsp_stream_free_buffer(data);
    dsp_stream_free(data);
    unlock();
}

void Graph::createModel(QString model)
{
    dsp_stream_p stream = vlbi_get_model(getVLBIContext(), model.toStdString().c_str());
    if(stream == nullptr) {
        stream = dsp_stream_new();
        dsp_stream_add_dim(stream, getPlotSize());
        dsp_stream_add_dim(stream, getPlotSize());
        vlbi_add_model(getVLBIContext(), stream, model.toStdString().c_str());
    }
    dsp_buffer_set(stream->buf, stream->len, 0xff);
}

void Graph::paint()
{
    if(mode == HolographIQ || mode == HolographII)
    {
        setPixmap(getCoverage(), getCoverageView());
        setPixmap(getMagnitude(), getMagnitudeView());
        setPixmap(getPhase(), getPhaseView());
        setPixmap(getIdft(), getIdftView());
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
        if((DBL_MIN == mx && DBL_MAX == mn) || mx == mn)
        {
            mx = M_PI;
            mn = -M_PI;
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
        if((DBL_MIN == mx && DBL_MAX == mn) || mx == mn)
        {
            mx = M_PI;
            mn = -M_PI;
        }
        double diff = mx - mn;
        axis = static_cast<QValueAxis*>(chart->axes()[1]);
        if(axis == nullptr)
            return;
        axis->setRange(mn - diff * 0.2, mx + diff * 0.2);
    }
    update(rect());
}

double Graph::getJ2000Time()
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    timespec ts = vlbi_time_string_to_timespec ((char*)now.toString(Qt::DateFormat::ISODate).toStdString().c_str());
    return vlbi_time_timespec_to_J2000time(ts);
}

double Graph::getLST()
{
    return vlbi_time_J2000time_to_lst(getJ2000Time(), Longitude);
}

double Graph::getAltitude()
{
    double alt, az;
    double ha = vlbi_astro_get_local_hour_angle(getLST(), Ra);
    vlbi_astro_get_alt_az_coordinates(ha, Dec, Latitude, &alt, &az);
    return alt;
}

double Graph::getAzimuth()
{
    double alt, az;
    double ha = vlbi_astro_get_local_hour_angle(getLST(), Ra);
    vlbi_astro_get_alt_az_coordinates(ha, Dec, Latitude, &alt, &az);
    return az;
}

bool Graph::haveSetting(QString setting)
{
    return settings->contains(QString(name + "_" + setting).replace(' ', ""));
}

void Graph::removeSetting(QString setting)
{
    settings->remove(QString(name + "_" + setting).replace(' ', ""));
}

void Graph::saveSetting(QString setting, QVariant value)
{
    settings->setValue(QString(name + "_" + setting).replace(' ', ""), value);
    settings->sync();
}

QVariant Graph::readSetting(QString setting, QVariant defaultValue)
{
    return settings->value(QString(name + "_" + setting).replace(' ', ""), defaultValue);
}

void Graph::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    view->setGeometry(0, 0, width(), height());
    correlator->setGeometry(0, 0, width(), height());
    int y_offset = 40;
    infos->setGeometry(correlator->width() - infos->width() - 10, y_offset, infos->width(), infos->height());
    int num_blocks = 4;
    int size = (correlator->width() - infos->width() - 20 - num_blocks * 5) / num_blocks;
    int n = 0;
    coverageView->setGeometry(size * n + 5 + 5 * n, y_offset + 30, size, size);
    coverageLabel->setGeometry(coverageView->x(), y_offset, size, 30);
    n++;
    magnitudeView->setGeometry(size * n + 5 + 5 * n, y_offset + 30, size, size);
    magnitudeLabel->setGeometry(magnitudeView->x(), y_offset, size, 30);
    n++;
    phaseView->setGeometry(size * n + 5 + 5 * n, y_offset + 30, size, size);
    phaseLabel->setGeometry(phaseView->x(), y_offset, size, 30);
    n++;
    idftView->setGeometry(size * n + 5 + 5 * n, y_offset + 30, size, size);
    idftLabel->setGeometry(idftView->x(), y_offset, size, 30);
    n++;
    y_offset = 40;
}
