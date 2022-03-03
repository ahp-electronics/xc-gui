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
    uiThread = new Thread(this, 100, 100);
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
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->Scale->setEnabled(false);
        ui->Range->setEnabled(false);
        ui->Mode->setEnabled(false);
        getGraph()->clearSeries();
        for(unsigned int l = ahp_xc_get_nlines() - 1; l >= 0; l--)
        {
            Lines[l]->setActive(false);
            ahp_xc_set_leds(l, 0);
            ui->Lines->removeTab(l);
            Lines[l]->~Line();
        }
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
        for (int i = 0; i < vlbi_total_contexts; i++)
            vlbi_exit(getVLBIContext(i));
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
        settings->endGroup();
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
                    for (int i = 0; i < vlbi_total_contexts; i++)
                    {
                        context[i] = vlbi_init();
                    }
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
                        for (int i = 0; i < vlbi_total_contexts; i++)
                        {
                            Lines[l]->setVLBIContext(getVLBIContext(i), i);
                            vlbi_add_node(getVLBIContext(i), Lines[l]->getStream(), name.toStdString().c_str(), false);
                        }
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
                            for (int j = 0; j < vlbi_total_contexts; j++)
                                Baselines[idx]->setVLBIContext(getVLBIContext(j), j);
                            Baselines[idx]->setStream(vlbi_get_baseline_stream(getVLBIContext(vlbi_context_iq),
                                                      Baselines[idx]->getLine1()->getName().toStdString().c_str(), Baselines[idx]->getLine2()->getName().toStdString().c_str()));
                            vlbi_set_baseline_buffer(getVLBIContext(vlbi_context_iq), Baselines[idx]->getLine1()->getName().toStdString().c_str(),
                                                     Baselines[idx]->getLine2()->getName().toStdString().c_str(), Baselines[idx]->getStream()->dft.fftw,
                                                     Baselines[idx]->getStream()->len);
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
        for (int i = 0; i < vlbi_total_contexts; i++)
            vlbi_set_location(getVLBIContext(i), lat, lon, el);
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
                        resetTimestamp();
                        break;
                    }
                    int idx = 0;
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        dsp_stream_set_dim(line->getStream(), 0, line->getStream()->len + 1);
                        dsp_stream_alloc_buffer(line->getStream(), line->getStream()->len);
                        line->getStream()->buf[line->getStream()->len - 1] = (double)packet->counts[x];
                        line->getStream()->location = (dsp_location*)realloc(line->getStream()->location,
                                                      sizeof(dsp_location) * (line->getStream()->len));
                        line->getStream()->location[line->getStream()->len - 1].xyz.x = line->getLocation()->xyz.x;
                        line->getStream()->location[line->getStream()->len - 1].xyz.y = line->getLocation()->xyz.y;
                        line->getStream()->location[line->getStream()->len - 1].xyz.z = line->getLocation()->xyz.z;
                        if(getMode() == HolographIQ)
                        {
                            for(int y = x + 1; y < Lines.count(); y++)
                            {
                                Baseline * line = Baselines[idx];
                                if(line->isActive())
                                {
                                    double offset1 = 0, offset2 = 0;
                                    vlbi_get_offsets(getVLBIContext(vlbi_context_iq), packettime, line->getLine1()->getName().toStdString().c_str(),
                                                     line->getLine2()->getName().toStdString().c_str(), getRa(), getDec(), &offset1, &offset2);
                                    offset1 /= ahp_xc_get_sampletime();
                                    offset2 /= ahp_xc_get_sampletime();
                                    if(ahp_xc_has_crosscorrelator())
                                    {
                                        ahp_xc_set_channel_cross(line->getLine1()->getLineIndex(), offset1, 0);
                                        ahp_xc_set_channel_cross(line->getLine2()->getLineIndex(), offset2, 0);
                                    }
                                    dsp_stream_alloc_buffer(line->getStream(), line->getStream()->len + 1);
                                    if(getMode() == HolographIQ)
                                    {
                                        line->getStream()->dft.fftw[line->getStream()->len][0] = (double)
                                                packet->crosscorrelations[idx].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].real;
                                        line->getStream()->dft.fftw[line->getStream()->len][1] = (double)
                                                packet->crosscorrelations[idx].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].imaginary;
                                    }
                                    dsp_stream_set_dim(line->getStream(), 0, line->getStream()->len);
                                }
                                else
                                {
                                    dsp_stream_alloc_buffer(line->getStream(), line->getStream()->len + 1);
                                    line->getStream()->dft.fftw[line->getStream()->len][0] = 0.0;
                                    line->getStream()->dft.fftw[line->getStream()->len][1] = 0.0;
                                    dsp_stream_set_dim(line->getStream(), 0, line->getStream()->len);
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
            plotVLBI("coverage", getGraph()->getCoverage(), Ra, Dec, coverage_delegate);
            plotVLBI("phase", getGraph()->getPhase(), Ra, Dec, vlbi_phase_delegate);
            plotVLBI("magnitude", getGraph()->getMagnitude(), Ra, Dec, vlbi_magnitude_delegate);

            vlbi_get_ifft(getVLBIContext(getMode() == HolographIQ ? vlbi_context_iq : vlbi_context_ii), "idft", "magnitude", "phase");
            QImageFromModel(getGraph()->getIdft(), "idft");
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

void MainWindow::plotVLBI(char *model, QImage *picture, double ra, double dec, vlbi_func2_t delegate)
{
    double radec[3] = { ra, dec, 0};
    vlbi_get_uv_plot(getVLBIContext(getMode() == HolographIQ ? vlbi_context_iq : vlbi_context_ii), model,
                     getGraph()->getPlotWidth(), getGraph()->getPlotHeight(), radec,
                     getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, true, delegate);
    QImageFromModel(picture, model);
}

void MainWindow::QImageFromModel(QImage* picture, char* model)
{
    char filename[128];
    sprintf(filename, "%s/%s.jpg", QDir::tempPath().toStdString().c_str(), model);
    if(QFile::exists(filename))
        unlink(filename);
    vlbi_get_model_to_jpeg(getVLBIContext(getMode() == HolographIQ ? vlbi_context_iq : vlbi_context_ii), filename, model);
    picture->load(filename);
    unlink(filename);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    int starty = 90;
    ui->Lines->setGeometry(5, starty + 5, this->width() - 10, ui->Lines->height());
    starty += 5 + ui->Lines->height();
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10);
}

