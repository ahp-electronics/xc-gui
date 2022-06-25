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

#include "line.h"
#include "ui_line.h"
#include <QMapNode>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>

QMutex Line::motor_mutex;

Line::Line(QString ln, int n, QSettings *s, QWidget *parent, QList<Line*> *p) :
    QWidget(parent),
    ui(new Ui::Line)
{
    setAccessibleName("Line");
    ui->setupUi(this);
    settings = s;
    localpercent = 0;
    localstop = 1;
    parents = p;
    name = ln;
    dark = new QMap<double, double>();
    magnitudeStack = new QMap<double, double>();
    countStack = new QMap<double, double>();
    phaseStack = new QMap<double, double>();
    magnitude = new QLineSeries();
    phase = new QLineSeries();
    magnitudes = new QLineSeries();
    phases = new QLineSeries();
    counts = new QLineSeries();
    elemental = new Elemental(this);
    elementalCounts = new Elemental(this);
    elementalPhase = new Elemental(this);
    elementalMagnitude = new Elemental(this);
    connect(elemental, static_cast<void (Elemental::*)(bool, double, double)>(&Elemental::scanFinished), this, &Line::plot);
    resetPercentPtr();
    resetStopPtr();
    getMagnitude()->setName(name + " magnitude");
    getPhase()->setName(name + " phase");
    getCounts()->setName(name + " (counts)");
    getPhases()->setName(name + " (magnitudes)");
    getMagnitudes()->setName(name + " (phases)");
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    stream->samplerate = 1.0/ahp_xc_get_packettime();
    line = n;
    flags = 0;
    ui->EndChannel->setRange(ahp_xc_get_frequency()/ahp_xc_get_delaysize(), ahp_xc_get_frequency() / 2 - 1);
    ui->StartChannel->setRange(ahp_xc_get_frequency()/ahp_xc_get_delaysize(), ahp_xc_get_frequency() / 2 - 3);
    ui->AutoChannel->setRange(ahp_xc_get_frequency()/ahp_xc_get_delaysize(), ahp_xc_get_frequency() / 2);
    ui->CrossChannel->setRange(ahp_xc_get_frequency()/ahp_xc_get_delaysize(), ahp_xc_get_frequency() / 2);
    readThread = new Thread(this, 0.01, 0.01, name+" read thread");
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ](Thread* thread)
    {
        Line * line = (Line *)thread->getParent();
        line->addCount();
        thread->requestInterruption();
        thread->unlock();
    });
    connect(ui->flag0, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        flags &= ~(1 << 0);
        flags |= ui->flag0->isChecked() << 0;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag0->text(), ui->flag0->isChecked());
    });
    connect(ui->flag1, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        flags &= ~(1 << 1);
        flags |= ui->flag1->isChecked() << 1;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag1->text(), ui->flag1->isChecked());
    });
    connect(ui->flag2, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        flags &= ~(1 << 2);
        flags |= ui->flag2->isChecked() << 2;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag2->text(), ui->flag2->isChecked());
    });
    connect(ui->flag3, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        flags &= ~(1 << 3);
        flags |= !ui->flag3->isChecked() << 3;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag3->text(), ui->flag3->isChecked());
    });
    connect(ui->flag4, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        flags &= ~(1 << 4);
        flags |= ui->flag4->isChecked() << 4;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag4->text(), ui->flag4->isChecked());
    });
    connect(ui->Save, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit savePlot();
    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit takeDark(this);
    });
    connect(ui->DFT, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        saveSetting("DFT", ui->DFT->isChecked());
    });
    connect(ui->ElementalAlign, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        saveSetting("ElementalAlign", ui->ElementalAlign->isChecked());
    });
    connect(ui->Resolution, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        Resolution = value;
        saveSetting("Resolution", Resolution);
    });
    connect(ui->AutoChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        AutoChannel = value;
        ahp_xc_set_channel_auto(getLineIndex(), fmin(ahp_xc_get_delaysize(), ahp_xc_get_frequency()/fmax(AutoChannel, 1)), 1, 0);
        saveSetting("AutoChannel", AutoChannel);
    });
    connect(ui->CrossChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        CrossChannel = value;
        ahp_xc_set_channel_cross(getLineIndex(), fmin(ahp_xc_get_delaysize(), ahp_xc_get_frequency()/fmax(CrossChannel, 1)), 1, 0);
        saveSetting("CrossChannel", CrossChannel);
    });
    connect(ui->Decimals, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        elemental->setDecimals(value);
        saveSetting("Decimals", ui->Decimals->value());
    });
    connect(ui->MinScore, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        elemental->setMinScore(value);
        saveSetting("MinScore", ui->MinScore->value());
    });
    connect(ui->MaxDots, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        elemental->setMaxDots(value);
        saveSetting("MaxDots", ui->MaxDots->value());
    });
    connect(ui->SampleSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        elemental->setSampleSize(value);
        saveSetting("SampleSize", ui->SampleSize->value());
    });
    connect(ui->StartChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartChannel->value() > ui->EndChannel->value() - 2)
        {
            ui->EndChannel->setValue(ui->StartChannel->value() + 2);
        }
        UpdateBufferSizes();
        emit updateBufferSizes();
        saveSetting("StartChannel", ui->StartChannel->value());
    });
    connect(ui->EndChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartChannel->value() > ui->EndChannel->value() - 2)
        {
            ui->StartChannel->setValue(ui->EndChannel->value() - 2);
        }
        UpdateBufferSizes();
        emit updateBufferSizes();
        saveSetting("EndChannel", ui->EndChannel->value());
    });
    connect(ui->Clear, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit clearCrosscorrelations();

        clearCorrelations();
        getMagnitude()->clear();
        getPhase()->clear();
        getMagnitudeStack()->clear();
        getPhaseStack()->clear();
        if(mode == Autocorrelator)
        {
            stack = 0.0;
            mx = 0.0;
        }
        emit activeStateChanged(this);
    });
    connect(ui->Counts, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        saveSetting(ui->Counts->text(), ui->Counts->isChecked());
    });
    connect(ui->Autocorrelations, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        saveSetting(ui->Autocorrelations->text(), ui->Autocorrelations->isChecked());
    });
    connect(ui->Crosscorrelations, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        saveSetting(ui->Crosscorrelations->text(), ui->Crosscorrelations->isChecked());
    });
    connect(ui->MountMotorIndex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        MountMotorIndex = value;
        if(ahp_gt_is_connected()) {
            ahp_gt_select_device(MountMotorIndex);
            if(!ahp_gt_detect_device()) {
                switch (ahp_gt_get_mount_type()) {
                case isMF:
                case isDOB:
                    fork = true;
                    break;
                default:
                    fork = false;
                    break;
                }
            }
        }
        saveSetting("MountMotorIndex", value);
    });
    connect(ui->RailMotorIndex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        RailMotorIndex = value;
        if(ahp_gt_is_connected()) {
            ahp_gt_select_device(RailMotorIndex);
            ahp_gt_detect_device();
        }
        saveSetting("RailMotorIndex", value);
    });
    connect(ui->x_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.x = (double)value / 1000.0;
        saveSetting("location_x", location.xyz.x);
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(RailMotorIndex)) {
                ahp_gt_select_device(RailMotorIndex);
                ahp_gt_goto_absolute(0, value*M_PI*2.0/ahp_gt_get_totalsteps(0), 800.0);
            }
        }
    });
    connect(ui->y_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.y = (double)value / 1000.0;
        saveSetting("location_y", location.xyz.y);
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(RailMotorIndex)) {
                ahp_gt_select_device(RailMotorIndex);
                ahp_gt_goto_absolute(1, value*M_PI*2.0/ahp_gt_get_totalsteps(1), 800.0);
            }
        }
    });
    connect(ui->z_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.z = (double)value / 1000.0;
        saveSetting("location_z", location.xyz.z);
    });
    connect(ui->Active, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked) {
        saveSetting("scan", ui->Active->isChecked());
    });
    ui->MountMotorIndex->setValue(readInt("MountMotorIndex", getLineIndex() * 2 + 1));
    ui->RailMotorIndex->setValue(readInt("RailMotorIndex", getLineIndex() * 2 + 2));

    ui->Active->setChecked(readBool("scan", false));
    ui->x_location->setValue(readDouble("location_x", 0.0) * 1000);
    ui->y_location->setValue(readDouble("location_y", 0.0) * 1000);
    ui->z_location->setValue(readDouble("location_z", 0.0) * 1000);
    ui->MinScore->setValue(readInt("MinScore", 50));
    ui->Decimals->setValue(readInt("Decimals", 0));
    ui->MaxDots->setValue(readInt("MaxDots", 10));
    ui->SampleSize->setValue(readInt("SampleSize", 5));
    ui->Resolution->setValue(readInt("Resolution", 1024));
    ui->AutoChannel->setValue(readInt("AutoChannel", ui->AutoChannel->maximum()));
    ui->CrossChannel->setValue(readInt("CrossChannel", ui->CrossChannel->maximum()));
    ui->EndChannel->setValue(readInt("EndChannel", ahp_xc_get_frequency()));
    ui->StartChannel->setValue(readInt("StartChannel", ui->StartChannel->minimum()));
    ui->DFT->setChecked(readBool("DFT", false));
    ui->flag4->setEnabled(!ahp_xc_has_differential_only());
    ahp_xc_set_leds(line, flags);
    setFlag(0, readBool(ui->flag0->text(), false));
    setFlag(1, readBool(ui->flag1->text(), false));
    setFlag(2, readBool(ui->flag2->text(), false));
    setFlag(3, readBool(ui->flag3->text(), false));
    setFlag(4, readBool(ui->flag4->text(), false));
    ui->Counts->setChecked(readBool(ui->Counts->text(), false));
    ui->Autocorrelations->setChecked(readBool(ui->Autocorrelations->text(), false));
    ui->Crosscorrelations->setChecked(readBool(ui->Crosscorrelations->text(), false));
    GetDark();
    getMagnitudeStack()->clear();
    getPhaseStack()->clear();
    setActive(false);
}

