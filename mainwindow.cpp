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
    settings = new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings->setDefaultFormat(QSettings::Format::NativeFormat);
    settings->setUserIniPath(ini);
    connected = false;
    TimeRange = 10;
    ui->setupUi(this);
    uiThread = new Thread();
    readThread = new Thread(1,1);
    vlbiThread = new Thread(500);
    motorThread = new Thread(1000);
    graph = new Graph(this);
    int starty = ui->Lines->y() + ui->Lines->height() + 5;
    getGraph()->setGeometry(5, starty + 5, this->width() - 10, this->height() - starty - 10);
    getGraph()->setUpdatesEnabled(true);
    getGraph()->setVisible(true);

    settings->beginGroup("Connection");
    ui->XCPort->clear();
    ui->XCPort->addItem(settings->value("lastconnected", "localhost:5760").toString());
    QStringList devices = settings->value("devices", "").toString().split(",");
    settings->endGroup();
    for (int i = 0; i < devices.length(); i++)
    {
        settings->beginGroup(devices[i]);
        QString connection = settings->value("connection", "").toString();
        if(!connection.isEmpty())
            ui->XCPort->addItem(connection);
        settings->endGroup();
    }
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.length(); i++)
        ui->XCPort->addItem(
#ifndef _WIN32
            "/dev/" +
#endif
            ports[i].portName());
    ui->XCPort->setCurrentIndex(0);
    connect(ui->Mode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
            [ = ](int index)
    {
        setMode((Mode)index);
        for(int x = 0; x < Lines.count(); x++)
        {
            Lines[x]->setMode((Mode)index);
        }
    });
    connect(ui->Range, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [ = ](int value)
    {
        settings->setValue("Timerange", value);
        TimeRange = ui->Range->value();
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
        ui->Connect->setEnabled(true);
        ui->Disconnect->setEnabled(false);
        ui->Scale->setEnabled(false);
        if(!connected)
            return;
        freePacket();
        vlbi_exit(vlbi_context);
        for(int l = 0; l < ahp_xc_get_nlines(); l++)
        {
            Lines[l]->~Line();
        }
        Lines.clear();
        getGraph()->clearSeries();
        if(socket.isOpen())
        {
            socket.disconnectFromHost();
        }
        ahp_xc_disconnect();
        settings->endGroup();
        connected = false;
    });
    connect(ui->Connect, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked),
            [ = ](bool checked)
    {
        int fd = -1;
        int port = 5760;
        QString address = "localhost";
        bool ttyconn = false;
        if(ui->XCPort->currentText().contains(':'))
        {
            address = ui->XCPort->currentText().split(":")[0];
            port = ui->XCPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            update();
            socket.connectToHost(address, port);
            socket.waitForConnected();
            if(socket.isValid())
            {
                socket.setReadBufferSize(4096);
                fd = socket.socketDescriptor();
                if(fd > -1)
                    ahp_xc_connect_fd(fd);
            }
            else
            {
                ui->Connect->setEnabled(true);
                update();
            }
        }
        else
        {
            ahp_xc_connect(ui->XCPort->currentText().toUtf8(), false);
        }
        if(ahp_xc_is_connected())
        {
            if(!ahp_xc_get_properties())
            {
                connected = true;
                createPacket();
                setMode(Counter);
                settings->beginGroup("Connection");
                settings->setValue("lastconnected", ui->XCPort->currentText());
                QString header = ahp_xc_get_header();
                settings->setValue("lastdevice", header);
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
                settings->setValue("connection", ui->XCPort->currentText());
                vlbi_context = vlbi_init();
                vlbi_max_threads(QThreadPool::globalInstance()->maxThreadCount());
                for(int l = 0; l < ahp_xc_get_nlines(); l++)
                {
                    QString name = "Line " + QString::number(l + 1);
                    Lines.append(new Line(name, l, settings, ui->Lines, &Lines));
                    getGraph()->addSeries(Lines[l]->getSpectrum());
                    getGraph()->addSeries(Lines[l]->getMagnitude());
                    getGraph()->addSeries(Lines[l]->getPhase());
                    getGraph()->addSeries(Lines[l]->getCounts());
                    getGraph()->addSeries(Lines[l]->getMagnitudes());
                    getGraph()->addSeries(Lines[l]->getPhases());
                    vlbi_add_node(getVLBIContext(), Lines[l]->getStream(), (char*)name.toStdString().c_str(), 0);
                    ui->Lines->addTab(Lines[l], name);
                }
                int idx = 0;
                for(int l = 0; l < ahp_xc_get_nlines(); l++)
                {
                    for(int i = l + 1; i < ahp_xc_get_nlines(); i++)
                    {
                        QString name = "Baseline " + QString::number(l + 1) + "*" + QString::number(i + 1);
                        Baselines.append(new Baseline(name, idx, Lines[l], Lines[i], settings));
                    }
                }
                readThread->start();
                uiThread->start();
                vlbiThread->start();
                motorThread->start();
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
        motorFD = -1;/*
        if(ui->MotorPort->currentText().contains(':')) {
            address = ui->MotorPort->currentText().split(":")[0];
            port = ui->MotorPort->currentText().split(":")[1].toInt();
            ui->Connect->setEnabled(false);
            update();
            motor_socket.connectToHost(address, port);
            motor_socket.waitForConnected();
            if(motor_socket.isValid()) {
                motor_socket.setReadBufferSize(4096);
                motorFD = motor_socket.socketDescriptor();
                if(motorFD > -1) {
                    if(!ahp_gt_connect_fd(motorFD)) {
                        settings->setValue("motor_connection", ui->MotorPort->currentText());
                    }
                }
            } else {
                ui->Connect->setEnabled(true);
                update();
            }
        } else {
            motorFD = open(ui->MotorPort->currentText().toUtf8(), O_RDWR);
            if(!ahp_gt_connect_fd(motorFD)) {
                settings->setValue("motor_connection", ui->MotorPort->currentText());
            }
        }*/
    });
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        ahp_xc_packet* packet = getPacket();
        switch (getMode())
        {
            case Crosscorrelator:
                if(!ahp_xc_get_packet(packet))
                {
                    double offs_time = (double)packet->timestamp / 1000000000.0;
                    offs_time += getStartTime();
                    double offset1 = 0, offset2 = 0;
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        line->getStream()->location = (dsp_location*)realloc(line->getStream()->location,
                                                      sizeof(dsp_location) * (line->getStream()->len));
                        line->getStream()->location[line->getStream()->len - 1].xyz.x = line->getLocation()->xyz.x;
                        line->getStream()->location[line->getStream()->len - 1].xyz.y = line->getLocation()->xyz.y;
                        line->getStream()->location[line->getStream()->len - 1].xyz.z = line->getLocation()->xyz.z;
                    }
                    for(int x = 0; x < Baselines.count(); x++)
                    {
                        Baseline * line = Baselines[x];
                        if(line->isActive())
                        {
                            vlbi_get_offsets(getVLBIContext(), offs_time, (char*)line->getLine1()->getName().toStdString().c_str(),
                                             (char*)line->getLine2()->getName().toStdString().c_str(), getRa(), getDec(), &offset1, &offset2);
                            ahp_xc_set_lag_cross(line->getLine1()->getLineIndex(), (off_t)(offset1 * getFrequency()));
                            ahp_xc_set_lag_cross(line->getLine2()->getLineIndex(), (off_t)(offset2 * getFrequency()));
                            line->getMagnitude()->append(packet->crosscorrelations[x].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].magnitude);
                            line->getPhase()->append(packet->crosscorrelations[x].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].phase);
                        }
                        else
                        {
                            line->getMagnitude()->append(0.0);
                            line->getPhase()->append(0.0);
                        }
                    }
                }
                break;
            case Counter:
                if(!ahp_xc_get_packet(packet))
                {
                    double packettime = (double)packet->timestamp;
                    double diff = packettime - lastpackettime;
                    if(diff < 0 || diff > 1000000000)
                    {
                        resetTimestamp();
                        break;
                    }
                    lastpackettime = packettime;
                    packettime /= 1000000000.0;
                    packettime += J2000_starttime;
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        QLineSeries *counts[3] =
                        {
                            Lines[x]->getCounts(),
                            Lines[x]->getMagnitudes(),
                            Lines[x]->getPhases(),
                        };
                        for (int y = 0; y < 2; y++)
                        {
                            if(line->isActive())
                            {
                                if(counts[y]->count() > 0)
                                {
                                    for(int d = counts[y]->count() - 1; d >= 0; d--)
                                    {
                                        if(counts[y]->at(d).x() < packettime - (double)getTimeRange())
                                            counts[y]->remove(d);
                                    }
                                }
                                switch (y)
                                {
                                    case 0:
                                        if(Lines[x]->showCounts())
                                            counts[y]->append(packettime, packet->counts[x] * 1000000 / ahp_xc_get_packettime());
                                        break;
                                    case 1:
                                        if(Lines[x]->showAutocorrelations()) {
                                            counts[y]->append(packettime, packet->autocorrelations[x].correlations[0].magnitude * 1000000 / ahp_xc_get_packettime());
                                            counts[y+1]->append(packettime, packet->autocorrelations[x].correlations[0].phase * packet->autocorrelations[x].correlations[0].magnitude / M_PI*2);
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                            else
                            {
                                counts[y]->clear();
                            }
                        }
                    }
                }
                break;
            case Autocorrelator:
                for(int x = 0; x < Lines.count(); x++)
                {
                    Line * line = Lines[x];
                    if(line->isActive())
                    {
                        line->stackCorrelations();
                    }
                }
                break;
            case Spectrograph:
                if(!ahp_xc_get_packet(packet))
                {
                    for(int x = 0; x < Lines.count(); x++)
                    {
                        Line * line = Lines[x];
                        if(line->isActive())
                        {
                            if(packet->autocorrelations[x].correlations[0].magnitude > 0
                                    && packet->autocorrelations[x].correlations[0].magnitude < 1.0)
                            {
                                line->sumValue(packet->autocorrelations[x].correlations[0].magnitude, 1);
                            }
                        }
                        else
                        {
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
    connect(uiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [ = ](Thread * thread)
    {
        for(int x = 0; x < Lines.count(); x++)
            Lines.at(x)->paint();
        getGraph()->paint();
        settings->sync();
        thread->unlock();
    });
    connect(vlbiThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
        if(getMode() != Crosscorrelator)
            return;
        double radec [2] = { getRa(), getDec() };

        for(int i  = 0; i < Baselines.count(); i++)
        {
            Baseline* line = Baselines[i];
            if(line->getMagnitude()->count() > 0)
                vlbi_set_baseline_buffer(getVLBIContext(), (char*)line->getLine1()->getName().toStdString().c_str(),
                                         (char*)line->getLine2()->getName().toStdString().c_str(), line->getMagnitude()->toVector().data(),
                                         line->getMagnitude()->count());
        }
        vlbi_get_uv_plot(getVLBIContext(), "magnitude", getGraph()->getPlotWidth(), getGraph()->getPlotHeight(),
                         radec, getFrequency(), 1000000.0 / ahp_xc_get_packettime(), true, true, coverage_delegate);

        for(int i  = 0; i < Baselines.count(); i++)
        {
            Baseline* line = Baselines[i];
            if(line->getPhase()->count() > 0)
                vlbi_set_baseline_buffer(getVLBIContext(), (char*)line->getLine1()->getName().toStdString().c_str(),
                                         (char*)line->getLine2()->getName().toStdString().c_str(), line->getPhase()->toVector().data(),
                                         line->getMagnitude()->count());
        }
        vlbi_get_uv_plot(getVLBIContext(), "phase", getGraph()->getPlotWidth(), getGraph()->getPlotHeight(), radec,
                         getFrequency(), 1000000.0 / ahp_xc_get_packettime(), true, true, vlbi_default_delegate);
        vlbi_get_ifft(getVLBIContext(), "idft", "magnitude", "phase");
        dsp_stream_p idft_stream = vlbi_get_model(getVLBIContext(), "idft");
        dsp_stream_p magnitude_stream = vlbi_get_model(getVLBIContext(), "magnitude");
        dsp_stream_p phase_stream = vlbi_get_model(getVLBIContext(), "phase");
        QImage *coverage = getGraph()->getCoverage();
        QImage *idft = getGraph()->getIdft();
        QImage *raw = getGraph()->getRaw();
        dsp_buffer_stretch(idft_stream->buf, idft_stream->len, 0.0, 255.0);
        dsp_buffer_stretch(magnitude_stream->buf, idft_stream->len, 0.0, 255.0);
        dsp_buffer_stretch(phase_stream->buf, idft_stream->len, 0.0, 255.0);
        dsp_buffer_1sub(idft_stream, 255.0);
        dsp_buffer_1sub(magnitude_stream, 255.0);
        dsp_buffer_1sub(phase_stream, 255.0);
        dsp_buffer_copy(magnitude_stream->buf, coverage->bits(), magnitude_stream->len);
        dsp_buffer_copy(phase_stream->buf, raw->bits(), phase_stream->len);
        dsp_buffer_copy(idft_stream->buf, idft->bits(), idft_stream->len);
        dsp_stream_free_buffer(magnitude_stream);
        dsp_stream_free(magnitude_stream);
        dsp_stream_free_buffer(phase_stream);
        dsp_stream_free(phase_stream);
        dsp_stream_free_buffer(idft_stream);
        dsp_stream_free(idft_stream);
        thread->unlock();
    });
    connect(motorThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread * thread)
    {
    });
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
   QMainWindow::resizeEvent(event);
   int starty = 35+ui->XCPort->y()+ui->XCPort->height();
   ui->Lines->setGeometry(5, starty+5, this->width()-10, ui->Lines->height());
   starty += 5+ui->Lines->height();
   getGraph()->setGeometry(5, starty+5, this->width()-10, this->height()-starty-10);
}

MainWindow::~MainWindow()
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
    for(int l = 0; l < ahp_xc_get_nlines(); l++)
    {
        ahp_xc_set_leds(l, 0);
    }
    if(connected)
    {
        ahp_xc_clear_capture_flag(CAP_ENABLE);
        ui->Disconnect->clicked(false);
    }
    getGraph()->~Graph();
    delete ui;
}
