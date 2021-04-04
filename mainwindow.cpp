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

void MainWindow::ReadThread(MainWindow *wnd)
{
    wnd->threadsRunning = true;
    QDateTime start = QDateTime::currentDateTimeUtc();
    ahp_xc_packet* packet = wnd->createPacket();
    wnd->setMode(Counter);
    while(wnd->threadsRunning) {
        switch (wnd->getMode()) {
            case Counter:
            if(!ahp_xc_get_packet(packet)) {
                QDateTime now = QDateTime::currentDateTimeUtc();
                double diff = (double)start.msecsTo(now)/1000.0;
                for(int x = 0; x < packet->n_lines; x++) {
                    Line * line = wnd->Lines[x];
                    QLineSeries *dots = line->getDots();
                    QLineSeries *average = line->getAverage();
                    if(diff > (double)wnd->getTimeRange())
                        for(int d = 0; d < dots->count(); d++)
                            if(dots->at(d).x()<diff-(double)wnd->getTimeRange())
                                dots->remove(d);
                    for(int d = 0; d < average->count(); d++)
                        if(average->at(d).x()<diff-(double)wnd->getTimeRange())
                            average->remove(d);
                    if(line->isActive()) {
                            dots->append(diff, packet->counts[x]*1000000/ahp_xc_get_packettime());
                            average->append(diff, packet->counts[x]*1000000/ahp_xc_get_packettime());
                            wnd->getGraph()->Update();
                    } else {
                        dots->clear();
                        average->clear();
                    }
                }
            }
            break;
        case Autocorrelator:
            for(int x = 0; x < packet->n_lines; x++) {
                Line * line = wnd->Lines[x];
                if(line->isActive()) {
                    line->stackCorrelations();
                    wnd->getGraph()->Update();
                }
            }
            break;
        case Crosscorrelator:
            for(int x = 0; x < packet->n_baselines; x++) {
                Baseline * b = wnd->Baselines[x];
                if(b->isActive()) {
                    b->stackCorrelations((int)b->getLine1()->getYScale()>(int)b->getLine2()->getYScale() ? b->getLine1()->getYScale() : b->getLine2()->getYScale());
                    wnd->getGraph()->Update();
                    b->setActive(false);
                }
            }
            break;
        default:
            break;
        }
    }
    wnd->freePacket(packet);
    ahp_xc_enable_capture(0);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    QString homedir = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0);
    QString inidir = homedir+"/.xc-gui/";
    QString ini = homedir+"/.xc-gui/settings.ini";
    if(!QDir(inidir).exists()){
        QDir().mkdir(inidir);
    }
    settings = new QSettings(ini, QSettings::Format::IniFormat);

    setMode(Counter);
    connected = false;
    TimeRange = 10;
    ui->setupUi(this);
    graph = new Graph(this);
    int starty = ui->Lines->y()+ui->Lines->height()+5;
    getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
    getGraph()->setVisible(true);

    settings->beginGroup("Connection");
    ui->ComPort->addItem(settings->value("lastconnected", "localhost:5760").toString());
    settings->endGroup();
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
            bool state = Lines[x]->getFlag(0);
            Lines[x]->setFlag(0, false);
            Lines[x]->setFlag(0, state);
            Lines[x]->setMode((Mode)index);
        }
    });

    connect(ui->Scale, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int value) {
        ahp_xc_set_frequency_divider(value);
    });
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->Scale->setEnabled(false);
        threadsRunning = false;
        for(int l = 0; l < ahp_xc_get_nbaselines(); l++) {
            Baselines[l]->~Baseline();
        }
        Baselines.clear();
        for(int l = 0; l < ahp_xc_get_nlines(); l++) {
            Lines[l]->~Line();
        }
        Lines.clear();
        getGraph()->clearSeries();
        if(socket.isOpen())
            socket.disconnectFromHost();
        ahp_xc_disconnect();
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        int fd = -1;
        int port = 5760;
        QString address = "localhost";
        QString comport = "/dev/ttyUSB0";
        if(ui->ComPort->currentText().contains(':')) {
            address = ui->ComPort->currentText().split(":")[0];
            port = ui->ComPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            socket.connectToHost(address, port);
            socket.waitForConnected();
            ui->Connect->setEnabled(true);
            socket.setReadBufferSize(4096);
            if(socket.isOpen())
                fd = socket.socketDescriptor();
        } else {
            file.setFileName(comport);
            file.open(QIODevice::OpenModeFlag::ReadWrite|QIODevice::OpenModeFlag::ExistingOnly);
            if(file.isOpen()) {
                fd = file.handle();
            }
        }
        if(fd>=0) {
            ahp_xc_connect_fd(fd);
            if(!ahp_xc_get_properties()) {
                if(socket.isOpen())
                    socket.setReadBufferSize(ahp_xc_get_packetsize()*4);
                settings->beginGroup("Connection");
                settings->setValue("lastconnected", ui->ComPort->currentText());
                settings->endGroup();
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    char name[150];
                    sprintf(name, "Line %d", l+1);
                    Lines.append(new Line(name, l, ui->Lines, &Lines));
                    ui->Lines->addTab(Lines[l], name);
                    getGraph()->addSeries(Lines[l]->getDots());
                    ahp_xc_set_voltage(l, 0);
                }
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    for(int i = l+1; i < ahp_xc_get_nlines(); i++) {
                        char name[150];
                        sprintf(name, "%d*%d", l+1, i+1);
                        Baseline* b = new Baseline(name, Lines[l], Lines[i], this);
                        Baselines.append(b);
                        Lines[l]->addBaseline(b);
                        Lines[i]->addBaseline(b);
                        getGraph()->addSeries(b->getDots());
                    }
                }
                readThread = std::thread(MainWindow::ReadThread, this);
                readThread.detach();
                connected = true;
                ui->Connect->setEnabled(false);
                ui->Disconnect->setEnabled(true);
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
    threadsRunning = false;
    for(int l = 0; l < ahp_xc_get_nlines(); l++) {
        ahp_xc_set_leds(l, 0);
    }
    if(connected)
        ui->Disconnect->clicked(false);
    getGraph()->~Graph();
    delete ui;
}