void Line::UpdateBufferSizes()
{
    start = fmin(ahp_xc_get_delaysize(), ahp_xc_get_frequency()/fmax(ui->EndChannel->value(), 1));
    end = fmin(ahp_xc_get_delaysize(), ahp_xc_get_frequency()/fmax(ui->StartChannel->value(), 1));
    len = end-start;
    step = fmax(1, round((double)len / Resolution));
    setMagnitudeSize(len);
    setPhaseSize(len);
}

void Line::runClicked(bool checked)
{
}

void Line::setLocation(dsp_location l)
{
    saveSetting("location_x", l.xyz.x);
    saveSetting("location_y", l.xyz.y);
    saveSetting("location_z", l.xyz.z);
}

void Line::updateLocation()
{
    if(stream != nullptr)
    {
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getRailIndex())) {
                lock();
                double x = ahp_gt_get_position(0);
                x *= ahp_gt_get_totalsteps(0);
                x /= M_PI / 2;
                x /= 1000.0;
                double y = ahp_gt_get_position(1);
                y *= ahp_gt_get_totalsteps(1);
                y /= M_PI / 2;
                y /= 1000.0;
                stream->location->xyz.x = x;
                stream->location->xyz.y = y;
                unlock();
            }
        }
    }
}

void Line::updateRa()
{
    if(stream != nullptr)
    {
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getMountIndex())) {
                double v = ahp_gt_get_position(0);
                v *= 12.0;
                v /= M_PI;
                setRa(v);
            }
        }
    }
}

