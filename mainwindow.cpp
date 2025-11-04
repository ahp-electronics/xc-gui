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
#include <libusb.h>
#include <urjtag.h>

const double graph_ratio = 0.75;

vlbi_context context[vlbi_total_contexts];
double coverage_delegate(double x, double y)
{
    (void)x;
    (void)y;
    return 1.0;
}

static int32_t reset_by_vid_pid(int vid, int pid)
{
    int rc = 0;
    struct libusb_device_handle *handle;

    libusb_init(nullptr);

    handle = libusb_open_device_with_vid_pid(nullptr, vid, pid);

    if(!handle){
        return 1;
    }

    if(libusb_reset_device(handle)){
        printf("Reset failed, you may need to replug your device.\n");
        rc = 1;
    }
    libusb_attach_kernel_driver(handle, 0);

    libusb_close(handle);

    libusb_exit(nullptr);

    return rc;
}

static void flash_svf(QString svf, QString bsdl)
{
    QString cmd = "echo 'cable FT2232\nbsdl path "+bsdl+"\ndetect\nfrequency "+QString::number(12000000)+"\nsvf "+svf+"'|jtag";
    system(cmd.toStdString().c_str());
    sleep(1);
    reset_by_vid_pid(0x0403, 0x6014);
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

QStringList MainWindow::CheckFirmware(QString url, int timeout_ms)
{
    QByteArray list;
    QJsonDocument doc;
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QNetworkReply *response = manager->get(QNetworkRequest(QUrl(url)));
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(response, SIGNAL(finished()), &loop, SLOT(quit()));
    timer.start(timeout_ms);
    loop.exec();
    QString base64 = "";
    if(response->error() == QNetworkReply::NetworkError::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(response->readAll());
        QJsonObject obj = doc.object();
        base64 = obj["data"].toString();
    }
    if(base64.isNull() || base64.isEmpty()) {
        goto ck_end;
    }
    list = QByteArray::fromBase64(base64.toUtf8());
    doc = QJsonDocument::fromJson(list.toStdString().c_str());
    response->deleteLater();
    response->manager()->deleteLater();
    return doc.toVariant().toStringList();
ck_end:
    response->deleteLater();
    response->manager()->deleteLater();
    return QStringList();
}

bool MainWindow::DownloadFirmware(QString url, QString svf, QString bsdl, QSettings *settings, int timeout_ms)
{
    QByteArray bin;
    url+="&download=on";
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QNetworkReply *response = manager->get(QNetworkRequest(QUrl(url)));
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    connect(response, SIGNAL(finished()), &loop, SLOT(quit()));
    timer.start(timeout_ms);
    loop.exec();
    QString base64 = settings->value("firmware-"+svf, "").toString();
    if(response->error() == QNetworkReply::NetworkError::NoError) {
        if(QFile::exists(svf)) unlink(svf.toUtf8());
        if(QFile::exists(bsdl)) unlink(bsdl.toUtf8());
        QJsonDocument doc = QJsonDocument::fromJson(response->readAll());
        QJsonObject obj = doc.object();
        base64 = obj["data"].toString();
        if(base64.isNull() || base64.isEmpty()) {
            goto dl_end;
        }
        bin = QByteArray::fromBase64(base64.toUtf8());
        QFile svf_file(svf);
        svf_file.open(QIODevice::WriteOnly);
        svf_file.write(bin, bin.length());
        svf_file.close();
        base64 = obj["image"].toString();
        if(base64.isNull() || base64.isEmpty()) {
            goto dl_end;
        }
        bin = QByteArray::fromBase64(base64.toUtf8());
        QFile bsdl_file(bsdl);
        bsdl_file.open(QIODevice::WriteOnly);
        bsdl_file.write(bin, bin.length());
        bsdl_file.close();
    }
dl_end:
    response->deleteLater();
    response->manager()->deleteLater();
    if(!QFile::exists(svf)) return false;
    if(!QFile::exists(bsdl)) return false;
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
    QString url = "https://www.iliaplatone.com/firmware.php?product=";
    bsdl_filename = homedir + dir_separator + strrand(32) + ".bsm";
    svf_filename = homedir + dir_separator + strrand(32);
    stdout_filename = homedir + dir_separator + QDateTime::currentDateTimeUtc().toString(Qt::DateFormat::ISODate).replace(":", "") + ".log";

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
    sendThread = new Thread(this, 1, 1000, "sendThread");
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
    settings->endGroup();
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.length(); i++)
    {
        QString portname =
            ports[i].portName();
        if(portname != ui->XCPort->itemText(0) && ports[i].vendorIdentifier() == 0x403 && ports[i].productIdentifier() == 0x6014)
            ui->XCPort->addItem(portname);
        if(portname != ui->MotorPort->itemText(0))
            ui->MotorPort->addItem(portname);
    }
    QStringList firmwares = CheckFirmware(url+"xc*");
    if(firmwares.count() > 0) {
        ui->firmware->clear();
        for (QString fw : firmwares)
            ui->firmware->addItem(fw.replace("firmware/", "").replace("-firmware.bin", ""));
    }
    ui->firmware->setCurrentText(settings->value("firmware", "xc2").toString());
    if(ui->XCPort->itemText(0) != "no connection")
        ui->XCPort->addItem("no connection");
    if(ui->MotorPort->itemText(0) != "no connection")
        ui->MotorPort->addItem("no connection");
    ui->XCPort->setCurrentIndex(0);
    ui->MotorPort->setCurrentIndex(0);
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
        for(Polytope* line : Polytopes)
                line->setTimeRange(TimeRange);

    });
    connect(ui->extclock, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked),
            [ = ](bool checked)
            {
                settings->setValue("extclock", checked);
                ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags()&~CAP_EXT_CLK));
                if(ui->extclock->isChecked())
                    ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags()|CAP_EXT_CLK));
            });
    connect(ui->Order, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [ = ](int value)
            {
                settings->setValue("Order", value);
                Order = ui->Order->value();
                setOrder();
            });
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        (void)checked;
        if(!connected)
            return;
        ui->Mode->setCurrentIndex(0);
        stopThreads();
        for(Line * line : Lines)
            line->setActive(false);
        for(Polytope * line : Polytopes)
        {
            getGraph()->removeSeries(line->getSpectrum()->getMagnitude());
            getHistogram()->removeSeries(line->getCounts()->getHistogram());
            line->~Polytope();
        }
        for(Line * line : Lines)
        {
            getGraph()->removeSeries(line->getCounts()->getSeries());
            getGraph()->removeSeries(line->getCounts()->getMagnitude());
            getGraph()->removeSeries((QLineSeries*)line->getSpectrum()->getMagnitude());
            getHistogram()->removeSeries(line->getCounts()->getHistogram());
            line->~Line();
        }
        Polytopes.clear();
        Lines.clear();
        ui->Lines->clear();
        freePacket();
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
        ui->Disconnect->setEnabled(false);
        ui->Range->setEnabled(false);
        ui->Order->setEnabled(false);
        ui->Mode->setEnabled(false);
        getGraph()->clearSeries();
        getHistogram()->clearSeries();
        connected = false;
    });
    connect(ui->Baudrate, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
    [ = ](int index)
    {
        ahp_xc_set_baudrate((baud_rate)index);
        settings->setValue("firmware", index);
    });
    connect(ui->firmware, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
    [ = ](int index)
    {
        settings->setValue("firmware", ui->firmware->currentText());
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this,
            [ = ](bool checked)
    {
        (void)checked;
        int port = 5760;
        QString address = "localhost";
        QString xcport, motorport, controlport;
        xcport = ui->XCPort->currentText();
        motorport = ui->MotorPort->currentText();
        settings->beginGroup("Connection");

        ui->firmware->setEnabled(false);
        ui->Connect->setEnabled(false);
        ui->XCPort->setEnabled(false);
        has_svf_firmware = false;
        if(DownloadFirmware(url+ui->firmware->currentText(), svf_filename, bsdl_filename, settings))
            has_svf_firmware = true;
        else {
            QFile svf_file(svf_filename);
            svf_file.open(QIODevice::ReadWrite);
            QFile bsdl_file(bsdl_filename);
            bsdl_file.open(QIODevice::ReadWrite);
            if(svf_file.isOpen() && bsdl_file.isOpen()) {
                QFile s(":/data/"+ui->firmware->currentText()+".json");
                s.open(QIODevice::ReadOnly);
                if(s.isOpen()) {
                    QJsonDocument doc = QJsonDocument::fromJson(s.readAll());
                    QJsonObject obj = doc.object();
                    QString base64 = obj["data"].toString();
                    if(base64.isNull() || base64.isEmpty()) {
                        has_svf_firmware = false;
                    } else {
                        has_svf_firmware = true;
                        svf_file.write(QByteArray::fromBase64(base64.toUtf8()));
                    }
                    base64 = obj["image"].toString();
                    if(base64.isNull() || base64.isEmpty()) {
                        has_bsdl = false;
                    } else {
                        has_bsdl = true;
                        bsdl_file.write(QByteArray::fromBase64(base64.toUtf8()));
                    }
                    s.close();
                }
                svf_file.close();
                bsdl_file.close();
            }
        }
        if(has_svf_firmware) {
            flash_svf(svf_filename, bsdl_filename);
            ui->firmware->setEnabled(true);
            ui->Connect->setEnabled(true);
            ui->XCPort->setEnabled(true);
        }
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
                QTcpSocket socket(this);
                socket.connectToHost(address, port);
                xcFD = (int)socket.socketDescriptor();
                ahp_xc_connect_fd(xcFD);
            }
            else
            {
                xc_local_port = true;
                ahp_xc_connect(xcport.toUtf8());
                xcFD = ahp_xc_get_fd();
            }
            if(ahp_xc_is_detected())
            {
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
                setOrder();
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
                        setOrder();
                    });
                    connect(this, static_cast<void (MainWindow::*)(ahp_xc_packet*)>(&MainWindow::newPacket), this, [ = ](ahp_xc_packet *packet)
                    {
                        Lines[l]->addCount(J2000_starttime, packet);
                    });
                    connect(getGraph(), static_cast<void (Graph::*)(Mode)>(&Graph::modeChanging), this, [=] (Mode m) {
                        switch(m) {
                        case Autocorrelator:
                            getGraph()->addSeries(Lines[l]->getSpectrum()->getMagnitude(), QString::number(Autocorrelator) + "0#" + QString::number(l+1));
                            getHistogram()->addSeries(Lines[l]->getSpectrum()->getHistogramMagnitude(), QString::number(Autocorrelator) + "0#" + QString::number(l+1));
                            break;
                        case CrosscorrelatorII:
                        case CrosscorrelatorIQ:
                            break;
                        case Counter:
                            getGraph()->addSeries(Lines[l]->getCounts()->getSeries(), QString::number(Counter) + "0#" + QString::number(l+1));
                            getGraph()->addSeries(Lines[l]->getCounts()->getMagnitude(), QString::number(Counter) + "1#" + QString::number(l+1));
                            getHistogram()->addSeries(Lines[l]->getCounts()->getHistogram(), QString::number(Counter) + "0#" + QString::number(l+1));
                            getHistogram()->addSeries(Lines[l]->getCounts()->getHistogramMagnitude(), QString::number(Counter) + "1#" + QString::number(l+1));
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
                    QString name = "Polytope " + QString::number(idx);
                    fprintf(f_stdout, "Adding %s\n", name.toStdString().c_str());
                    Polytopes.append(new Polytope(name, idx, Lines, settings));
                    Polytopes[idx]->setTimeRange(TimeRange);
                    connect(this, static_cast<void (MainWindow::*)(ahp_xc_packet*)>(&MainWindow::newPacket), [ = ](ahp_xc_packet *packet)
                    {
                        Polytopes[idx]->addCount(J2000_starttime, packet);
                    });
                    connect(getGraph(), static_cast<void (Graph::*)(Mode)>(&Graph::modeChanging), this, [=] (Mode m) {
                        switch(m) {
                        case Autocorrelator:
                            break;
                        case CrosscorrelatorII:
                        case CrosscorrelatorIQ:
                            getGraph()->addSeries(Polytopes[idx]->getSpectrum()->getMagnitude(), QString::number(CrosscorrelatorII) + "0#" + QString::number(idx+1));
                            getHistogram()->addSeries(Polytopes[idx]->getSpectrum()->getHistogramMagnitude(), QString::number(CrosscorrelatorII) + "0#" + QString::number(idx+1));
                            break;
                        case Counter:
                            getGraph()->addSeries(Polytopes[idx]->getCounts()->getMagnitude(), QString::number(Counter) + "3#" + QString::number(idx+1));
                            getHistogram()->addSeries(Polytopes[idx]->getCounts()->getHistogram(), QString::number(Counter) + "2#" + QString::number(idx+1));
                            getHistogram()->addSeries(Polytopes[idx]->getCounts()->getHistogramMagnitude(), QString::number(Counter) + "3#" + QString::number(idx+1));
                            break;
                        case HolographII:
                        case HolographIQ:
                            break;
                        default: break;
                        }
                    });
                    Polytopes[idx]->setGraph(getGraph());
                    Polytopes[idx]->sethistogram(getHistogram());
                    Polytopes[idx]->setStopPtr(&threadsStopped);
                }

                createPacket();

                Order = settings->value("Order", 2).toInt();
                ui->Order->setEnabled(true);
                ui->Connect->setEnabled(false);
                ui->Run->setEnabled(true);
                ui->Disconnect->setEnabled(true);
                ui->Range->setValue(settings->value("Timerange", 0).toInt());
                ui->Range->setEnabled(true);
                ui->Mode->setEnabled(true);
                setMode(Counter);
            } else {
err_exit:
                sleep(1);
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
    connect(getGraph(), static_cast<void (Graph::*)()>(&Graph::Refresh), this, [ = ]()
    {
    });
    connect(getHistogram(), static_cast<void (Graph::*)()>(&Graph::Refresh), this, [ = ]()
    {
    });
    connect(this, static_cast<void (MainWindow::*)()>(&MainWindow::repaint), [ = ]()
    {
    });
    connect(this, static_cast<void (MainWindow::*)(ahp_xc_packet*)>(&MainWindow::newPacket), this, [ = ](ahp_xc_packet *packet)
    {
    });
    connect(sendThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ] (Thread * thread)
    {
        if(threadsStopped)
            goto end_unlock;
        switch (getMode())
        {
            case HolographII:
            case HolographIQ:
                setRa(getRa());
                setDec(getDec());
            case CrosscorrelatorII:
            case CrosscorrelatorIQ:
                setOrder();
                break;
            case Autocorrelator:
                break;
            case Counter:
                break;
            default:
                break;
        }
            setMotorHandle(motorFD);
        end_unlock:
            thread->unlock();
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
                for(Polytope *line : Polytopes)
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
                ahp_xc_set_correlation_order(1);
                npackets = ahp_xc_scan_correlations(requests.toVector().data(), requests.count(), &spectrum, &threadsStopped, &percent);
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
                if(ui->Run->text() == "Stop") {
                    for(int x = 0; x < Lines.count(); x++)
                        Lines[x]->runClicked(false);
                    QThread::msleep(250);
                    for(int x = 0; x < Lines.count(); x++)
                        Lines[x]->runClicked(false);
                }
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
        }
        emit repaint();
        thread->unlock();
    });
    connect(vlbiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [ = ] (Thread * thread)
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
    sendThread->start();
    motorThread->start();
}

void MainWindow::stopThreads()
{
    if(ui->Run->text() == "Stop")
        ui->Run->click();
    for(int l = 0; l < Lines.count(); l++)
        Lines[l]->Stop();
    uiThread->stop();
    sendThread->stop();
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
        ahp_xc_set_capture_flags((xc_capture_flags)(ahp_xc_get_capture_flags() & ~(CAP_ENABLE)));
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
    if(ui->Run->text() == "Stop") {
        for(int x = 0; x < Lines.count(); x++)
            Lines[x]->runClicked(false);
        QThread::msleep(250);
        for(int x = 0; x < Lines.count(); x++)
            Lines[x]->runClicked(false);
    }
}

void MainWindow::resetTimestamp()
{
    J2000_starttime = getTime();
    lastpackettime = J2000_starttime;
}

void MainWindow::setOrder()
{
    int max_order = 0;
    for(int x = 0; x < Lines.count(); x++) {
        if(Lines[x]->scanActive() || Lines[x]->showCrosscorrelations())
            max_order ++;
    }

    if(max_order >= 2) {
        int order = fmax(2, fmin(max_order, Order));
        if(getOrder() != order) return;
        for(int x = 0; x < Polytopes.count(); x++)
            Polytopes[x]->setCorrelationOrder(order);
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
        ahp_xc_set_correlation_order(2);
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
    sendThread->stop();
    readThread->stop();
    motorThread->stop();
    uiThread->wait();
    vlbiThread->wait();
    sendThread->wait();
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
