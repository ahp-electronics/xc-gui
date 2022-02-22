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

#include "types.h"
#include <cmath>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <QIODevice>
#include <QStandardPaths>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QTcpSocket>
#include <QThreadPool>
#include <QDir>
#include <fcntl.h>
#include "mainwindow.h"
#include "./ui_mainwindow.h"

static double coverage_delegate(double x, double y) {
    (void)x;
    (void)y;
    return 1.0;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setAccessibleName("MainWindow");
    QString homedir = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).at(0);
    QString ini = homedir + "/settings.ini";
    if(!QDir(homedir).exists())
    {
        QDir().mkdir(homedir);
    }
    if(!QFile(ini).exists())
    {
        QFile *f = new QFile(ini);
        f->open(QIODevice::WriteOnly);
        f->close();
        f->~QFile();
    }
    settings = new QSettings(ini, QSettings::Format::IniFormat);
    connected = false;
    TimeRange = 10;
    ui->setupUi(this);
    uiThread = new Thread(this);
    readThread = new Thread(this, 100, 100);
    vlbiThread = new Thread(this, 100, 777);
    motorThread = new Thread(this, 1000, 1000);
    Elemental::loadCatalog();
    graph = new Graph(settings, this);
    int starty = 35 + ui->XCPort->y() + ui->XCPort->height();
    ui->Lines->setGeometry(5, starty + 5, this->width() - 10, ui->Lines->height());
    starty += 5 + ui->Lines->height();
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10);
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10);
    getGraph()->setUpdatesEnabled(true);
    getGraph()->setVisible(true);

    settings->beginGroup("Connection");
    ui->XCPort->clear();
    ui->XCPort->addItem(settings->value("xc_connection", "no connection").toString());
    ui->MotorPort->clear();
    ui->MotorPort->addItem(settings->value("motor_connection", "no connection").toString());
    QList<QSerialPortInfo> devices = QSerialPortInfo::availablePorts();
    settings->endGroup();
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.length(); i++)
    {
        QString portname =
#ifndef _WIN32
            "/dev/" +
#endif
            ports[i].portName();
        if(portname != ui->XCPort->itemText(0))
            ui->XCPort->addItem(portname);
        if(portname != ui->MotorPort->itemText(0))
            ui->MotorPort->addItem(portname);
    }
    if(ui->XCPort->itemText(0) != "no connection")
        ui->XCPort->addItem("no connection");
    if(ui->MotorPort->itemText(0) != "no connection")
        ui->MotorPort->addItem("no connection");
    ui->XCPort->setCurrentIndex(0);
    ui->MotorPort->setCurrentIndex(0);
    connect(ui->Mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [ = ](int index)
    {
        setMode((Mode)index);
    });
    connect(ui->Range, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [ = ](int value)
    {
        settings->setValue("Timerange", value);
        TimeRange = ui->Range->value();
    });
    connect(ui->Voltage, static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
            [ = ](int value)
    {
        unsigned char v = value | 0x80;
        ui->voltageLabel->setText("Voltage: " + QString::number(value * 150 / 127) + " V~");
        settings->setValue("Voltage", value);
        write(motorFD, &v, 1);
    });
    connect(ui->Scale, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [ = ](int value)
    {
        settings->setValue("Timescale", value);
        ahp_xc_set_frequency_divider((char)value);
    });
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        (void)checked;
        for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
        {
            Lines[l]->setActive(false);
        }
        stopThreads();
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->Scale->setEnabled(false);
        ui->Range->setEnabled(false);
        ui->Mode->setEnabled(false);
        if(!connected)
            return;
        freePacket();
        vlbi_exit(getVLBIContext());
        for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
        {
            Lines[l]->~Line();
        }
        Lines.clear();
        getGraph()->clearSeries();
        if(xc_socket.isOpen())
        {
            xc_socket.disconnectFromHost();
        }
        if(motor_socket.isOpen())
        {
            motor_socket.disconnectFromHost();
        }
        if(gps_socket.isOpen())
        {
            gps_socket.disconnectFromHost();
        }
        ahp_xc_disconnect();
        settings->endGroup();
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        (void)checked;
        int port = 5760;
        QString address = "localhost";
        QString xcport, motorport, gpsport;
        xcport = ui->XCPort->currentText();
        motorport = ui->MotorPort->currentText();
        settings->beginGroup("Connection");
        motorFD = -1;
        if(motorport == "no connection")
        {
            settings->setValue("motor_connection", motorport);
        }
        else
        {
            if(motorport.contains(':'))
            {
                address = motorport.split(":")[0];
                port = motorport.split(":")[1].toInt();
                ui->Connect->setEnabled(false);
                update();
                motor_socket.connectToHost(address, port);
                motor_socket.waitForConnected();
                if(motor_socket.isValid())
                {
                    motor_socket.setReadBufferSize(4096);
                    motorFD = motor_socket.socketDescriptor();
                    if(motorFD != -1)
                    {
                        getGraph()->setMotorFD(motorFD);
                        if(!ahp_gt_connect_fd(getGraph()->getMotorFD()))
                            settings->setValue("motor_connection", motorport);
                    }
                }
                else
                {
                    ui->Connect->setEnabled(true);
                    update();
                }
            }
            else
            {
                ahp_gt_connect(motorport.toUtf8());
                motorFD = ahp_gt_get_fd();
                if(motorFD == -1)
                    motorFD = open(motorport.toUtf8(), O_RDWR);
                if(motorFD != -1)
                    settings->setValue("motor_connection", motorport);
            }
        }
        xcFD = -1;
        if(xcport == "no connection")
        {
            settings->setValue("xc_connection", xcport);
        }
        else
        {
            if(xcport.contains(':'))
            {
                address = xcport.split(":")[0];
                port = xcport.split(":")[1].toInt();
                ui->Connect->setEnabled(false);
                update();
                xc_socket.connectToHost(address, port);
                xc_socket.waitForConnected();
                if(xc_socket.isValid())
                {
                    xc_socket.setReadBufferSize(4096);
                    xcFD = xc_socket.socketDescriptor();
                    if(xcFD > -1)
                        ahp_xc_connect_fd(xcFD);
                }
                else
                {
                    ui->Connect->setEnabled(true);
                    update();
                }
            }
            else
            {
                ahp_xc_connect(xcport.toUtf8(), false);
                xcFD = ahp_xc_get_fd();
            }
            if(ahp_xc_is_connected())
            {
                if(!ahp_xc_get_properties())
                {
                    connected = true;
                    createPacket();
                    settings->setValue("xc_connection", xcport);
                    QString header = ahp_xc_get_header();
                    QString devices = settings->value("devices", "").toString();
                    if(!devices.contains(header))
                    {
                        if(!devices.isEmpty())
                            devices.append(",");
                        devices.append(header);
                        settings->setValue("devices", devices);
                    }
                    settings->endGroup();
                    settings->beginGroup(header);
                    context = vlbi_init();
                    vlbi_max_threads(QThreadPool::globalInstance()->maxThreadCount());
                    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
                    {
                        QString name = "Line " + QString::number(l + 1);
                        Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                        getGraph()->addSeries(Lines[l]->getMagnitude());
                        getGraph()->addSeries(Lines[l]->getPhase());
                        getGraph()->addSeries(Lines[l]->getMagnitudes());
                        getGraph()->addSeries(Lines[l]->getPhases());
                        getGraph()->addSeries(Lines[l]->getCounts());
                        vlbi_add_node(getVLBIContext(), Lines[l]->getStream(), name.toStdString().c_str(), false);
                        ui->Lines->addTab(Lines[l], name);
                    }
                    int idx = 0;
                    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
                    {
                        for(unsigned int i = l + 1; i < ahp_xc_get_nlines(); i++)
                        {
                            QString name = "Baseline " + QString::number(l + 1) + "*" + QString::number(i + 1);
                            Baselines.append(new Baseline(name, idx, Lines[l], Lines[i], settings));
                            getGraph()->addSeries(Baselines[idx]->getMagnitude());
                            getGraph()->addSeries(Baselines[idx]->getPhase());
                            idx++;
                        }
                    }
                    getGraph()->loadSettings();
                    createPacket();
                    setMode(Counter);
                    ui->Connect->setEnabled(false);
                    ui->Disconnect->setEnabled(true);
                    ui->Range->setValue(settings->value("Timerange", 0).toInt());
                    ui->Scale->setValue(settings->value("Timescale", 0).toInt());
                    ui->Scale->setEnabled(true);
                    ui->Range->setEnabled(true);
                    ui->Mode->setEnabled(true);
                    if(motorFD >= 0)
                    {
                        ui->Voltage->setValue(settings->value("Voltage", 0).toInt());
                    }
                }
                else
                    ahp_xc_disconnect();
            }
            else
                ahp_xc_disconnect();
        }
    });
    connect(getGraph(), static_cast<void (Graph::*)()>(&Graph::connectMotors),
            [ = ]()
    {
    });
    connect(getGraph(), static_cast<void (Graph::*)()>(&Graph::disconnectMotors),
            [ = ]()
    {
    });
    connect(getGraph(), static_cast<void (Graph::*)(double, double)>(&Graph::gotoRaDec),
            [ = ](double ra, double dec)
    {
        ra *= M_PI / 12.0;
        dec *= M_PI / 180.0;
        for(int x = 0; x < Lines.count(); x++)
        {
            Lines[x]->gotoRaDec(ra, dec);
        }
    });
    connect(getGraph(), static_cast<void (Graph::*)(double, double, double)>(&Graph::coordinatesUpdated),
            [ = ](double ra, double dec, double radius)
    {
        Ra = ra;
        Dec = dec;
    });
    connect(getGraph(), static_cast<void (Graph::*)(double, double, double)>(&Graph::locationUpdated),
            [ = ](double lat, double lon, double el)
    {
        vlbi_set_location(getVLBIContext(), lat, lon, el);
    });
    connect(getGraph(), static_cast<void (Graph::*)(double)>(&Graph::frequencyUpdated),
            [ = ](double freq)
    {
    });
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        ahp_xc_packet* packet = getPacket();
        switch (getMode())
        {
            case Holograph:
                if(!ahp_xc_get_packet(packet))
                {
                    double packettime = (double)packet->timestamp;
                    double diff = packettime - lastpackettime;
                    if(diff < 0 || diff > 1000000000)
                    {
                        resetTimestamp();
                        break;
                    }
                    lastpackettime = packettime;
                    packettime /= 1000000000.0;
                    packettime += J2000_starttime;
                    int idx = 0;
                    for(int x = 0; x < Lines.count(); x++) {
                        for(int y = x+1; y < Lines.count(); y++) {
                            Baseline * line = Baselines[idx];
                            if(line->isActive())
                            {
                                double offset1 = 0, offset2 = 0;
                                vlbi_get_offsets(getVLBIContext(), packettime, line->getLine1()->getName().toStdString().c_str(),
                                        line->getLine2()->getName().toStdString().c_str(), getRa(), getDec(), &offset1, &offset2);
                                offset1 /= ahp_xc_get_sampletime();
                                offset2 /= ahp_xc_get_sampletime();
                                ahp_xc_set_channel_cross(line->getLine1()->getLineIndex(), offset1, 0);
                                ahp_xc_set_channel_cross(line->getLine2()->getLineIndex(), offset2, 0);
                                line->getCounts()->append((double)packet->crosscorrelations[x].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].real);
                                line->getCounts()->append((double)packet->crosscorrelations[x].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].imaginary);
                            }
                            else
                            {
                                line->getCounts()->append(0.0);
                                line->getCounts()->append(0.0);
                            }
                            idx++;
                        }
                        Line * line = Lines[x];
                        line->getStream()->sizes[0]++;
                        line->getStream()->len++;
                        dsp_stream_alloc_buffer(line->getStream(), line->getStream()->len);
                        line->getStream()->location = (dsp_location*)realloc(line->getStream()->location,
                                                      sizeof(dsp_location) * (line->getStream()->len));
                        line->getStream()->location[line->getStream()->len - 1].xyz.x = line->getLocation()->xyz.x;
                        line->getStream()->location[line->getStream()->len - 1].xyz.y = line->getLocation()->xyz.y;
                        line->getStream()->location[line->getStream()->len - 1].xyz.z = line->getLocation()->xyz.z;
                    }
                }
                break;
            case Counter:
                if(!ahp_xc_get_packet(packet))
                {
                    double packettime = (double)packet->timestamp;
                    double diff = packettime - lastpackettime;
                    if(diff < 0 || diff > 1000000000)
                    {
                        resetTimestamp();
                        break;
                    }
                    lastpackettime = packettime;
                    packettime /= 1000000000.0;
                    packettime += J2000_starttime;
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        QLineSeries *counts[3] =
                        {
                            Lines[x]->getCounts(),
                            Lines[x]->getMagnitudes(),
                            Lines[x]->getPhases(),
                        };
                        for (int y = 0; y < 3; y++)
                        {
                            if(line->isActive())
                            {
                                if(counts[y]->count() > 0)
                                {
                                    for(int d = counts[y]->count() - 1; d >= 0; d--)
                                    {
                                        if(counts[y]->at(d).x() < packettime - (double)getTimeRange())
                                            counts[y]->remove(d);
                                    }
                                }
                                switch (y)
                                {
                                case 0:
                                    if(Lines[x]->showCounts())
                                        counts[y]->append(packettime, (double)packet->counts[x] / ahp_xc_get_packettime());
                                    break;
                                case 1:
                                    if(Lines[x]->showAutocorrelations())
                                        counts[y]->append(packettime, (double)packet->autocorrelations[x].correlations[0].magnitude * M_PI * 2 / pow(packet->autocorrelations[x].correlations[0].real+packet->autocorrelations[x].correlations[0].imaginary, 2));
                                    break;
                                case 2:
                                    if(Lines[x]->showAutocorrelations())
                                        counts[y]->append(packettime, (double)packet->autocorrelations[x].correlations[0].phase);
                                    break;
                                default:
                                    break;
                                }
                            }
                            else
                            {
                                counts[y]->clear();
                            }
                        }
                    }
                }
                break;
            case Crosscorrelator:
                for(int x = 0; x < Baselines.count(); x++)
                {
                    Baseline * line = Baselines[x];
                    if(line->isActive())
                    {
                        line->stackCorrelations();
                    }
                }
                break;
            case Autocorrelator:
                for(int x = 0; x < Lines.count(); x++)
                {
                    Line * line = Lines[x];
                    if(line->isActive())
                    {
                        line->stackCorrelations();
                    }
                }
                break;
            default:
                break;
        }
        thread->unlock();
    });
    connect(uiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [ = ](Thread * thread)
    {
        for(int x = 0; x < Lines.count(); x++)
            Lines.at(x)->paint();
        getGraph()->paint();
        thread->unlock();
    });
    connect(vlbiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        if(getMode() != Holograph) {
            thread->unlock();
            return;
        }
        for(int i  = 0; i < Baselines.count(); i++)
        {
            Baseline* line = Baselines[i];
            if(line->getCounts()->count() > 0) {
                vlbi_set_baseline_buffer(getVLBIContext(), line->getLine1()->getName().toStdString().c_str(),
                                        line->getLine2()->getName().toStdString().c_str(), (fftw_complex*)line->getCounts()->toVector().data(),
                                        line->getCounts()->count() / 2);
            }
        }
        plotVLBI("coverage", getGraph()->getCoverage(), Ra, Dec, coverage_delegate);
        plotVLBI("phase", getGraph()->getPhase(), Ra, Dec, vlbi_phase_delegate);
        plotVLBI("magnitude", getGraph()->getMagnitude(), Ra, Dec, vlbi_magnitude_delegate);
        vlbi_get_ifft(getVLBIContext(), "idft", "magnitude", "phase");

        dsp_stream_p stream = vlbi_get_model(getVLBIContext(), "idft");
        dsp_buffer_stretch(stream->buf, stream->len, 0.0, 255.0);
        dsp_buffer_1sub(stream, 255.0);
        dsp_buffer_copy(stream->buf, getGraph()->getIdft()->bits(), stream->len);
        thread->unlock();
    });
    connect(motorThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        MainWindow* main = (MainWindow*)thread->getParent();
        int fd = main->getMotorFD();
        if(fd >= 0)
        {
            if(ahp_gt_is_connected())
            {
                ///TODO
            }
        }
        thread->unlock();
    });
    resize(1280, 720);
}