void Line::updateDec()
{
    if(stream != nullptr)
    {
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getMountIndex())) {
                double v = ahp_gt_get_position(1);
                v *= 180.0;
                v /= M_PI;
                setDec(v);
            }
        }
    }
}

void Line::addCount()
{
    ahp_xc_packet *packet = getPacket();
    QLineSeries *counts[3] =
    {
        getCounts(),
        getMagnitudes(),
        getPhases(),
    };
    QMap<double, double>*stacks[3] =
    {
        getCountStack(),
        getMagnitudeStack(),
        getPhaseStack(),
    };
    Elemental *elementals[3] =
    {
        getCountElemental(),
        getMagnitudeElemental(),
        getPhaseElemental(),
    };
    switch(getMode()) {
    default: break;
    case HolographIQ:
    case HolographII:
    if(isActive())
    {
        dsp_stream_p stream = getStream();
        if(stream == nullptr) break;
        lock();
        dsp_stream_set_dim(stream, 0, stream->sizes[0] + 1);
        dsp_stream_alloc_buffer(stream, stream->len);
        stream->buf[stream->len - 1] = (double)packet->counts[getLineIndex()];
        memcpy(&stream->location[stream->len - 1], getLocation(), sizeof(dsp_location));
        unlock();
    }
    case Counter:
    for (int z = 0; z < 2; z++)
    {
        QLineSeries *Counts = counts[z];
        if(isActive())
        {
            if(Counts->count() > 0)
            {
                for(int d = Counts->count() - 1; d >= 0; d--)
                {
                    if(Counts->at(d).x() < getPacketTime() - getTimeRange())
                        Counts->remove(d);
                }
            }
            switch (z)
            {
                case 0:
                    if(showCounts()) {
                        Counts->append(getPacketTime(), (double)packet->counts[getLineIndex()] / ahp_xc_get_packettime());
                    }
                    break;
                case 1:
                    if(showAutocorrelations()) {
                        Counts->append(getPacketTime(), (double)packet->autocorrelations[getLineIndex()].correlations[0].magnitude / ahp_xc_get_packettime());
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
    break;
    case Spectrograph:
        for (int z = 0; z < 3; z++)
        {
            QLineSeries *Counts = counts[z];
            QMap<double, double> *Stack = stacks[z];
            Elemental *Elements = elementals[z];
            bool active = false;
            if(isActive())
            {
                Elements->setStreamSize(fmax(2, Elements->getStreamSize()+1));
                switch (z)
                {
                    case 0:
                        if(showCounts()) {
                            Elements->getStream()->buf[Elements->getStreamSize()-1] = (double)packet->counts[getLineIndex()] / ahp_xc_get_packettime();
                            active = true;
                        }
                        break;
                    case 1:
                        if(showAutocorrelations()) {
                            Elements->getStream()->buf[Elements->getStreamSize()-1] = (double)packet->autocorrelations[getLineIndex()].correlations[0].magnitude / ahp_xc_get_packettime();
                            active = true;
                        }
                        break;
                    case 2:
                        if(showAutocorrelations()) {
                            Elements->getStream()->buf[Elements->getStreamSize()-1] = (double)packet->autocorrelations[getLineIndex()].correlations[0].phase;
                            active = true;
                        }
                        break;
                    default:
                        break;
                }
            }
            else
            {
                Stack->clear();
                Elements->setStreamSize(2);
            }
            if(active) {
                Elements->getStream()->buf[0] = getMinFrequency();
                Elements->getStream()->buf[1] = getMaxFrequency();
                dsp_buffer_normalize(Elements->getStream()->buf, Elements->getStreamSize(), Elements->getStream()->buf[0], Elements->getStream()->buf[1]);
                int size = fmin(Elements->getStreamSize(), getResolution());
                dsp_t *buf = Elements->getStream()->buf;
                dsp_stream_set_buffer(Elements->getStream(), &buf[2], Elements->getStreamSize()-2);
                double *histo = dsp_stats_histogram(Elements->getStream(), size);
                dsp_stream_set_buffer(Elements->getStream(), buf, Elements->getStreamSize()+2);
                Counts->clear();
                for (int x = 1; x < size; x++)
                {
                    stackValue(Counts, Stack, x, Elements->getStream()->buf[0] + x * (Elements->getStream()->buf[1]-Elements->getStream()->buf[0]) / size, histo[x]);
                }
                free(histo);
            }
        }
    }
}

bool Line::isRailBusy()
{
    if(stream != nullptr)
    {
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getRailIndex())) {
                bool busy = ahp_gt_is_axis_moving(0);
                busy |= ahp_gt_is_axis_moving(1);
                return busy;
            }
        }
    }
    return false;
}

bool Line::isMountBusy()
{
    if(stream != nullptr)
    {
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getMountIndex())) {
                bool busy = ahp_gt_is_axis_moving(0);
                busy |= ahp_gt_is_axis_moving(1);
                return busy;
            }
        }
    }
    return false;
}

