#include "types.h"
#include <cmath>
#include <ctime>
#include <cstring>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include "mainwindow.h"
#include "./ui_mainwindow.h"

void MainWindow::ReadThread(MainWindow *wnd)
{
    wnd->threadsRunning = true;
    QDateTime start = QDateTime::currentDateTimeUtc();
    ahp_xc_packet* packet = wnd->createPacket();
    int stepping = 1000000.0/ahp_xc_get_packettime();
    int *auto_lag = new int[packet->n_lines];
    bool skip[packet->n_lines];
    int average_value = 1;
    int averaged = average_value;
    double coherence = 0;
    double crosscoherence = 0;
    while(wnd->threadsRunning) {
        QThread::usleep(ahp_xc_get_packettime());
        if(wnd->getMode() == Autocorrelator)
        {
            for(int x = 0; x < packet->n_lines; x++) {
                Line * line = wnd->Lines[x];
                if(line->isActive())
                    line->stackCorrelations();
                else
                    line->clearCorrelations();
                wnd->getGraph()->Update();
            }
            continue;
        } else if(wnd->getMode() == Crosscorrelator)
        {
            for(int x = 0; x < packet->n_baselines; x++) {
                Baseline * b = wnd->Baselines[x];
                if(b->isActive())
                    b->stackCorrelations((int)wnd->Lines[b->getLine1()]->getYScale()>(int)wnd->Lines[b->getLine2()]->getYScale() ? wnd->Lines[b->getLine1()]->getYScale() : wnd->Lines[b->getLine2()]->getYScale());
                else
                    b->clearCorrelations();
                wnd->getGraph()->Update();
            }
            continue;
        } else if(!ahp_xc_get_packet(packet)) {
            QDateTime now = QDateTime::currentDateTimeUtc();
            double diff = (double)start.msecsTo(now)/1000.0;
            for(int x = 0; x < packet->n_lines; x++) {
                Line * line = wnd->Lines[x];
                QLineSeries *dots = line->getDots();
                QLineSeries *average = line->getAverage();
                switch (wnd->getMode()) {
                    case Counter:
                    if(diff > (double)wnd->getTimeRange())
                    for(int d = 0; d < dots->count(); d++)
                        if(dots->at(d).x()<diff-(double)wnd->getTimeRange())
                            dots->remove(d);
                    for(int d = 0; d < average->count(); d++)
                        if(average->at(d).x()<diff-(double)wnd->getTimeRange())
                            average->remove(d);
                    if(line->isActive()) {
                        if(!skip[x]) {
                            dots->append(diff, packet->counts[x]);
                            average->append(diff, packet->counts[x]);
                            wnd->getGraph()->Update();
                        }
                        skip[x] = false;
                    } else {
                        dots->clear();
                        average->clear();
                        skip[x] = true;
                        auto_lag[x] = 2;
                        ahp_xc_set_lag_auto(x, auto_lag[x]);
                    }
                        break;
                    default:
                        break;
                }
            }
        } else {
            ahp_xc_enable_capture(0);
            ahp_xc_enable_capture(1);
        }
    }
    wnd->freePacket(packet);
    ahp_xc_enable_capture(0);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setMode(Counter);
    connected = false;
    TimeRange = 10;
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
                    Lines.append(new Line(name, l, ui->Lines, &Lines));
                    ui->Lines->addTab(Lines[l], name);
                    getGraph()->addSeries(Lines[l]->getDots());
                    ahp_xc_set_voltage(l, 0);
                }
                for(int l = 0; l < ahp_xc_get_nlines(); l++) {
                    for(int i = l+1; i < ahp_xc_get_nlines(); i++) {
                        char name[150];
                        sprintf(name, "%d*%d", l+1, i+1);
                        Baseline* b = new Baseline(name, l, i, this);
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
