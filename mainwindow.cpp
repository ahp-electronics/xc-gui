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
#include <urjtag.h>
#include <dfu.h>


const double graph_ratio = 0.75;

vlbi_context context[vlbi_total_contexts];
double coverage_delegate(double x, double y)
{
    (void)x;
    (void)y;
    return 1.0;
}

static int32_t flash_svf(int32_t fd, const char *bsdl_path)
{
    return program_jtag(fd, "dirtyjtag", bsdl_path, 1000000, 0);
}

static int32_t flash_dfu(int32_t fd, int32_t *progress, int32_t *finished)
{
    return dfu_flash(fd, progress, finished);
}

static char *strrand(int len)
{
    int i;
    char* ret = (char*)malloc(len+1);
    for(i = 0; i < len; i++)
        ret[i] = 'a' + (rand() % 21);
    ret[i] = 0;
    return ret;
}

QMutex(MainWindow::vlbi_mutex);

bool MainWindow::DownloadFirmware(QString url, QString filename, QSettings *settings, int timeout_ms)
{
    QByteArray bin;
    QFile file(filename);
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QNetworkReply *response = manager->get(QNetworkRequest(QUrl(url)));
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(response, SIGNAL(finished()), &loop, SLOT(quit()));
    timer.start(timeout_ms);
    loop.exec();
    QString base64 = settings->value("firmware", "").toString();
    if(response->error() == QNetworkReply::NetworkError::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(response->readAll());
        QJsonObject obj = doc.object();
        base64 = obj["data"].toString();
    }
    if(base64.isNull() || base64.isEmpty()) {
        goto dl_end;
    }
    bin = QByteArray::fromBase64(base64.toUtf8());
    file.open(QIODevice::WriteOnly);
    file.write(bin, bin.length());
    file.close();
dl_end:
    response->deleteLater();
    response->manager()->deleteLater();
    if(!QFile::exists(filename)) return false;
    return true;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setAccessibleName("MainWindow");
#ifdef _WIN32
    QString dir_separator = "\\";
#else
    QString dir_separator = "/";
#endif
    homedir = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).at(0);
    QString ini = homedir + dir_separator + "settings.ini";
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
    QString url = "https://www.iliaplatone.com/firwmare.php?product=";
    bsdl_filename = homedir + dir_separator + strrand(32) + ".bsm";
    svf_filename = homedir + dir_separator + strrand(32);
    dfu_filename = homedir + dir_separator + strrand(32);
    stdout_filename = homedir + dir_separator + QDateTime::currentDateTimeUtc().toString(Qt::DateFormat::ISODate).replace(":", "") + ".log";
    if(DownloadFirmware(url+"xc-hub", dfu_filename, settings))
        has_dfu_firmware = true;

    connected = false;
    TimeRange = 10;
    fd_stdout = open(stdout_filename.toStdString().c_str(), O_CREAT|O_RDWR, 0666);
    if(fd_stdout != -1) {
        f_stdout = fdopen(fd_stdout, "r+");
    } else {
        f_stdout = stderr;
    }
    if(f_stdout != nullptr) {
        dsp_set_stdout(f_stdout);
        dsp_set_stderr(f_stdout);
        ahp_set_stderr(f_stdout);
    }
    ui->setupUi(this);
    uiThread = new Thread(this, 50, 50, "uiThread");
    readThread = new Thread(this, 1, 1, "readThread");
    vlbiThread = new Thread(this, 500, 500, "vlbiThread");
    motorThread = new Thread(this, 500, 500, "motorThread");
    graph = new Graph(settings, this);
    histogram = new Graph(settings, this);
    int starty = 80;
    ui->Lines->setGeometry(5, starty + 5, this->width() - 10, ui->Lines->height());
    starty += 5 + ui->Lines->height();
    getGraph()->setUpdatesEnabled(true);
    getGraph()->setVisible(true);
    getGraph()->setGeometry(5, starty + 5, this->width() * graph_ratio - 10, this->height() - starty - 10);
    getHistogram()->setUpdatesEnabled(true);
    getHistogram()->setVisible(true);
    getHistogram()->setGeometry(getGraph()->width() + 10, starty + 5, this->width() - getGraph()->width() - 10, this->height() - starty - 10);
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
    ui->Download->setChecked(settings->value("Download", false).toBool());
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
        for(Baseline* line : Baselines)
                line->setTimeRange(TimeRange);

    });
    connect(ui->Order, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [ = ](int value)
    {
        settings->setValue("Order", value);
        Order = ui->Order->value();
        updateOrder();
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
        ui->Download->setEnabled(true);
        ui->Mode->setCurrentIndex(0);
        stopThreads();
        for(Line * line : Lines)
            line->setActive(false);
        for(Baseline * line : Baselines)
        {
            getGraph()->removeSeries(line->getSpectrum()->getMagnitude());
            getHistogram()->removeSeries(line->getCounts()->getHistogram());
            line->~Baseline();
        }
        for(Line * line : Lines)
        {
            getGraph()->removeSeries(line->getCounts()->getSeries());
            getGraph()->removeSeries(line->getCounts()->getMagnitude());
            getGraph()->removeSeries((QLineSeries*)line->getSpectrum()->getMagnitude());
            getHistogram()->removeSeries(line->getCounts()->getHistogram());
            line->~Line();
        }
        Baselines.clear();
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
        ui->Order->setEnabled(false);
        ui->Mode->setEnabled(false);
        getGraph()->clearSeries();
        getHistogram()->clearSeries();
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        (void)checked;
        int port = 5760;
        bool try_high_speed;
        QString address = "localhost";
        QString xcport, motorport, controlport;
        xcport = ui->XCPort->currentText();
        motorport = ui->MotorPort->currentText();
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
                try_high_speed = true;
high_speed_connect:
                xcFD = ahp_xc_get_fd();
            }
            if(ahp_xc_is_connected())
            {
                if(!ahp_xc_get_properties())
                {
                    ui->Download->setEnabled(false);
                    if(ui->Download->isChecked()) {
                        QString product;
                        if(ahp_xc_has_crosscorrelator())
                            product.append("x");
                        else
                            product.append("a");
                        product.append("c");
                        product.append(QString::number(ahp_xc_get_nlines()));
                        if(DownloadFirmware(url+product, svf_filename, settings))
                            has_svf_firmware = true;
                        if(has_svf_firmware) {
                            if(DownloadFirmware(url+"ecp5u", bsdl_filename, settings))
                                has_svf_firmware = true;
                            QString bsdl_path = homedir;
                            QFile file(svf_filename);
                            file.open(QIODevice::ReadOnly);
                            int maxerr = 10;
                            int err = 1;
                            while (maxerr-- > 0 && err != 0)
                                err = flash_svf(file.handle(), bsdl_path.toStdString().c_str());
                            file.close();
                            if (err) goto err_exit;
                        }
                    }

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
                    ahp_xc_max_threads(QThread::idealThreadCount());
                    vlbi_max_threads(QThread::idealThreadCount());

                    for(int i = 0; i < vlbi_total_contexts; i++) {
                        context[i] = vlbi_init();
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
                    for(unsigned int l = 0; l < ahp_xc_get_nlines(); l++)
                    {
                        QString name = "Line " + QString::number(l + 1);
                        fprintf(f_stdout, "Adding %s\n", name.toStdString().c_str());
                        Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                        Lines[l]->setTimeRange(TimeRange);
                        connect(this, static_cast<void (MainWindow::*)()>(&MainWindow::scanStarted), [ = ] () {
                            Lines[l]->enableControls(false);
                        });
                        connect(this, static_cast<void (MainWindow::*)(bool)>(&MainWindow::scanFinished), [ = ] (bool complete) {
                            Lines[l]->enableControls(true);
                        });
                        connect(Lines[l], static_cast<void (Line::*)(Line*)>(&Line::scanActiveStateChanging),
                                [ = ](Line* line) {
                            if(line->scanActive())
                                line->addToVLBIContext();
                            else
                                line->removeFromVLBIContext();
                        });
                        connect(Lines[l], static_cast<void (Line::*)(Line*)>(&Line::scanActiveStateChanged),
                                [ = ](Line* line) {
                            updateOrder();
                        });
                        connect(this, static_cast<void (MainWindow::*)(ahp_xc_packet*)>(&MainWindow::newPacket), [ = ](ahp_xc_packet *packet)
                        {
                            Lines[l]->addCount(J2000_starttime, packet);
                            emit repaint();
                        });
                        connect(getGraph(), static_cast<void (Graph::*)(Mode)>(&Graph::modeChanging), this, [=] (Mode m) {
                            switch(m) {
                            case Autocorrelator:
                                getGraph()->addSeries(Lines[l]->getSpectrum()->getMagnitude(), QString::number(Autocorrelator) + "0#" + QString::number(l+1));
                                getGraph()->addSeries(Lines[l]->getSpectrum()->getPhase(), QString::number(Autocorrelator) + "1#" + QString::number(l+1));
                                getHistogram()->addSeries(Lines[l]->getSpectrum()->getHistogram(), QString::number(Autocorrelator) + "0#" + QString::number(l+1));
                                break;
                            case CrosscorrelatorII:
                            case CrosscorrelatorIQ:
                                break;
                            case Counter:
                                getGraph()->addSeries(Lines[l]->getCounts()->getSeries(), QString::number(Counter) + "0#" + QString::number(l+1));
                                getGraph()->addSeries(Lines[l]->getCounts()->getMagnitude(), QString::number(Counter) + "1#" + QString::number(l+1));
                                getGraph()->addSeries(Lines[l]->getCounts()->getPhase(), QString::number(Counter) + "2#" + QString::number(l+1));
                                getHistogram()->addSeries(Lines[l]->getCounts()->getHistogram(), QString::number(Counter) + "0#" + QString::number(l+1));
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
                        Lines[l]->setGraph(getGraph());
                        Lines[l]->sethistogram(getHistogram());
                        Lines[l]->setStopPtr(&threadsStopped);
                        Lines[l]->Initialize();
                        ui->Lines->addTab(Lines[l], name);
                    }
                    for(unsigned int idx = 0; idx < ahp_xc_get_nbaselines(); idx++)
                    {
                        QString name = "Baseline " + QString::number(idx);
                        fprintf(f_stdout, "Adding %s\n", name.toStdString().c_str());
                        Baselines.append(new Baseline(name, idx, Lines, settings));
                        Baselines[idx]->setTimeRange(TimeRange);
                        connect(this, static_cast<void (MainWindow::*)(ahp_xc_packet*)>(&MainWindow::newPacket), [ = ](ahp_xc_packet *packet)
                        {
                            Baselines[idx]->addCount(J2000_starttime, packet);
                            emit repaint();
                        });
                        connect(getGraph(), static_cast<void (Graph::*)(Mode)>(&Graph::modeChanging), this, [=] (Mode m) {
                            switch(m) {
                            case Autocorrelator:
                                break;
                            case CrosscorrelatorII:
                            case CrosscorrelatorIQ:
                                getGraph()->addSeries(Baselines[idx]->getSpectrum()->getMagnitude(), QString::number(CrosscorrelatorII) + "0#" + QString::number(idx+1));
                                getGraph()->addSeries(Baselines[idx]->getSpectrum()->getPhase(), QString::number(CrosscorrelatorII) + "1#" + QString::number(idx+1));
                                getHistogram()->addSeries(Baselines[idx]->getSpectrum()->getHistogram(), QString::number(CrosscorrelatorII) + "0#" + QString::number(idx+1));
                                break;
                            case Counter:
                                getGraph()->addSeries(Baselines[idx]->getCounts()->getMagnitude(), QString::number(Counter) + "3#" + QString::number(idx+1));
                                getGraph()->addSeries(Baselines[idx]->getCounts()->getPhase(), QString::number(Counter) + "4#" + QString::number(idx+1));
                                getHistogram()->addSeries(Baselines[idx]->getCounts()->getHistogram(), QString::number(Counter) + "#" + QString::number(idx+1));
                                break;
                            case HolographII:
                            case HolographIQ:
                                break;
                            default: break;
                            }
                        });
                        Baselines[idx]->setGraph(getGraph());
                        Baselines[idx]->sethistogram(getHistogram());
                        Baselines[idx]->setStopPtr(&threadsStopped);
                    }

                    createPacket();

                    Order = settings->value("Order", 2).toInt();
                    ui->Order->setEnabled(true);
                    ui->Connect->setEnabled(false);
                    ui->Voltage->setEnabled(ahp_xc_has_leds());
                    ui->Run->setEnabled(true);
                    ui->Disconnect->setEnabled(true);
                    ui->Range->setValue(settings->value("Timerange", 0).toInt());
                    ui->Range->setEnabled(true);
                    ui->Mode->setEnabled(true);
                    ui->Voltage->setValue(settings->value("Voltage", 0).toInt());
                    setMode(Counter);
                } else {
err_exit:
                    ui->Download->setEnabled(true);
                    ahp_xc_disconnect();
                    if(xc_local_port && try_high_speed) {
                        try_high_speed = false;
                        ahp_xc_connect(xcport.toUtf8(), true);
                        goto high_speed_connect;
                    }
                    return;
                }
            }
            else {
                ui->Download->setEnabled(true);
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
            [ = ](double ra, double dec, double distance)
    {
        Ra = ra;
        Dec = dec;
        Distance = distance;
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
            Lines[x]->setElevation(Elevation);
        }
        for(int i = 0; i < vlbi_total_contexts; i++)
            vlbi_set_location(getVLBIContext(i), Latitude, Longitude, Elevation);
    });
    connect(getGraph(), static_cast<void (Graph::*)(double)>(&Graph::frequencyUpdated),
            [ = ](double freq)
    {
        (void)freq;
    });
    connect(getHistogram(), static_cast<void (Graph::*)(double)>(&Graph::frequencyUpdated),
            [ = ](double freq)
    {
        (void)freq;
    });
    connect(ui->Download, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked),
            [ = ](bool checked)
    {
        settings->setValue("Download", checked);
    });
    connect(getGraph(), static_cast<void (Graph::*)()>(&Graph::Refresh), this, [ = ]()
    {
    });
    connect(getHistogram(), static_cast<void (Graph::*)()>(&Graph::Refresh), this, [ = ]()
    {
    });
    connect(this, static_cast<void (MainWindow::*)()>(&MainWindow::repaint), this, [ = ]()
    {
        getHistogram()->paint();
        getGraph()->paint();
        update(rect());
    });
    connect(this, static_cast<void (MainWindow::*)(ahp_xc_packet*)>(&MainWindow::newPacket), [ = ](ahp_xc_packet *packet)
    {
    });
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ] (Thread * thread)
    {
        int y = 0;
        int off = 0;
        ahp_xc_packet *packet;
        QList<ahp_xc_scan_request> requests;
        ahp_xc_sample *spectrum = nullptr;
        int npackets;
        if(threadsStopped)
            goto end_unlock;
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
                requests.clear();
                for(int x = 0; x < Lines.count(); x++)
                {
                    Line* line = Lines[x];
                    if(line->scanActive())
                    {
                        requests.append((ahp_xc_scan_request) {
                                            .index = line->getLineIndex(),
                                            .start = (off_t)line->getStartChannel(),
                                            .len = (size_t)line->getChannelBandwidth(),
                                            .step = (size_t)line->getScanStep()
                                        }
                        );
                        line->setPercentPtr(&percent);
                    } else {
                        line->resetPercentPtr();
                    }
                }
                npackets = ahp_xc_scan_autocorrelations(requests.toVector().data(), requests.count(), &spectrum, &threadsStopped, &percent);
                if(npackets == 0)
                    break;
                for(int x = 0; x < Lines.count(); x++)
                {
                    Line* line = Lines[x];
                    if(line->scanActive())
                    {
                        line->stackCorrelations(&spectrum[off]);
                        off += requests[y].len/requests[y].step;
                        y++;
                    }
                }
                free(spectrum);
                if(!threadsStopped)
                    ui->Run->click();
                break;
            default:
                if(!ahp_xc_get_packet(getPacket())) {
                    packet = ahp_xc_copy_packet(getPacket());
                    double diff = packet->timestamp - lastpackettime;
                    lastpackettime = packet->timestamp;
                    lock();
                    if(diff < TimeRange)
                        newPacket(packet);
                    unlock();
                    ahp_xc_free_packet(packet);
                }
                break;
        }
    end_unlock:
        thread->unlock();
    });
    connect(uiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [ = ] (Thread * thread)
    {
        for(int x = 0; x < Lines.count(); x++)
            Lines.at(x)->paint();
        fseek(f_stdout, 0, SEEK_END);
        int len = ftell(f_stdout);
        fseek(f_stdout, lastlog_pos, SEEK_SET);
        lastlog_pos = fmax(0, len);
        len -= ftell(f_stdout);
        if(len > 0) {
            char text[len];
            fread(text, 1, len, f_stdout);
            statusBar()->clearMessage();
            if(len == 0)
                statusBar()->showMessage("Ready", 1000);
            else {
                char *next = text;
                char *line = next;
                char *eol = strchr(next, '\n');
                while(eol > next) {
                    line = next;
                    *eol = 0;
                    next = eol+1;
                    eol = strchr(next, '\n');
                }
                statusBar()->showMessage(line, 1000);
            }
            ui->voltageLabel->setText("Voltage: " + QString::number(currentVoltage * 100 / 255) + " %");
            ui->voltageLabel->update(ui->voltageLabel->rect());
        }
        emit repaint();
        thread->unlock();
    });
    connect(vlbiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ] (Thread * thread)
    {
        if(getMode() == HolographIQ || getMode() == HolographII)
        {
            double radec[] = { getGraph()->getRa(), getGraph()->getDec(), getGraph()->getDistance() };
            if(enable_vlbi) {
                vlbi_get_uv_plot(getVLBIContext(), "coverage",
                                 getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                                 getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), 1, 0, coverage_delegate, &threadsStopped);
                vlbi_get_uv_plot(getVLBIContext(), "magnitude",
                                 getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                                 getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), 1, 0, vlbi_magnitude_delegate, &threadsStopped);
                vlbi_get_uv_plot(getVLBIContext(), "phase",
                                 getGraph()->getPlotSize(), getGraph()->getPlotSize(), radec,
                                 getGraph()->getFrequency(), 1.0 / ahp_xc_get_packettime(), 1, 0, vlbi_phase_delegate, &threadsStopped);
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
                } else {
                    if(vlbi_has_model(getVLBIContext(), "magnitude") && vlbi_has_model(getVLBIContext(), "phase"))
                        vlbi_get_ifft(getVLBIContext(), "idft", "magnitude", "phase");
                }
                emit repaint();
            }
        }
        thread->unlock();
    });
    connect(motorThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ] (Thread * thread)
    {
        MainWindow* main = (MainWindow*)thread->getParent();
        int fd = -1;
        setVoltage(ui->Voltage->value());
        fd = main->getMotorFD();
        if(fd >= 0)
        {
            if(ahp_gt_is_connected())
            {
                for (Line *line : Lines) {
                    line->updateLocation();
                }
            }
        }
        thread->unlock();
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
    getGraph()->setGeometry(5, starty, this->width() * ((getMode() != HolographII && getMode() != HolographIQ) ? graph_ratio : 1.0) - 10, this->height() - starty - statusBar()->height());
    getHistogram()->setGeometry(getGraph()->width() + 10, starty, this->width() - getGraph()->width() - 10, this->height() - starty - statusBar()->height());
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
        resetTimestamp();
        if(getMode() != Autocorrelator && getMode() != CrosscorrelatorII && getMode() != CrosscorrelatorIQ)
            ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags() | CAP_ENABLE));
        if(getMode() == HolographIQ || getMode() == HolographII) {
            vlbiThread->start();
        }
        for(int x = 0; x < Lines.count(); x++) {
            if(getMode() != Counter && getMode() != HolographII && getMode() != HolographIQ) {
                nlines++;
            }
            Lines[x]->setActive(true);
        }
        if(nlines > 0 && getMode() != Counter && getMode() != HolographII && getMode() != HolographIQ)
        for(Line *line : Lines) {
            if(line->scanActive()) {
                emit line->setBufferSizes();
                if(getMode() == Autocorrelator) {
                    line->enableControls(false);
                }
            }
        }
        if(getMode() == Autocorrelator)
            emit scanStarted();
        threadsStopped = false;
    }
    else
    {
        ui->Run->setText("Run");
        threadsStopped = true;
        if(getMode() != Autocorrelator && getMode() != CrosscorrelatorII && getMode() != CrosscorrelatorIQ)
            ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags() & ~CAP_ENABLE));
        if(getMode() == HolographIQ || getMode() == HolographII) {
            vlbiThread->stop();
        }
        for(int x = 0; x < Lines.count(); x++) {
            Lines[x]->setActive(false);
        }
        nlines = 0;
        if(getMode() == Autocorrelator)
            emit scanFinished(false);
    }
    for(int x = 0; x < Lines.count(); x++)
        Lines[x]->runClicked(ui->Run->text() == "Stop");
}