void Line::gotoRaDec(double ra, double dec) {
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(getMountIndex())) {
            timespec ts = vlbi_time_string_to_timespec(QDateTime::currentDateTimeUtc().toString(Qt::DateFormat::ISODate).toStdString().c_str());
            double j2000 = vlbi_time_timespec_to_J2000time(ts);
            double lst = vlbi_time_J2000time_to_lst(j2000, getLongitude());
            double ha = vlbi_astro_get_local_hour_angle(lst, ra);
            ha *= M_PI / 12.0;
            ha += M_PI / 2.0;
            dec *= M_PI / 180.0;
            dec -= M_PI / 2.0;
            if(!isForkMount()) {
                if(ha < M_PI * 3.0 / 2.0 && ha > M_PI / 2.0)
                    dec = -dec;
                if((ha > M_PI / 2.0 && ha < M_PI) || (ha > M_PI * 3.0 / 2.0 && ha < M_PI * 2.0)) {
                    flipMount(true);
                    ha = M_PI - ha;
                    dec = -dec;
                } else {
                    flipMount(false);
                }
                if(ha > M_PI) {
                    ha -= M_PI;
                }
            }
            ahp_gt_select_device(getMountIndex());
            ahp_gt_goto_absolute(0, ha, 800.0);
            ahp_gt_goto_absolute(1, dec, 800.0);
        }
    }
}