void MainWindow::plotVLBI(const char *model, QImage *picture, double ra, double dec, vlbi_func2_t delegate)
{
    double radec[3] = { ra, dec, 0};
    vlbi_get_uv_plot(getVLBIContext(), model, getGraph()->getPlotWidth(), getGraph()->getPlotHeight(), radec,
                     getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, true, delegate);
    dsp_stream_p stream = vlbi_get_model(getVLBIContext(), model);
    dsp_buffer_stretch(stream->buf, stream->len, 0.0, 255.0);
    dsp_buffer_1sub(stream, 255.0);
    dsp_buffer_copy(stream->buf, picture->bits(), stream->len);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    int starty = 35 + ui->XCPort->y() + ui->XCPort->height();
    ui->Lines->setGeometry(5, starty + 5, this->width() - 10, ui->Lines->height());
    starty += 5 + ui->Lines->height();
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10);
}

void MainWindow::startThreads()
{
    readThread->start();
    uiThread->start();
    motorThread->start();
}

void MainWindow::stopThreads()
{
    uiThread->requestInterruption();
    uiThread->wait();
    vlbiThread->requestInterruption();
    vlbiThread->wait();
    readThread->requestInterruption();
    readThread->wait();
    motorThread->requestInterruption();
    motorThread->wait();
}

