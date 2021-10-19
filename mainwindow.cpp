#include "types.h"
#include <cmath>
#include <ctime>
#include <cstring>
#include <QIODevice>
#include <QStandardPaths>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QTcpSocket>
#include <QThreadPool>
#include <QDir>
#include <fcntl.h>
#include <vlbi.h>
#include "mainwindow.h"
#include "./ui_mainwindow.h"

static double coverage_delegate(double x, double y)
{
    return 1;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setAccessibleName("MainWindow");
    QString homedir = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).at(0);
    QString ini = homedir+"/settings.ini";
    if(!QDir(homedir).exists()){
        QDir().mkdir(homedir);
    }
    if(!QFile(ini).exists()){
        QFile *f = new QFile(ini);
        f->open(QIODevice::WriteOnly);
        f->close();
        f->~QFile();
    }
    settings = new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings->setDefaultFormat(QSettings::Format::NativeFormat);
    settings->setUserIniPath(ini);
    connected = false;
    TimeRange = 10;
    ui->setupUi(this);
    uiThread = new Thread(100);
    readThread = new Thread(500);
    vlbiThread = new Thread(500);
    motorThread = new Thread(500);
    graph = new Graph(this);
    setMode(Counter);
    int starty = ui->Lines->y()+ui->Lines->height()+5;
    getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
    getGraph()->setUpdatesEnabled(true);
    getGraph()->setVisible(true);

    settings->beginGroup("Connection");
    ui->XCPort->clear();
    ui->XCPort->addItem(settings->value("lastconnected", "localhost:5760").toString());
    QStringList devices = settings->value("devices", "").toString().split(",");
    settings->endGroup();
    for (int i = 0; i < devices.length(); i++) {
        settings->beginGroup(devices[i]);
        QString connection = settings->value("xc_connection", "").toString();
        if(!connection.isEmpty())
            ui->XCPort->addItem(connection);
        connection = settings->value("gps_connection", "").toString();
        if(!connection.isEmpty())
            ui->GpsPort->addItem(connection);
        connection = settings->value("motor_connection", "").toString();
        if(!connection.isEmpty())
            ui->MotorPort->addItem(connection);
        settings->endGroup();
    }
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.length(); i++) {
        QString portname =
        #ifndef _WIN32
        "/dev/"+
        #endif
                            ports[i].portName();
        ui->XCPort->addItem(portname);
        ui->GpsPort->addItem(portname);
        ui->MotorPort->addItem(portname);
    }
    ui->XCPort->setCurrentIndex(0);
    ui->GpsPort->setCurrentIndex(0);
    ui->MotorPort->setCurrentIndex(0);
    connect(ui->Mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [=](int index) {
        setMode((Mode)index);
        for(int x = 0; x < Lines.count(); x++) {
            Lines[x]->setMode((Mode)index);
        }
    });
    connect(ui->Range, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int value) {
        settings->setValue("Timerange", value);
        TimeRange = ui->Range->value();
    });
    connect(ui->Scale, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int value) {
        settings->setValue("Timescale", value);
        ahp_xc_set_frequency_divider((char)value);
    });
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        stopThreads();
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->Scale->setEnabled(false);
        if(!connected)
            return;
        freePacket();
        vlbi_exit(vlbi_context);
        for(int l = 0; l < ahp_xc_get_nlines(); l++) {
            Lines[l]->~Line();
        }
        Lines.clear();
        getGraph()->clearSeries();
        if(xc_socket.isOpen()) {
            xc_socket.disconnectFromHost();
        }
        if(motor_socket.isOpen()) {
            motor_socket.disconnectFromHost();
        }
        if(gps_socket.isOpen()) {
            gps_socket.disconnectFromHost();
        }
        ahp_xc_disconnect();
        ahp_gt_disconnect();
        getGraph()->deinitGPS();
        settings->endGroup();
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        int fd = -1;
        int port = 5760;
        QString address = "localhost";
        bool ttyconn = false;
        if(ui->GpsPort->currentText().contains(':')) {
            address = ui->GpsPort->currentText().split(":")[0];
            port = ui->GpsPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            update();
            gps_socket.connectToHost(address, port);
            gps_socket.waitForConnected();
            if(gps_socket.isValid()) {
                gps_socket.setReadBufferSize(4096);
                fd = gps_socket.socketDescriptor();
                if(fd > -1){
                    getGraph()->setGnssPortFD(fd);
                }
            } else {
                ui->Connect->setEnabled(true);
                update();
            }
        } else {
            fd = open(ui->GpsPort->currentText().toUtf8(), O_RDWR);
            if(fd > -1) {
                getGraph()->setGnssPortFD(fd);
            }
        }
        if(getGraph()->initGPS())
            settings->setValue("gps_connection", ui->GpsPort->currentText());
        fd = -1;
        if(ui->XCPort->currentText().contains(':')) {
            address = ui->XCPort->currentText().split(":")[0];
            port = ui->XCPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            update();
            xc_socket.connectToHost(address, port);
            xc_socket.waitForConnected();
            if(xc_socket.isValid()) {
                xc_socket.setReadBufferSize(4096);
                fd = xc_socket.socketDescriptor();
                if(fd > -1)
                    ahp_xc_connect_fd(fd);
            } else {
                ui->Connect->setEnabled(true);
                update();
            }
        } else {
            ahp_xc_connect(ui->XCPort->currentText().toUtf8(), false);
        }
        if(ahp_xc_is_connected()) {
            if(!ahp_xc_get_properties()) {
                connected = true;
                settings->beginGroup("Connection");
                settings->setValue("lastconnected", ui->XCPort->currentText());
                QString header = ahp_xc_get_header();
                settings->setValue("lastdevice", header);
                QString devices = settings->value("devices", "").toString();
                if(!devices.contains(header)) {
                    if(!devices.isEmpty())
                        devices.append(",");
                    devices.append(header);
                    settings->setValue("devices", devices);
                }
                settings->endGroup();
                settings->beginGroup(header);
                settings->setValue("xc_connection", ui->XCPort->currentText());
                vlbi_context = vlbi_init();
                vlbi_max_threads(QThreadPool::globalInstance()->maxThreadCount());
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    QString name = "Line "+QString::number(l+1);
                    Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                    getGraph()->addSeries(Lines[l]->getSpectrum());
                    getGraph()->addSeries(Lines[l]->getDots());
                    getGraph()->addSeries(Lines[l]->getCounts());
                    getGraph()->addSeries(Lines[l]->getAutocorrelations());
                    vlbi_add_node(getVLBIContext(), Lines[l]->getStream(), (char*)name.toStdString().c_str(), 0);
                    ui->Lines->addTab(Lines[l], name);
                }
                int idx = 0;
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    for(int i = l+1; i < ahp_xc_get_nlines(); i++) {
                        QString name = "Baseline "+QString::number(l+1)+"*"+QString::number(i+1);
                        Baselines.append(new Baseline(name, idx, Lines[l], Lines[i], settings));
                    }
                }
                createPacket();
                setMode(Counter);
                startThreads();
                ui->Connect->setEnabled(false);
                ui->Disconnect->setEnabled(true);
                ui->Range->setValue(settings->value("Timerange", 0).toInt());
                ui->Scale->setValue(settings->value("Timescale", 0).toInt());
                ui->Scale->setEnabled(true);
                ui->Range->setEnabled(true);
            } else
                ahp_xc_disconnect();
        } else
            ahp_xc_disconnect();
        fd = -1;
        if(ui->MotorPort->currentText().contains(':')) {
            address = ui->MotorPort->currentText().split(":")[0];
            port = ui->MotorPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            update();
            motor_socket.connectToHost(address, port);
            motor_socket.waitForConnected();
            if(motor_socket.isValid()) {
                motor_socket.setReadBufferSize(4096);
                fd = motor_socket.socketDescriptor();
                if(fd > -1) {
                    if(!ahp_gt_connect_fd(fd)) {
                        settings->setValue("motor_connection", ui->MotorPort->currentText());
                    }
                }
            } else {
                ui->Connect->setEnabled(true);
                update();
            }
        } else {
            if(!ahp_gt_connect(ui->MotorPort->currentText().toUtf8())) {
                settings->setValue("motor_connection", ui->MotorPort->currentText());
            }
        }
    });
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [=](Thread* thread)
    {
        ahp_xc_packet* packet = getPacket();
        switch (getMode()) {
        case Crosscorrelator:
            if(!ahp_xc_get_packet(packet)) {
                double offs_time = (double)packet->timestamp/10000000.0;
                offs_time += getStartTime();
                double offset1 = 0, offset2 = 0;
                for(int x = 0; x < Lines.count(); x++) {
                    Line * line = Lines[x];
                    line->getStream()->location = (dsp_location*)realloc(line->getStream()->location, sizeof(dsp_location)*(line->getStream()->len));
                    line->getStream()->location[line->getStream()->len-1].xyz.x = line->getLocation()->xyz.x;
                    line->getStream()->location[line->getStream()->len-1].xyz.y = line->getLocation()->xyz.y;
                    line->getStream()->location[line->getStream()->len-1].xyz.z = line->getLocation()->xyz.z;
                }
                for(int x = 0; x < Baselines.count(); x++) {
                    Baseline * line = Baselines[x];
                    if(line->isActive()) {
                        vlbi_get_offsets(getVLBIContext(), offs_time, (char*)line->getLine1()->getName().toStdString().c_str(), (char*)line->getLine2()->getName().toStdString().c_str(), getRa(), getDec(), &offset1, &offset2);
                        ahp_xc_set_lag_cross(line->getLine1()->getLineIndex(), (off_t)(offset1*getFrequency()));
                        ahp_xc_set_lag_cross(line->getLine2()->getLineIndex(), (off_t)(offset2*getFrequency()));
                        line->getValues()->append(packet->crosscorrelations[x].correlations[ahp_xc_get_crosscorrelator_lagsize()/2].coherence);
                    } else {
                        line->getValues()->append(0.0);
                    }
                }
            }
            break;
        case Counter:
            if(!ahp_xc_get_packet(packet)) {
                double packettime = (double)packet->timestamp/10.0;
                double diff = packettime - lastpackettime;
                if(diff < 0 || diff > 10000000) {
                    resetTimestamp();
                    break;
                }
                lastpackettime = packettime;
                packettime /= 1000000.0;
                packettime += J2000_starttime;
                for(int x = 0; x < Lines.count(); x++) {
                    Line * line = Lines[x];
                    QLineSeries *counts[2] = {
                        Lines[x]->getCounts(),
                        Lines[x]->getAutocorrelations()
                    };
                    for (int y = 0; y < 2; y++) {
                        if(line->isActive()) {
                            if(counts[y]->count() > 0) {
                                for(int d = counts[y]->count() - 1; d >= 0; d--) {
                                    if(counts[y]->at(d).x() < packettime-(double)getTimeRange())
                                        counts[y]->remove(d);
                                }
                            }
                            switch (y) {
                            case 0:
                                if(Lines[x]->showCounts())
                                    counts[y]->append(packettime, packet->counts[x]*1000000/ahp_xc_get_packettime());
                                break;
                            case 1:
                                if(Lines[x]->showAutocorrelations())
                                    counts[y]->append(packettime, packet->autocorrelations[x].correlations[0].correlations*1000000/ahp_xc_get_packettime());
                                break;
                            default:
                                break;
                            }
                        } else {
                            counts[y]->clear();
                        }
                    }
                }
            }
            break;
        case Autocorrelator:
            for(int x = 0; x < Lines.count(); x++) {
                Line * line = Lines[x];
                if(line->isActive()) {
                    line->stackCorrelations();
                }
            }
            break;
        case Spectrograph:
            if(!ahp_xc_get_packet(packet)) {
                for(int x = 0; x < Lines.count(); x++) {
                    Line * line = Lines[x];
                    if(line->isActive()) {
                        if(packet->autocorrelations[x].correlations[0].coherence > 0 && packet->autocorrelations[x].correlations[0].coherence < 1.0) {
                            line->sumValue(packet->autocorrelations[x].correlations[0].coherence, 1);
                        }
                    } else {
                        line->getAverage()->clear();
                    }
                }
            }
            break;
        default:
            break;
        }
        thread->unlock();
    });
    connect(uiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [=](Thread* thread)
    {
        for(int x = 0; x < Lines.count(); x++)
            Lines.at(x)->paint();
        if(vlbi_mutex.tryLock()) {
            getGraph()->paint();
            vlbi_mutex.unlock();
        }
        settings->sync();
        thread->unlock();
    });
    connect(vlbiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [=](Thread* thread)
    {
        if(getMode() != Crosscorrelator)
            return;
        for(int i  = 0; i < Baselines.count(); i++) {
            Baseline* line = Baselines[i];
            if(line->getValues()->count() > 0)
                vlbi_set_baseline_buffer(getVLBIContext(), (char*)line->getLine1()->getName().toStdString().c_str(), (char*)line->getLine2()->getName().toStdString().c_str(), line->getValues()->toVector().data(), line->getValues()->count());
        }
        double radec [2] = { getRa(), getDec() };
        dsp_stream_p coverage_stream = vlbi_get_uv_plot(getVLBIContext(), getGraph()->getPlotWidth(), getGraph()->getPlotHeight(), radec, getFrequency(), 1000000.0/ahp_xc_get_packettime(), true, true, coverage_delegate);
        dsp_stream_p raw_stream = vlbi_get_uv_plot(getVLBIContext(), getGraph()->getPlotWidth(), getGraph()->getPlotHeight(), radec, getFrequency(), 1000000.0/ahp_xc_get_packettime(), true, true, vlbi_default_delegate);
        dsp_buffer_stretch(coverage_stream->buf, coverage_stream->len, 0.0, 255.0);
        dsp_buffer_stretch(raw_stream->buf, raw_stream->len, 0.0, 255.0);
        QImage *coverage = getGraph()->getCoverage();
        QImage *idft = getGraph()->getIdft();
        QImage *raw = getGraph()->getRaw();
        dsp_stream_p idft_stream = vlbi_get_ifft_estimate(raw_stream);
        dsp_buffer_stretch(idft_stream->buf, idft_stream->len, 0.0, 255.0);
        while(!vlbi_mutex.tryLock()) {
            if(thread->isInterruptionRequested())
                goto end_free;
            QThread::msleep(10);
        }
        dsp_buffer_1sub(coverage_stream, 255.0);
        dsp_buffer_1sub(raw_stream, 255.0);
        dsp_buffer_1sub(idft_stream, 255.0);
        dsp_buffer_copy(coverage_stream->buf, coverage->bits(), coverage_stream->len);
        dsp_buffer_copy(raw_stream->buf, raw->bits(), raw_stream->len);
        dsp_buffer_copy(idft_stream->buf, idft->bits(), idft_stream->len);
        vlbi_mutex.unlock();
end_free:
        dsp_stream_free_buffer(raw_stream);
        dsp_stream_free_buffer(idft_stream);
        dsp_stream_free(raw_stream);
        dsp_stream_free(idft_stream);
        thread->unlock();
    });
    connect(motorThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [=](Thread* thread)
    {
        dsp_location location;
        for(int x = 0; x < Lines.count(); x++) {
            Lines[x]->setLocation(location);
        }
        thread->unlock();
    });
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);
   int starty = 15+ui->XCPort->y()+ui->XCPort->height();
   ui->Lines->setGeometry(5, starty+5, this->width()-10, ui->Lines->height());
   starty += 5+ui->Lines->height();
   getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
}

void MainWindow::startThreads()
{
    readThread->setTimer(ahp_xc_get_packettime()/1000);
    readThread->start();
    uiThread->start();
    vlbiThread->start();
    motorThread->start();
}

void MainWindow::stopThreads()
{
    uiThread->requestInterruption();
    uiThread->wait();
    uiThread->unlock();
    uiThread->~Thread();
    readThread->requestInterruption();
    readThread->wait();
    readThread->unlock();
    readThread->~Thread();
    vlbiThread->requestInterruption();
    vlbiThread->wait();
    vlbiThread->unlock();
    vlbiThread->~Thread();
    motorThread->requestInterruption();
    motorThread->wait();
    motorThread->unlock();
    motorThread->~Thread();
}

MainWindow::~MainWindow()
{
    for(int l = 0; l < ahp_xc_get_nlines(); l++) {
        ahp_xc_set_leds(l, 0);
    }
    if(connected) {
        ui->Disconnect->clicked(false);
    }
    getGraph()->~Graph();
    delete ui;
}