void Line::startTracking() {
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(getMountIndex())) {
            Line::motor_lock();
            ahp_gt_set_address(getMountIndex());
            ahp_gt_stop_motion(0, 1);
            ahp_gt_start_tracking(0);
            Line::motor_unlock();
        }
    }
}

void Line::startSlewing(double ra_rate, double dec_rate) {
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(getMountIndex())) {
            Line::motor_lock();
            ahp_gt_set_address(getMountIndex());
            if(ra_rate != 0.0) {
                ahp_gt_stop_motion(0, 1);
                ahp_gt_start_motion(0, ra_rate);
            }
            if(dec_rate != 0.0) {
                ahp_gt_stop_motion(1, 1);
                ahp_gt_start_motion(1, dec_rate);
            }
            Line::motor_unlock();
        }
    }
}

void Line::haltMotors() {
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(getMountIndex())) {
            Line::motor_lock();
            ahp_gt_set_address(getMountIndex());
            ahp_gt_stop_motion(0, 1);
            ahp_gt_stop_motion(1, 1);
            Line::motor_unlock();
        }
        if(ahp_gt_is_detected(getRailIndex())) {
            Line::motor_lock();
            ahp_gt_set_address(getRailIndex());
            ahp_gt_stop_motion(0, 1);
            ahp_gt_stop_motion(1, 1);
            Line::motor_unlock();
        }
    }
}

bool Line::haveSetting(QString setting)
{
    return settings->contains(QString(name + "_" + setting).replace(' ', ""));
}

void Line::removeSetting(QString setting)
{
    settings->remove(QString(name + "_" + setting).replace(' ', ""));
}

void Line::saveSetting(QString setting, QVariant value)
{
    settings->setValue(QString(name + "_" + setting).replace(' ', ""), value);
    settings->sync();
}

QVariant Line::readSetting(QString setting, QVariant defaultValue)
{
    return settings->value(QString(name + "_" + setting).replace(' ', ""), defaultValue);
}

