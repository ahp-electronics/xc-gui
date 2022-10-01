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
#include "QMutex"
#include "mainwindow.h"
#include "./ui_mainwindow.h"
vlbi_context context[vlbi_total_contexts];
static double coverage_delegate(double x, double y)
{
    (void)x;
    (void)y;
    return 1.0;
}

QMutex(MainWindow::vlbi_mutex);

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
    f_stdout = tmpfile();
    if(f_stdout != nullptr) {
        dsp_set_stdout(f_stdout);
        dsp_set_stderr(f_stdout);
    } else {
        f_stdout = stdout;
    }
    ui->setupUi(this);
    uiThread = new Thread(this, 10, 10, "uiThread");
    readThread = new Thread(this, 100, 1, "readThread");
    vlbiThread = new Thread(this, 1000, 1000, "vlbiThread");
    motorThread = new Thread(this, 500, 500, "motorThread");
    graph = new Graph(settings, this);
    int starty = 80;
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
    connect(ui->Run, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this, &MainWindow::runClicked);
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
        for(Line* line : Lines)
            line->setTimeRange(TimeRange);
    });
    connect(ui->Voltage, static_cast<void (QSlider::*)(int)>(&QSlider::valueChanged),
            [ = ](int value)
    {
        settings->setValue("Voltage", value);
    });
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        (void)checked;
        if(!connected)
            return;
        ui->Mode->setCurrentIndex(0);
        stopThreads();
        for(Baseline * line : Baselines)
        {
            getGraph()->removeSeries(line->getCounts());
            getGraph()->removeSeries(line->getMagnitudes());
            getGraph()->removeSeries(line->getPhases());
            getGraph()->removeSeries(line->getMagnitude());
            getGraph()->removeSeries(line->getPhase());
            line->~Baseline();
        }
        Baselines.clear();
        for(Line * line : Lines)
        {
            getGraph()->removeSeries(line->getCounts());
            getGraph()->removeSeries(line->getMagnitudes());
            getGraph()->removeSeries(line->getPhases());
            getGraph()->removeSeries(line->getMagnitude());
            getGraph()->removeSeries(line->getPhase());
            line->~Line();
        }
        Lines.clear();
        ui->Lines->clear();
        freePacket();
        if(xc_local_port)
            ahp_xc_set_baudrate(R_BASE);
        ahp_xc_set_capture_flags(CAP_NONE);
        ahp_xc_disconnect();
        if(xc_socket.isOpen())
        {
            xc_socket.disconnectFromHost();
        }
        if(ahp_gt_is_connected())
        {
            if(ahp_gt_is_connected())
                ahp_gt_disconnect();
        }
        if(motor_socket.isOpen())
        {
            motor_socket.disconnectFromHost();
        }
        if(ahp_hub_is_connected())
        {
            if(ahp_hub_is_connected())
                ahp_hub_disconnect();
        }
        if(control_socket.isOpen())
        {
            control_socket.disconnectFromHost();
        }
        motorFD = -1;
        controlFD = -1;
        for(int i = 0; i < vlbi_total_contexts; i++)
            vlbi_exit(getVLBIContext(i));
        settings->endGroup();
        ui->Connect->setEnabled(true);
        ui->Run->setEnabled(false);
        ui->Voltage->setEnabled(false);
        ui->Disconnect->setEnabled(false);
        ui->Range->setEnabled(false);
        ui->Mode->setEnabled(false);
        getGraph()->clearSeries();
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
        xcFD = -1;
        xc_local_port = false;
        if(xcport == "no connection")
        {
            settings->setValue("xc_connection", xcport);
            return;
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
                xc_local_port = true;
                ahp_xc_connect(xcport.toUtf8(), false);
                xcFD = ahp_xc_get_fd();
            }
            if(ahp_xc_is_connected())
            {
                if(!ahp_xc_get_properties())
                {
                    if(ahp_xc_get_packettime() > 0.1 && xc_local_port)
                        ahp_xc_set_baudrate((baud_rate)round(log2(ahp_xc_get_packettime() / 0.1)));
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
                                    ahp_gt_select_device(0);
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
                            if(!ahp_gt_connect(motorport.toUtf8()))
                                settings->setValue("motor_connection", motorport);
                            motorFD = ahp_gt_get_fd();
                        }
                    }
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
                                if(ahp_hub_is_connected())
                                {
                                    getGraph()->setMotorFD(controlFD);
                                    if(!ahp_hub_connect_fd(getGraph()->getMotorFD()))
                                        settings->setValue("control_connection", controlport);
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
                            if(!ahp_hub_connect(controlport.toUtf8()))
                                settings->setValue("control_connection", controlport);
                            controlFD = ahp_hub_get_fd();
                        }
                    }
                    connected = true;
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
                    for(int i = 0; i < vlbi_total_contexts; i++)
                        context[i] = vlbi_init();
                    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
                    {
                        QString name = "Line " + QString::number(l + 1);
                        fprintf(f_stdout, "Adding %s\n", name.toStdString().c_str());
                        Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                        Lines[l]->setTimeRange(TimeRange);
                        connect(Lines[l], static_cast<void (Line::*)(Line*)>(&Line::activeStateChanging),
                                [ = ](Line* sender) {
                            (void)sender;
                        });
                        connect(getGraph(), static_cast<void (Graph::*)(Mode)>(&Graph::modeChanging), this, [=] (Mode m) {
                            switch(m) {
                            case Autocorrelator:
                                getGraph()->addSeries(Lines[l]->getMagnitude(), QString::number(Autocorrelator) + "0#" + QString::number(l+1));
                                getGraph()->addSeries(Lines[l]->getPhase(), QString::number(Autocorrelator) + "1#" + QString::number(l+1));
                                break;
                            case CrosscorrelatorII:
                            case CrosscorrelatorIQ:
                                break;
                            case Counter:
                            case Spectrograph:
                                getGraph()->addSeries(Lines[l]->getMagnitudes(), QString::number(Counter) + "0#" + QString::number(l+1));
                                getGraph()->addSeries(Lines[l]->getCounts(), QString::number(Counter) + "1#" + QString::number(l+1));
                                break;
                            case HolographII:
                            case HolographIQ:
                                break;
                            default: break;
                            }
                        });
                        connect(getGraph(), static_cast<void (Graph::*)(double, double)>(&Graph::gotoRaDec), Lines[l], &Line::gotoRaDec);
                        connect(getGraph(), static_cast<void (Graph::*)()>(&Graph::startTracking), Lines[l], &Line::startTracking);
                        connect(getGraph(), static_cast<void (Graph::*)(double, double)>(&Graph::startSlewing), Lines[l], &Line::startSlewing);
                        connect(getGraph(), static_cast<void (Graph::*)()>(&Graph::haltMotors), Lines[l], &Line::haltMotors);
                        connect(ui->Run, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), Lines[l], &Line::runClicked);
                        Lines[l]->setGraph(getGraph());
                        Lines[l]->setStopPtr(&threadsStopped);
                        ui->Lines->addTab(Lines[l], name);
                    }
                    int idx = 0;
                    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
                    {
                        for(unsigned int i = l + 1; i < ahp_xc_get_nlines(); i++)
                        {
                            QString name = "Baseline " + QString::number(l + 1) + "*" + QString::number(i + 1);
                            fprintf(f_stdout, "Adding %s\n", name.toStdString().c_str());
                            Baselines.append(new Baseline(name, idx, Lines[l], Lines[i], settings));
                            Baselines[idx]->setGraph(getGraph());
                            Baselines[idx]->setStopPtr(&threadsStopped);
                            connect(getGraph(), static_cast<void (Graph::*)(Mode)>(&Graph::modeChanging), this, [=] (Mode m) {
                                switch(m) {
                                case Autocorrelator:
                                    break;
                                case CrosscorrelatorII:
                                case CrosscorrelatorIQ:
                                    getGraph()->addSeries(Baselines[idx]->getMagnitude(), QString::number(CrosscorrelatorII) + "0#" + QString::number(idx+1));
                                    break;
                                case Counter:
                                case Spectrograph:
                                    getGraph()->addSeries(Baselines[idx]->getMagnitudes(), QString::number(CrosscorrelatorII) + "0#" + QString::number(idx+1));
                                    break;
                                case HolographII:
                                case HolographIQ:
                                    break;
                                default: break;
                                }
                            });
                            idx++;
                        }
                    }

                    ahp_xc_set_correlation_order(2);
                    ahp_xc_max_threads(QThread::idealThreadCount());
                    vlbi_max_threads(QThread::idealThreadCount());

                    for(int i = 0; i < vlbi_total_contexts; i++) {
                        context[i] = vlbi_init();
                    }

                    getGraph()->loadSettings();
                    createPacket();
                    ui->Connect->setEnabled(false);
                    ui->Voltage->setEnabled(ahp_xc_has_leds());
                    ui->Run->setEnabled(true);
                    ui->Disconnect->setEnabled(true);
                    ui->Range->setValue(settings->value("Timerange", 0).toInt());
                    ui->Range->setEnabled(true);
                    ui->Mode->setEnabled(true);
                    if(ahp_hub_is_connected())
                    {
                        ui->Voltage->setValue(settings->value("Voltage", 0).toInt());
                    }
                    setMode(Counter);
                } else {
                    ahp_xc_disconnect();
                    return;
                }
            }
            else {
                ahp_xc_disconnect();
                return;
            }
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
    connect(getGraph(), static_cast<void (Graph::*)(double, double, double)>(&Graph::coordinatesUpdated),
            [ = ](double ra, double dec, double radius)
    {
        (void)radius;
        Ra = ra;
        Dec = dec;
    });
    connect(getGraph(), static_cast<void (Graph::*)(double, double, double)>(&Graph::locationUpdated),
            [ = ](double lat, double lon, double el)
    {
        Latitude = lat;
        Longitude = lon;
        Elevation = el;
        for(int x = 0; x < Lines.count(); x++)
        {
            Lines[x]->setLatitude(Latitude);
            Lines[x]->setLongitude(Longitude);
        }
        for(int i = 0; i < vlbi_total_contexts; i++)
            vlbi_set_location(getVLBIContext(i), Latitude, Longitude, Elevation);
    });
    connect(getGraph(), static_cast<void (Graph::*)(double)>(&Graph::frequencyUpdated),
            [ = ](double freq)
    {
        (void)freq;
    });
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        ahp_xc_packet* packet = getPacket();
        QList<unsigned int> indexes;
        QList<off_t> starts;
        QList<size_t> sizes;
        QList<size_t> steps;
        ahp_xc_sample *spectrum = nullptr;
        int npackets;
        if(threadsStopped)
            goto end_unlock;
        for(Line *line : Lines)
            if(line->scanActive())
                line->UpdateBufferSizes();
        switch (getMode())
        {
            case CrosscorrelatorII:
            case CrosscorrelatorIQ:
                for(Baseline *line : Baselines)
                {
                    if(line->scanActive())
                    {
                        line->setPercentPtr(&percent);
                        line->stackCorrelations();
                    } else {
                        line->resetPercentPtr();
                    }
                }
                break;
            case Autocorrelator:
                indexes.clear();
                starts.clear();
                sizes.clear();
                steps.clear();
                for(Line *line : Lines)
                {
                    if(line->scanActive())
                    {
                        indexes.append(line->getLineIndex());
                        starts.append(line->getStartChannel());
                        sizes.append(line->getChannelBandwidth());
                        steps.append(line->getScanStep());
                        line->setPercentPtr(&percent);
                    } else {
                        line->resetPercentPtr();
                    }
                }
                npackets = ahp_xc_scan_autocorrelations(indexes.count(), indexes.toVector().data(), &spectrum, starts.toVector().data(), sizes.toVector().data(), steps.toVector().data(), &threadsStopped, &percent);
                if(npackets == 0)
                    break;
                for(int x = 0; x < indexes.count(); x++)
                {
                    Line * line = Lines[indexes[x]];
                    if(line->isActive())
                    {
                        int off = 0;
                        if(x > 0)
                            off += sizes[x-1]/steps[x-1];
                        line->stackCorrelations(&spectrum[off]);
                    }
                }
                if(indexes.count() > 0)
                    emit scanFinished(true);
                free(spectrum);
                break;
            default:
                if(!ahp_xc_get_packet(packet))
                {
                    double packettime = packet->timestamp + J2000_starttime;
                    double diff = packettime - lastpackettime;
                    if (diff > getTimeRange() || diff < 0) {
                        resetTimestamp();
                        break;
                    }
                    lastpackettime = packettime;
                    for(Line * line : Lines)
                    {
                        line->addCount(packettime);
                    }
                    for(Baseline * line : Baselines)
                    {
                        line->addCount(packettime);
                    }
                }
                break;
        }