void MainWindow::resetTimestamp()
{
    xc_capture_flags cur = ahp_xc_get_capture_flags();
    ahp_xc_set_capture_flags((xc_capture_flags)(cur|CAP_RESET_TIMESTAMP));
    start = QDateTime::currentDateTimeUtc();
    lastpackettime = 0;
    ahp_xc_set_capture_flags((xc_capture_flags)(cur&~CAP_RESET_TIMESTAMP));
    timespec ts = vlbi_time_string_to_timespec((char*)start.toString(Qt::DateFormat::ISODate).toStdString().c_str());
    J2000_starttime = vlbi_time_timespec_to_J2000time(ts);
    for(int i = 0; i < Lines.count(); i++)
        Lines[i]->getStream()->starttimeutc = ts;
}

MainWindow::~MainWindow()
{
    stopThreads();
    uiThread->~Thread();
    vlbiThread->~Thread();
    readThread->~Thread();
    motorThread->~Thread();
    unsigned char v = 0x80;
    if(motorFD >= 0)
    {
        write(motorFD, &v, 1);
        if(ahp_gt_is_connected())
            ahp_gt_disconnect();
    }
    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
    {
        ahp_xc_set_leds(l, 0);
    }
    ahp_xc_set_capture_flags(CAP_NONE);
    if(connected)
    {
        ui->Disconnect->clicked(false);
    }
    getGraph()->~Graph();
    delete ui;
}