void Line::setFlag(int flag, bool value)
{
    switch (flag)
    {
        case 0:
            ui->flag0->setChecked(value);
            break;
        case 1:
            ui->flag1->setChecked(value);
            break;
        case 2:
            ui->flag2->setChecked(value);
            break;
        case 3:
            ui->flag3->setChecked(value);
            break;
        case 4:
            ui->flag4->setChecked(value);
            break;
        default:
            break;
    }
}

void Line::resetTimestamp()
{
    dsp_stream_set_dim(stream, 0, 1);
    dsp_stream_alloc_buffer(stream, stream->len + 1);
    getStream()->starttimeutc = vlbi_time_string_to_timespec(QDateTime::currentDateTimeUtc().toString(
                                    Qt::DateFormat::ISODate).toStdString().c_str());
}

bool Line::getFlag(int flag)
{
    switch (flag)
    {
        case 0:
            return ui->flag0->isChecked();
        case 1:
            return ui->flag1->isChecked();
        case 2:
            return ui->flag2->isChecked();
        case 3:
            return ui->flag3->isChecked();
        case 4:
            return ui->flag4->isChecked();
        default:
            return false;
    }
}

bool Line::showCounts()
{
    return ui->Counts->isChecked();
}

bool Line::showAutocorrelations()
{
    return ui->Autocorrelations->isChecked();
}

bool Line::showCrosscorrelations()
{
    return ui->Crosscorrelations->isChecked();
}

void Line::setMode(Mode m)
{
    getDark()->clear();
    getMagnitude()->clear();
    getPhase()->clear();
    getCounts()->clear();
    getMagnitudes()->clear();
    getPhases()->clear();
    getCountStack()->clear();
    getMagnitudeStack()->clear();
    getPhaseStack()->clear();
    getCountElemental()->setStreamSize(1);
    getMagnitudeElemental()->setStreamSize(1);
    getPhaseElemental()->setStreamSize(1);
    getElemental()->setStreamSize(1);
    if(m != Counter && mode != Spectrograph)
    {
        stack = 0.0;
        mx = 0.0;
    }
    if(m == Autocorrelator)
    {
        connect(this, static_cast<void (Line::*)()>(&Line::savePlot), this, &Line::SavePlot);
        connect(this, static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Line::TakeDark);
    }
    else
    {
        disconnect(this, static_cast<void (Line::*)()>(&Line::savePlot), this, &Line::SavePlot);
        disconnect(this, static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Line::TakeDark);
    }
    ui->flag0->setEnabled(ahp_xc_has_leds());
    ui->flag1->setEnabled(ahp_xc_has_leds());
    resetPercentPtr();
    resetStopPtr();
    mode = m;
    switch(mode) {
    case HolographII:
    case HolographIQ:
        ui->Counts->setEnabled(false);
        ui->Autocorrelations->setEnabled(false);
        ui->Crosscorrelations->setEnabled(false);
        ui->Resolution->setEnabled(false);
        ui->Active->setEnabled(true);
        ui->DFT->setEnabled(false);
        ui->AutoChannel->setEnabled(false);
        ui->CrossChannel->setEnabled(false);
        ui->StartChannel->setEnabled(false);
        ui->EndChannel->setEnabled(false);
        break;
    case Spectrograph:
        ui->Counts->setEnabled(true);
        ui->Autocorrelations->setEnabled(true);
        ui->Crosscorrelations->setEnabled(true);
        ui->Resolution->setEnabled(true);
        ui->Active->setEnabled(false);
        ui->DFT->setEnabled(true);
        ui->AutoChannel->setEnabled(true);
        ui->CrossChannel->setEnabled(true);
        ui->StartChannel->setEnabled(true);
        ui->EndChannel->setEnabled(true);
        break;
    case Counter:
        ui->Counts->setEnabled(true);
        ui->Autocorrelations->setEnabled(true);
        ui->Crosscorrelations->setEnabled(true);
        ui->Resolution->setEnabled(false);
        ui->Active->setEnabled(false);
        ui->DFT->setEnabled(false);
        ui->AutoChannel->setEnabled(true);
        ui->CrossChannel->setEnabled(true);
        ui->StartChannel->setEnabled(false);
        ui->EndChannel->setEnabled(false);
        break;
    default:
        ui->Counts->setEnabled(false);
        ui->Autocorrelations->setEnabled(false);
        ui->Crosscorrelations->setEnabled(false);
        ui->Resolution->setEnabled(true);
        ui->Active->setEnabled(true);
        ui->DFT->setEnabled(true);
        ui->AutoChannel->setEnabled(false);
        ui->CrossChannel->setEnabled(false);
        ui->StartChannel->setEnabled(true);
        ui->EndChannel->setEnabled(true);
        break;
    }
    ui->Elemental->setEnabled(m != Counter && mode != HolographII && mode != HolographIQ);
}