end_unlock:
        thread->unlock();
    });
    connect(uiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [ = ](Thread * thread)
    {
        for(int x = 0; x < Lines.count(); x++)
            Lines.at(x)->paint();
        QTextStream str(f_stdout);
        QString text = str.readLine();
        if(text.isEmpty())
            statusBar()->showMessage("Ready");
        else {
            statusBar()->showMessage(text.replace("\n", ""));
            fseek(f_stdout, 0, SEEK_SET);
            ftruncate(fileno(f_stdout), 0);
        }
        getGraph()->paint();
        ui->voltageLabel->setText("Voltage: " + QString::number(currentVoltage * 150 / 127) + " V~");
        ui->voltageLabel->update(ui->voltageLabel->rect());
        thread->unlock();
    });
    connect(vlbiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        if(getMode() == HolographIQ || getMode() == HolographII)
        {
            double radec[3] = { getGraph()->getRa(), getGraph()->getDec(), 0};
            lock_vlbi();

            vlbi_get_uv_plot(getVLBIContext(), "coverage",
                             getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                             getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, false, coverage_delegate, &threadsStopped);
            vlbi_get_uv_plot(getVLBIContext(), "magnitude",
                             getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                             getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, false, vlbi_magnitude_delegate, &threadsStopped);
            vlbi_get_uv_plot(getVLBIContext(), "phase",
                             getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                             getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), true, false, vlbi_phase_delegate, &threadsStopped);

            if(getGraph()->isTracking()) {
                if(vlbi_has_model(getVLBIContext(), "coverage_stack"))
                    vlbi_stack_models(getVLBIContext(), "coverage_stack", "coverage_stack", "coverage");
                else
                    vlbi_copy_model(getVLBIContext(), "coverage_stack", "coverage");

                if(vlbi_has_model(getVLBIContext(), "magnitude_stack"))
                    vlbi_stack_models(getVLBIContext(), "magnitude_stack", "magnitude_stack", "magnitude");
                else
                    vlbi_copy_model(getVLBIContext(), "magnitude_stack", "magnitude");

                if(vlbi_has_model(getVLBIContext(), "phase_stack"))
                    vlbi_stack_models(getVLBIContext(), "phase_stack", "phase_stack", "phase");
                else
                    vlbi_copy_model(getVLBIContext(), "phase_stack", "phase");

                vlbi_get_ifft(getVLBIContext(), "idft", "magnitude_stack", "phase_stack");
            }
            emit plotModels();

            unlock_vlbi();
        }
        thread->unlock();
    });
    connect(motorThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        MainWindow* main = (MainWindow*)thread->getParent();
        int fd = -1;
        fd = main->getControlFD();
        if(fd >= 0) {
            if(currentVoltage != ui->Voltage->value()) {
                currentVoltage = currentVoltage < ui->Voltage->value() ? currentVoltage + 3 : currentVoltage -5;
                setVoltage(currentVoltage);
            }
        }
        fd = main->getMotorFD();
        if(fd >= 0)
        {
            if(ahp_gt_is_connected())
            {
                ///TODO
            }
        }
        thread->unlock();
    });
    connect(this, static_cast<void (MainWindow::*)()>(&MainWindow::plotModels), this, [ = ] () {
        if(getGraph()->isTracking()) {
            getGraph()->plotModel(getGraph()->getCoverage(), (char*)"coverage_stack");
            getGraph()->plotModel(getGraph()->getMagnitude(), (char*)"magnitude_stack");
            getGraph()->plotModel(getGraph()->getPhase(), (char*)"phase_stack");
            getGraph()->plotModel(getGraph()->getIdft(), (char*)"idft");
        } else {
            vlbi_get_ifft(getVLBIContext(), "idft", "magnitude", "phase");
            getGraph()->plotModel(getGraph()->getCoverage(), (char*)"coverage");
            getGraph()->plotModel(getGraph()->getMagnitude(), (char*)"magnitude");
            getGraph()->plotModel(getGraph()->getPhase(), (char*)"phase");
            getGraph()->plotModel(getGraph()->getIdft(), (char*)"idft");
        }
    });
    resize(1280, 720);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    int starty = 80;
    ui->Lines->setGeometry(5, starty + 5, this->width() - 10, ui->Lines->height());
    starty += 5 + ui->Lines->height();
    statusBar()->setGeometry(0, this->height() - statusBar()->height(), width(), 20);
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10 - statusBar()->height());
}

