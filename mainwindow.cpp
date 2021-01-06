#include "mainwindow.h"
#include <ctime>
#include <cstring>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include "line.h"
#include "./ui_mainwindow.h"

void MainWindow::ReadThread(MainWindow *wnd)
{
    wnd->threadsRunning = true;
    QDateTime start = QDateTime::currentDateTimeUtc();
    ahp_xc_packet* packet = wnd->createPacket();
    int stepping = 1000000.0/ahp_xc_get_packettime();
    int *auto_lag = new int[packet->n_lines];
    while(wnd->threadsRunning) {
        if(!ahp_xc_get_packet(packet)) {
            QDateTime now = QDateTime::currentDateTimeUtc();
            double diff = (double)start.msecsTo(now)/1000.0;
            for(int x = 0; x < packet->n_lines; x++) {
                QLineSeries *dots = wnd->Lines[x]->getDots();/*
                if(diff > (double)wnd->getTimeRange())
                    for(int d = 0; d < dots->count(); d++)
                        if(dots->at(d).x()<diff-(double)wnd->getTimeRange())
                            dots->remove(d);*/
                if((wnd->Lines[x]->getFlags()&1)==1) {
                    if(auto_lag[x] < ahp_xc_get_delaysize()) {
                        dots->append(auto_lag[x], packet->autocorrelations[x].correlations[0].coherence);
                        wnd->getGraph()->Update();
                        auto_lag[x]++;
                        ahp_xc_set_lag_auto(x, auto_lag[x]);
                    }
                } else {
                    auto_lag[x] = 0;
                }
            }
        } else {
            QThread::msleep(ahp_xc_get_packettime()/1000);
        }
    }
    wnd->freePacket(packet);
    ahp_xc_enable_capture(0);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    connected = false;
    TimeRange = 60;
    ui->setupUi(this);
    graph = new Graph(this);
    int starty = ui->Lines->y()+ui->Lines->height()+5;
    getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
    getGraph()->setVisible(true);
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.length(); i++)
        ui->ComPort->addItem(ports[i].portName());
    if(ports.length() == 0)
        ui->ComPort->addItem("No ports available");
    ui->ComPort->setCurrentIndex(0);
    connect(ui->Disconnect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        threadsRunning = false;
        if(readThread.joinable())
            readThread.join();
        for(int l = 0; l < ahp_xc_get_nlines(); l++) {
            Lines[l]->~Line();
        }
        Lines.clear();
        getGraph()->clearSeries();
        ahp_xc_disconnect();
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [=](bool checked) {
        if(ui->ComPort->currentText() == "No ports available")
            return;
        char port[150] = {0};
        #ifndef _WIN32
            strcat(port, "/dev/");
        #endif
        strcat(port, ui->ComPort->currentText().toStdString().c_str());
        if(!ahp_xc_connect(port)) {
            if(!ahp_xc_get_properties()) {
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    char name[150];
                    sprintf(name, "Line %d", l+1);
                    Lines.append(new Line(name, l, ui->Lines));
                    ui->Lines->addTab(Lines[l], name);
                    getGraph()->addSeries(Lines[l]->getDots());
                }
                readThread = std::thread(MainWindow::ReadThread, this);
                connected = true;
                ui->Connect->setEnabled(false);
                ui->Disconnect->setEnabled(true);
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
    for(int l = 0; l < ahp_xc_get_nlines(); l++) {
        ahp_xc_set_leds(l, 0);
    }
    if(connected)
        ui->Disconnect->clicked(false);
    getGraph()->~Graph();
    delete ui;
}
