#include "types.h"
#include <cmath>
#include <ctime>
#include <cstring>
#include <QStandardPaths>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QTcpSocket>
#include <QDir>
#include <fcntl.h>
#include "mainwindow.h"
#include "./ui_mainwindow.h"

void MainWindow::UiThread(QWidget *sender)
{
    MainWindow* wnd = qobject_cast<MainWindow*>(sender);
    if(!wnd)
        return;
    for(int i = 0; i < wnd->Lines.count(); i++) {
        wnd->Lines[i]->setPercent();
        if(wnd->Lines[i]->isActive())
            wnd->getGraph()->Update();
    }
    QThread::msleep(200);
}

void MainWindow::ReadThread(QWidget *sender)
{
    MainWindow* wnd = qobject_cast<MainWindow*>(sender);
    if(!wnd)
        return;
    ahp_xc_packet* packet = wnd->getPacket();

    switch (wnd->getMode()) {
        case Counter:
        if(!ahp_xc_get_packet(packet)) {
            QDateTime now = QDateTime::currentDateTimeUtc();
            double diff = (double)wnd->start.msecsTo(now)/1000.0;
            for(int x = 0; x < wnd->Lines.count(); x++) {
                Line * line = wnd->Lines[x];
                QSplineSeries *counts[3] = {
                    wnd->Lines[x]->getCounts(),
                    wnd->Lines[x]->getAutocorrelations(),
                    wnd->Lines[x]->getCrosscorrelations()
                };
                for (int y = 0; y < 3; y++) {
                    if(diff > (double)wnd->getTimeRange())
                        for(int d = 0; d < counts[y]->count(); d++)
                            if(counts[y]->at(d).x()<diff-(double)wnd->getTimeRange())
                                counts[y]->remove(d);
                    if(line->isActive()) {
                        switch (y) {
                        case 0:
                            if(wnd->Lines[x]->showCounts())
                                counts[y]->append(diff, packet->counts[x]*1000000/ahp_xc_get_packettime());
                            break;
                        case 1:
                            if(wnd->Lines[x]->showAutocorrelations())
                                counts[y]->append(diff, packet->autocorrelations[x].correlations[0].correlations*1000000/ahp_xc_get_packettime());
                            break;
                        case 2:
                            if(wnd->Lines[x]->showCrosscorrelations())
                                counts[y]->append(diff, packet->crosscorrelations[x].correlations[ahp_xc_get_crosscorrelator_lagsize()-1].correlations*1000000/ahp_xc_get_packettime());
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
        for(int x = 0; x < wnd->Lines.count(); x++) {
            Line * line = wnd->Lines[x];
            if(line->isActive()) {
                line->stackCorrelations();
            }
        }
        break;
    case Crosscorrelator:
        for(int x = 0; x < wnd->Lines.count(); x++) {
            Line * line = wnd->Lines[x];
            if(line->isActive()) {
                line->stackCorrelations(line->getLine2());
            }
        }
        break;
    default:
        break;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setAccessibleName("MainWindow");
    QString homedir = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0);
    QString inidir = homedir+"/.xc-gui/";
    QString ini = homedir+"/.xc-gui/settings.ini";
    if(!QDir(inidir).exists()){
        QDir().mkdir(inidir);
    }
    settings = new QSettings(ini, QSettings::Format::NativeFormat);

    setMode(Counter);
    connected = false;
    TimeRange = 60;
    ui->setupUi(this);
    uiThread = new progressThread(this);
    readThread = new progressThread(this);
    graph = new Graph(this);
    int starty = ui->Lines->y()+ui->Lines->height()+5;
    getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
    getGraph()->setVisible(true);

    settings->beginGroup("Connection");
    ui->ComPort->addItem(settings->value("lastconnected", "localhost:5760").toString());
    QStringList devices = settings->value("devices", "").toString().split(",");
    settings->endGroup();
    for (int i = 0; i < devices.length(); i++) {
        settings->beginGroup(devices[i]);
        QString connection = settings->value("connection", "").toString();
        if(!connection.isEmpty())
            ui->ComPort->addItem(connection);
        settings->endGroup();
    }
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.length(); i++)
        ui->ComPort->addItem(
#ifndef _WIN32
"/dev/"+
#endif
                    ports[i].portName());
    ui->ComPort->setCurrentIndex(0);
    connect(ui->Mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [=](int index) {
        setMode((Mode)index);
        for(int x = 0; x < Lines.count(); x++) {
            Lines[x]->setMode((Mode)index);
        }
    });

    connect(ui->Scale, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int value) {
        settings->setValue("Timescale", value);
        ahp_xc_set_frequency_divider((char)value);
    });
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->BaudRate->setEnabled(false);
        ui->Scale->setEnabled(false);
        freePacket();
        for(int l = 0; l < ahp_xc_get_nlines(); l++) {
            Lines[l]->~Line();
        }
        Lines.clear();
        getGraph()->clearSeries();
        if(socket.isOpen()) {
            socket.disconnectFromHost();
        }
        ahp_xc_disconnect();
        settings->endGroup();
        connected = false;
    });
    connect(ui->BaudRate, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [=](int currentIndex) {
        ahp_xc_set_baudrate((baud_rate)currentIndex);
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        int fd = -1;
        int port = 5760;
        QString address = "localhost";
        QString comport = "/dev/ttyUSB0";
        bool ttyconn = false;
        if(ui->ComPort->currentText().contains(':')) {
            address = ui->ComPort->currentText().split(":")[0];
            port = ui->ComPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            update();
            socket.connectToHost(address, port);
            socket.waitForConnected();
            if(socket.isValid()) {
                socket.setReadBufferSize(4096);
                fd = socket.socketDescriptor();
                if(fd > -1)
                    ahp_xc_connect_fd(fd);
            } else {
                ui->Connect->setEnabled(true);
                update();
            }
        } else {
            ahp_xc_connect(comport.toUtf8());
        }
        if(ahp_xc_is_connected()) {
            if(!ahp_xc_get_properties()) {
                connected = true;
                settings->beginGroup("Connection");
                settings->setValue("lastconnected", ui->ComPort->currentText());
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
                settings->setValue("connection", ui->ComPort->currentText());
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    QString name = "Line "+QString::number(l+1);
                    Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                    getGraph()->addSeries(Lines[l]->getDots());
                    getGraph()->addSeries(Lines[l]->getCounts());
                    getGraph()->addSeries(Lines[l]->getAutocorrelations());
                    getGraph()->addSeries(Lines[l]->getCrosscorrelations());
                    ui->Lines->addTab(Lines[l], name);
                }
                createPacket();
                setMode(Counter);
                ui->BaudRate->setEnabled(true);
                connect(readThread, static_cast<void (progressThread::*)(QWidget*)>(&progressThread::progressChanged), MainWindow::ReadThread);
                readThread->start();
                connect(uiThread, static_cast<void (progressThread::*)(QWidget*)>(&progressThread::progressChanged), MainWindow::UiThread);
                uiThread->start();
                ui->Connect->setEnabled(false);
                ui->Disconnect->setEnabled(true);
                ui->Scale->setValue(settings->value("Timescale", 0).toInt());
                ui->Scale->setEnabled(true);
            } else
                ahp_xc_disconnect();
        } else
            ahp_xc_disconnect();
    });
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);
   int starty = 15+ui->ComPort->y()+ui->ComPort->height();
   ui->Lines->setGeometry(5, starty+5, this->width()-10, ui->Lines->height());
   starty += 5+ui->Lines->height();
   getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
}

MainWindow::~MainWindow()
{
    uiThread->requestInterruption();
    uiThread->wait();
    uiThread->~progressThread();
    readThread->requestInterruption();
    readThread->wait();
    readThread->~progressThread();
    for(int l = 0; l < ahp_xc_get_nlines(); l++) {
        ahp_xc_set_leds(l, 0);
    }
    if(connected) {
        ahp_xc_clear_capture_flag(CAP_ENABLE);
        ui->Disconnect->clicked(false);
    }
    getGraph()->~Graph();
    delete ui;
}