void MainWindow::startThreads()
{
    uiThread->start();
    readThread->start();
    motorThread->start();
}

void MainWindow::stopThreads()
{
    if(ui->Run->text() == "Stop")
        ui->Run->click();
    for(int l = 0; l < Lines.count(); l++)
        Lines[l]->Stop();
    uiThread->stop();
    readThread->stop();
    motorThread->stop();
}

void MainWindow::runClicked(bool checked)
{
    (void)checked;
    int nlines = 0;
    if(ui->Run->text() == "Run")
    {
        ui->Run->setText("Stop");
        lock_vlbi();
        if(getMode() != Autocorrelator && getMode() != CrosscorrelatorII && getMode() != CrosscorrelatorIQ)
            ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags() | CAP_ENABLE));
        if(getMode() == HolographIQ || getMode() == HolographII) {
            readThread->stop();
            vlbiThread->start();
            readThread->start();
        }
        for(int x = 0; x < Lines.count(); x++) {
            if(getMode() == Autocorrelator) {
                nlines++;
            }
            Lines[x]->setActive(true);
        }
        if(nlines > 0 && getMode() == Autocorrelator)
            emit scanStarted();
        threadsStopped = false;
        resetTimestamp();
        unlock_vlbi();
    }
    else
    {
        ui->Run->setText("Run");
        threadsStopped = true;
        if(getMode() != Autocorrelator && getMode() != CrosscorrelatorII && getMode() != CrosscorrelatorIQ)
            ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags() & ~CAP_ENABLE));
        if(getMode() == HolographIQ || getMode() == HolographII) {
            for(int x = 0; x < Lines.count(); x++)
                Lines[x]->removeFromVLBIContext();
            vlbiThread->stop();
            readThread->stop();
        }
        for(int x = 0; x < Lines.count(); x++) {
            Lines[x]->setActive(false);
        }
        nlines = 0;
        if(getMode() == Autocorrelator)
            emit scanFinished(false);
    }
}

void MainWindow::resetTimestamp()
{
    starttime = vlbi_time_string_to_timespec((char*)QDateTime::currentDateTimeUtc().toString(
                    Qt::DateFormat::ISODate).toStdString().c_str());
    J2000_starttime = vlbi_time_timespec_to_J2000time(starttime);
    lastpackettime = J2000_starttime;
}

void MainWindow::setVoltage(int level)
{
    level = Min(255, Max(0, level));
    unsigned char v = 0xff - (level & 0x7f);
    if(ahp_hub_is_connected()) {
        ahp_hub_send_command(&v, 1);
    }
}

MainWindow::~MainWindow()
{
    uiThread->stop();
    vlbiThread->stop();
    readThread->stop();
    motorThread->stop();
    uiThread->wait();
    vlbiThread->wait();
    readThread->wait();
    motorThread->wait();
    if(connected)
    {
        ui->Disconnect->clicked(false);
    }
    getGraph()->~Graph();
    settings->~QSettings();
    fclose(f_stdout);
    delete ui;
}