void MainWindow::startThreads()
{
    readThread->start();
}

void MainWindow::stopThreads()
{
    for(int l = 0; l < Lines.count(); l++)
        Lines[l]->Stop();
    vlbiThread->requestInterruption();
    vlbiThread->wait();
    readThread->requestInterruption();
    readThread->wait();
    motorThread->requestInterruption();
    motorThread->wait();
}

void MainWindow::resetTimestamp()
{
    int cur = ahp_xc_get_capture_flags();
    ahp_xc_set_capture_flags((xc_capture_flags)(cur & ~ENABLE_CAPTURE));
    start = QDateTime::currentDateTimeUtc();
    ahp_xc_set_capture_flags((xc_capture_flags)(cur | ENABLE_CAPTURE));
    starttime = vlbi_time_string_to_timespec((char*)start.toString(Qt::DateFormat::ISODate).toStdString().c_str());
    J2000_starttime = vlbi_time_timespec_to_J2000time(starttime);
    lastpackettime = J2000_starttime;
    for(int i = 0; i < Lines.count(); i++)
        Lines[i]->getStream()->starttimeutc = starttime;
}

void MainWindow::setVoltage(unsigned char level)
{
    unsigned char v = 0x80 | (level & 0x7f);
    if(controlFD >= 0)
        write(controlFD, &v, 1);
}

MainWindow::~MainWindow()
{
    if(connected)
    {
        ui->Disconnect->clicked(false);
    }
    uiThread->requestInterruption();
    uiThread->wait();
    uiThread->~Thread();
    vlbiThread->~Thread();
    readThread->~Thread();
    motorThread->~Thread();
    getGraph()->~Graph();
    settings->~QSettings();
    delete ui;
}