void Line::paint()
{
    *stop = !isActive();
    if(ui->Progress != nullptr)
    {
        if(mode == Autocorrelator || mode == CrosscorrelatorIQ || mode == CrosscorrelatorII)
        {
            if(getPercent() > ui->Progress->minimum() && getPercent() < ui->Progress->maximum())
                ui->Progress->setValue(getPercent());
        }
    }
    update(rect());
}

void Line::addToVLBIContext()
{
    lock();
    resetTimestamp();
    vlbi_add_node(getVLBIContext(), getStream(), getName().toStdString().c_str(), false);
    unlock();
}

void Line::removeFromVLBIContext()
{
    if(stream != nullptr)
    {
        lock();
        vlbi_del_node(getVLBIContext(), getName().toStdString().c_str());
        unlock();
    }
}

void Line::stackValue(QLineSeries* series, QMap<double, double>* stacked, int idx, double x, double y)
{
    if(y == 0.0) return;
    y /= 2;
    if(getDark()->contains(x))
        y -= getDark()->value(x);
    if(stacked->count() > idx)
    {
        y += stacked->values().at(idx) / 2;
        stacked->keys().replace(idx, x);
        stacked->values().replace(idx, y);
    }
    else
    {
        stacked->insert(x, y);
    }
    series->append(x, y);
}

void Line::stretch(QLineSeries* series)
{
    double mx = DBL_MIN;
    double mn = DBL_MAX;
    for (int x = 0; x < series->count(); x++)
    {
        mn = fmin(series->at(x).y(), mn);
        mx = fmax(series->at(x).y(), mx);
    }
    for (int x = 0; x < series->count(); x++)
    {
        QPointF p = series->at(x);
        series->replace(x, p.x(), (p.y() - mn) * M_PI * 2.0 / (mx - mn));
    }
}

void Line::SavePlot()
{
    if(!isActive())
        return;
    QString filename = QFileDialog::getSaveFileName(this, "DialogTitle", "filename.csv",
                       "CSV files (.csv);Zip files (.zip, *.7z)", 0, 0); // getting the filename (full path)
    QFile data(filename);
    if(data.open(QFile::WriteOnly | QFile::Truncate))
    {
        QTextStream output(&data);
        output << "'lag (ns)';'magnitude';'phase'\n";
        for(int x = 0, y = 0; x < getMagnitude()->count(); y++, x++)
        {
            output << "'" + QString::number(getMagnitude()->at(y).x()) + "';'" + QString::number(getMagnitude()->at(y).y()) + "';'"
                   + QString::number(getPhase()->at(y).x()) + "';'" + QString::number(getPhase()->at(y).y()) + "'\n";
        }
    }
    data.close();
}

bool Line::scanActive() {
    return ui->Active->isChecked();
}

bool Line::Differential()
{
    return getFlag(4);
}

bool Line::dft()
{
    return ui->DFT->isChecked();
}

bool Line::Align()
{
    return ui->ElementalAlign->isChecked();
}

void Line::motor_lock()
{
    while(!motor_mutex.tryLock());
}

void Line::motor_unlock()
{
    motor_mutex.unlock();
}