void MainWindow::resetTimestamp()
{
    J2000_starttime = getTime();
    lastpackettime = J2000_starttime;
}

void MainWindow::updateOrder()
{
    int max_order = 0;
    for(int x = 0; x < Lines.count(); x++) {
        if(Lines[x]->scanActive() || Lines[x]->showCrosscorrelations())
            max_order ++;
    }

    if(max_order >= 2) {
        int order = fmax(2, fmin(max_order, Order));
        for(int x = 0; x < Baselines.count(); x++)
            Baselines[x]->setCorrelationOrder(order);
        while(!lock_vlbi());
        ahp_xc_set_correlation_order(order);
        vlbi_set_correlation_order(getVLBIContext(), order);
        unlock_vlbi();
        ui->Order->blockSignals(true);
        ui->Order->setRange(2, max_order);
        ui->Order->setValue(order);
        ui->Order->blockSignals(false);
        enable_vlbi = true;
    } else {
        enable_vlbi = false;
        ui->Order->setRange(2, 2);
    }
}

void MainWindow::setVoltage(int level)
{
    if(ahp_xc_is_detected() && currentVoltage != level) {
        currentVoltage = Min(255, Max(0, level));
        ahp_xc_set_voltage(0, currentVoltage);
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
    getHistogram()->~Graph();
    getGraph()->~Graph();
    settings->~QSettings();
    fclose(f_stdout);
    delete ui;
}
