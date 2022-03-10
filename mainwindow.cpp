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
#include "mainwindow.h"
#include "./ui_mainwindow.h"

static double coverage_delegate(double x, double y)
{
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
    uiThread = new Thread(this, 200, 100);
    readThread = new Thread(this, 100, 100);
    vlbiThread = new Thread(this, 4000, 1000);
    motorThread = new Thread(this, 1000, 1000);
    Elemental::loadCatalog();
    graph = new Graph(settings, this);
    int starty = 90;
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
    ui->ControlPort->clear();
    ui->ControlPort->addItem(settings->value("control_connection", "no connection").toString());
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
        if(portname != ui->ControlPort->itemText(0))
            ui->ControlPort->addItem(portname);
    }
    if(ui->XCPort->itemText(0) != "no connection")
        ui->XCPort->addItem("no connection");
    if(ui->MotorPort->itemText(0) != "no connection")
        ui->MotorPort->addItem("no connection");
    if(ui->ControlPort->itemText(0) != "no connection")
        ui->ControlPort->addItem("no connection");
    ui->XCPort->setCurrentIndex(0);
    ui->MotorPort->setCurrentIndex(0);
    ui->ControlPort->setCurrentIndex(0);
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
        ui->voltageLabel->setText("Voltage: " + QString::number(value * 150 / 127) + " V~");
        settings->setValue("Voltage", value);
        setVoltage(value);
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
        if(!connected)
            return;
        stopThreads();
        settings->endGroup();
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->Scale->setEnabled(false);
        ui->Range->setEnabled(false);
        ui->Mode->setEnabled(false);
        getGraph()->clearSeries();
        for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
        {
            ahp_xc_set_leds(l, 0);
        }
        Baselines.clear();
        ui->Lines->clear();
        Lines.clear();
        if(motorFD >= 0)
        {
            if(ahp_gt_is_connected())
                ahp_gt_disconnect();
        }
        if(controlFD >= 0)
        {
            setVoltage(0);
            fdclose(controlFD, "rw");
        }
        freePacket();
        ahp_xc_set_capture_flags(CAP_NONE);
        ahp_xc_disconnect();
        if(xc_socket.isOpen())
        {
            xc_socket.disconnectFromHost();
        }
        if(motor_socket.isOpen())
        {
            motor_socket.disconnectFromHost();
        }
        if(control_socket.isOpen())
        {
            control_socket.disconnectFromHost();
        }
        vlbi_exit(getVLBIContext());
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        (void)checked;
        int port = 5760;
        QString address = "localhost";
        QString xcport, motorport, controlport;
        xcport = ui->XCPort->currentText();
        motorport = ui->MotorPort->currentText();
        controlport = ui->ControlPort->currentText();
        settings->beginGroup("Connection");
        controlFD = -1;
        if(controlport == "no connection")
        {
            settings->setValue("control_connection", controlport);
        }
        else
        {
            if(controlport.contains(':'))
            {
                address = controlport.split(":")[0];
                port = controlport.split(":")[1].toInt();
                ui->Connect->setEnabled(false);
                update();
                control_socket.connectToHost(address, port);
                control_socket.waitForConnected();
                if(control_socket.isValid())
                {
                    control_socket.setReadBufferSize(4096);
                    controlFD = control_socket.socketDescriptor();
                    if(controlFD != -1)
                    {
                        getGraph()->setControlFD(controlFD);
                        if(!ahp_gt_connect_fd(getGraph()->getControlFD()))
                            if(controlFD != -1)
                            {
                                settings->setValue("control_connection", controlport);
                            }
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
                controlFD = open(controlport.toUtf8(), O_RDWR);
                if(controlFD != -1)
                {
                    settings->setValue("control_connection", controlport);
                }
            }
        }
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
                motorFD = open(motorport.toUtf8(), O_RDWR);
                if(motorFD != -1)
                {
                    settings->setValue("motor_connection", motorport);
                }
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
                    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
                    {
                        QString name = "Line " + QString::number(l + 1);
                        Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                        getGraph()->addSeries(Lines[l]->getMagnitude());
                        getGraph()->addSeries(Lines[l]->getPhase());
                        getGraph()->addSeries(Lines[l]->getMagnitudes());
                        getGraph()->addSeries(Lines[l]->getPhases());
                        getGraph()->addSeries(Lines[l]->getCounts());
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
                            getGraph()->addSeries(Baselines[idx]->getMagnitudes());
                            getGraph()->addSeries(Baselines[idx]->getPhases());
                            idx++;
                        }
                    }

                    vlbi_max_threads(1);

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
                    if(controlFD >= 0)
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
        Latitude = lat;
        Longitude = lon;
        Elevation = el;
        vlbi_set_location(getVLBIContext(), Latitude, Longitude, Elevation);
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
            case HolographIQ:
            case HolographII:
                if(!ahp_xc_get_packet(packet))
                {
                    double packettime = packet->timestamp + J2000_starttime;
                    double diff = packettime - lastpackettime;
                    lastpackettime = packettime;
                    if(diff < 0 || diff > getTimeRange())
                    {
                        break;
                    }
                    int idx = 0;
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        if(line->isActive())
                        {
                            dsp_stream_p stream = line->getStream();
                            if(stream == nullptr) continue;
                            line->lock();
                            dsp_stream_set_dim(stream, 0, stream->sizes[0] + 1);
                            dsp_stream_alloc_buffer(stream, stream->len);
                            stream->buf[stream->len - 1] = (double)packet->counts[x];
                            memcpy(&stream->location[stream->len - 1], line->getLocation(), sizeof(dsp_location));
                            line->unlock();
                        }
                        if(getMode() == HolographIQ)
                        {
                            for(int y = x + 1; y < Lines.count(); y++)
                            {
                                Baseline * line = Baselines[idx];
                                if(line->isActive())
                                {
                                    dsp_stream_p stream = line->getStream();
                                    if(stream == nullptr) continue;
                                    line->lock();
                                    dsp_stream_set_dim(stream, 0, stream->sizes[0] + 1);
                                    dsp_stream_alloc_buffer(stream, stream->len);
                                    double offset1 = 0, offset2 = 0;
                                    vlbi_get_offsets(getVLBIContext(), packettime, Lines[x]->getLastName().toStdString().c_str(),
                                                     Lines[y]->getLastName().toStdString().c_str(), getGraph()->getRa(), getGraph()->getDec(), &offset1, &offset2);
                                    offset1 /= ahp_xc_get_sampletime();
                                    offset2 /= ahp_xc_get_sampletime();
                                    if(ahp_xc_has_crosscorrelator())
                                    {
                                        ahp_xc_set_channel_cross(Lines[x]->getLineIndex(), offset1, 0);
                                        ahp_xc_set_channel_cross(Lines[y]->getLineIndex(), offset2, 0);
                                    }
                                    stream->dft.fftw[stream->len - 1][0] = (double)
                                                                           packet->crosscorrelations[idx].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].real;
                                    stream->dft.fftw[stream->len - 1][1] = (double)
                                                                           packet->crosscorrelations[idx].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].imaginary;
                                    line->unlock();
                                }
                                idx++;
                            }
                        }
                    }
                }
                break;
            case Counter:
                if(!ahp_xc_get_packet(packet))
                {
                    double packettime = packet->timestamp + J2000_starttime;
                    double diff = packettime - lastpackettime;
                    lastpackettime = packettime;
                    if(diff < 0 || diff > getTimeRange())
                    {
                        resetTimestamp();
                        break;
                    }
                    int idx = 0;
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        QLineSeries *counts[3] =
                        {
                            line->getCounts(),
                            line->getMagnitudes(),
                            line->getPhases(),
                        };
                        for (int z = 0; z < 3; z++)
                        {
                            QLineSeries *Counts = counts[z];
                            if(line->isActive())
                            {
                                if(Counts->count() > 0)
                                {
                                    for(int d = Counts->count() - 1; d >= 0; d--)
                                    {
                                        if(Counts->at(d).x() < packettime - (double)getTimeRange())
                                            Counts->remove(d);
                                    }
                                }
                                switch (z)
                                {
                                    case 0:
                                        if(line->showCounts())
                                            Counts->append(packettime, (double)packet->counts[x] / ahp_xc_get_packettime());
                                        break;
                                    case 1:
                                        if(line->showAutocorrelations())
                                            Counts->append(packettime, (double)packet->autocorrelations[x].correlations[0].magnitude * M_PI * 2 / pow(
                                                               packet->autocorrelations[x].correlations[0].real + packet->autocorrelations[x].correlations[0].imaginary, 2));
                                        break;
                                    case 2:
                                        if(line->showAutocorrelations())
                                            Counts->append(packettime, (double)packet->autocorrelations[x].correlations[0].phase);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            else
                            {
                                Counts->clear();
                            }
                        }
                        for(int y = x + 1; y < Lines.count(); y++)
                        {
                            Baseline * line = Baselines[idx];
                            QLineSeries *counts[2] =
                            {
                                line->getMagnitudes(),
                                line->getPhases(),
                            };
                            if(line->getLine1()->showCrosscorrelations() && line->getLine2()->showCrosscorrelations())
                            {
                                double mag = 0.0;
                                for (int z = 0; z < 2; z++)
                                {
                                    QLineSeries *Counts = counts[z];
                                    if(line->isActive())
                                    {
                                        if(Counts->count() > 0)
                                        {
                                            for(int d = Counts->count() - 1; d >= 0; d--)
                                            {
                                                if(Counts->at(d).x() < packettime - (double)getTimeRange())
                                                    Counts->remove(d);
                                            }
                                        }
                                        switch (z)
                                        {
                                            case 0:
                                                if(ahp_xc_has_crosscorrelator())
                                                    Counts->append(packettime, (double)packet->crosscorrelations[idx].correlations[0].magnitude * M_PI * 2 / pow(
                                                                       packet->crosscorrelations[idx].correlations[0].real + packet->crosscorrelations[idx].correlations[0].imaginary, 2));
                                                else
                                                {
                                                    mag = (double)sqrt(pow(packet->counts[x], 2) + pow(packet->counts[y],
                                                                       2)) * M_PI * 2 / pow(packet->counts[x] + packet->counts[y], 2);
                                                    Counts->append(packettime, mag);
                                                }
                                                break;
                                            case 1:
                                                if(ahp_xc_has_crosscorrelator())
                                                    Counts->append(packettime, (double)packet->crosscorrelations[idx].correlations[0].phase);
                                                else
                                                {
                                                    double rad = 0.0;
                                                    if(mag > 0.0)
                                                    {
                                                        double r = packet->counts[x] * M_PI * 2 / pow(packet->counts[x] + packet->counts[y], 2) / mag;
                                                        double i = packet->counts[y] * M_PI * 2 / pow(packet->counts[x] + packet->counts[y], 2) / mag;
                                                        double rad = acos(i);
                                                        if(r < 0.0)
                                                            rad = M_PI * 2 - rad;
                                                    }
                                                    Counts->append(packettime, rad);
                                                }
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                    else
                                    {
                                        Counts->clear();
                                    }
                                }
                            }
                            idx++;
                        }
                    }
                }
                break;
            case CrosscorrelatorII:
            case CrosscorrelatorIQ:
                for(int x = 0; x < Baselines.count(); x++)
                {
                    Baseline * line = Baselines[x];
                    if(line->isActive())
                    {
                        line->stackCorrelations();
                    }
                }
                break;
            case AutocorrelatorI:
            case AutocorrelatorIQ:
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
        if(getMode() == HolographIQ || getMode() == HolographII)
        {
            double radec[3] = { getGraph()->getRa(), getGraph()->getDec(), 0};
            vlbi_set_location(getVLBIContext(), getGraph()->getLatitude(), getGraph()->getLongitude(), getGraph()->getElevation());
            vlbi_get_uv_plot(getVLBIContext(), "coverage",
                             getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                             getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, true, coverage_delegate);
            vlbi_get_uv_plot(getVLBIContext(), "phase",
                             getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                             getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, true, vlbi_phase_delegate);
            vlbi_get_uv_plot(getVLBIContext(), "magnitude",
                             getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                             getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, true, vlbi_magnitude_delegate);
            vlbi_get_ifft(getVLBIContext(), "idft", "magnitude", "phase");

            getGraph()->plotModel(getGraph()->getCoverage(), "coverage");
            getGraph()->plotModel(getGraph()->getMagnitude(), "magnitude");
            getGraph()->plotModel(getGraph()->getPhase(), "phase");
            getGraph()->plotModel(getGraph()->getIdft(), "idft");
        }
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
    uiThread->start();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    int starty = 90;
    ui->Lines->setGeometry(5, starty + 5, this->width() - 10, ui->Lines->height());
    starty += 5 + ui->Lines->height();
    ui->statusbar->setGeometry(0, this->height() - ui->statusbar->height(), width(), 20);
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10 - ui->statusbar->height());
}

void MainWindow::startThreads()
{
    readThread->start();
}

void MainWindow::stopThreads()
{
    for(int l = 0; l < Lines.count(); l++)
        Lines[l]->Stop();
    vlbiThread->unlock();
    vlbiThread->requestInterruption();
    vlbiThread->wait();
    readThread->unlock();
    readThread->requestInterruption();
    readThread->wait();
    motorThread->unlock();
    motorThread->requestInterruption();
    motorThread->wait();
}

void MainWindow::resetTimestamp()
{
    starttime = vlbi_time_string_to_timespec((char*)QDateTime::currentDateTimeUtc().toString(
                    Qt::DateFormat::ISODate).toStdString().c_str());
    J2000_starttime = vlbi_time_timespec_to_J2000time(starttime);
    lastpackettime = J2000_starttime;
}

void MainWindow::setVoltage(unsigned char level)
{
    unsigned char v = 0x80 | (level & 0x7f);
    if(controlFD >= 0)
        write(controlFD, &v, 1);
}

MainWindow::~MainWindow()
{
    uiThread->unlock();
    uiThread->requestInterruption();
    uiThread->wait();
    if(connected)
    {
        ui->Disconnect->clicked(false);
    }
    uiThread->~Thread();
    vlbiThread->~Thread();
    readThread->~Thread();
    motorThread->~Thread();
    getGraph()->~Graph();
    settings->~QSettings();
    delete ui;
}