void Line::setActive(bool a)
{
    activeStateChanging(this);
    if(a && ! running)
    {
        addToVLBIContext();
    }
    else if(!a && running)
    {
        removeFromVLBIContext();
    }
    if(a) {
        if(getMode() == Counter || getMode() == Spectrograph) {
            running = (showCounts() || showAutocorrelations());
        }
    }
    emit activeStateChanged(this);
}

bool Line::DarkTaken()
{
    if(ui->TakeDark->text() == "Apply Dark")
    {
        ui->TakeDark->setText("Clear Dark");
        return true;
    }
    else
    {
        ui->TakeDark->setText("Apply Dark");
        return false;
    }
}

void Line::GetDark()
{
    if(haveSetting("Dark"))
    {
        QStringList darkstring = readString("Dark", "").split(";");
        for(int x = 0; x < darkstring.length(); x++)
        {
            QStringList lag_value = darkstring[x].split(",");
            if(lag_value.length() == 2)
                getDark()->insert(lag_value[0].toDouble(), lag_value[1].toDouble());
        }
        if(getDark()->count() > 0)
        {
            ui->TakeDark->setText("Clear Dark");
            getMagnitude()->setName(name + " magnitude (residuals)");
        }
    }
}

void Line::TakeDark(Line *sender)
{
    if(sender->DarkTaken())
    {
        getDark()->clear();
        for(int x = 0; x < getMagnitude()->count(); x++)
        {
            getDark()->insert(getMagnitude()->at(x).x(), getMagnitude()->at(x).y());
            QString darkstring = readString("Dark", "");
            if(!darkstring.isEmpty())
                saveSetting("Dark", darkstring + ";");
            darkstring = readString("Dark", "");
            saveSetting("Dark", darkstring + QString::number(getMagnitude()->at(x).x()) + "," + QString::number(getMagnitude()->at(
                            x).y()));
        }
        getMagnitude()->setName(name + " magnitude (residuals)");
    }
    else
    {
        ui->TakeDark->setText("Apply Dark");
        removeSetting("Dark");
        getDark()->clear();
        getMagnitude()->setName(name + " magnitude");
    }
}

void Line::stackCorrelations(ahp_xc_sample *spectrum, int npackets)
{
    scanning = true;
    *stop = 0;
    *percent = 0;
    if(spectrum != nullptr && npackets > 0)
    {
        npackets--;
        int lag = 1;
        int _lag = lag;
        for (int x = 0, z = 1; x < npackets; x++, z++)
        {
            int lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime();
            if(lag < npackets && lag >= 0)
            {
                magnitude_buf[lag] = (double)spectrum[z].correlations[0].magnitude / ahp_xc_get_packettime();
                phase_buf[lag] = (double)spectrum[z].correlations[0].phase;
                for(int y = lag; y < npackets; y++)
                {
                    magnitude_buf[y] = magnitude_buf[lag];
                    phase_buf[y] = phase_buf[lag];
                }
                _lag = lag;
            }
        }
        elemental->setMagnitude(magnitude_buf, npackets);
        elemental->setPhase(phase_buf, npackets);
        if(dft())
            elemental->idft();
        if(Align())
            elemental->run();
        else
            elemental->finish(false, start/step, 1.0/step);
    }
    resetPercentPtr();
    resetStopPtr();
    scanning = false;
}

void Line::plot(bool success, double o, double s)
{
    double timespan = ahp_xc_get_sampletime() / s;
    double offset = o;
    getMagnitude()->clear();
    getPhase()->clear();
    stack += 1.0;
    for (int x = 0; x < elemental->getStreamSize(); x++)
    {
        if(dft()) {
            stackValue(getMagnitude(), getMagnitudeStack(), x, ((x + offset) * timespan), elemental->getBuffer()[x]);
        } else {
            stackValue(getMagnitude(), getMagnitudeStack(), x, 1.0 / ((x + offset) * timespan), elemental->getMagnitude()[x]);
        }
    }
}

Line::~Line()
{
    readThread->~Thread();
    elemental->~Elemental();
    delete ui;
}
