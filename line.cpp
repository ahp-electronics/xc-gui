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

#include <vlbi.h>
#include "line.h"
#include "mainwindow.h"
#include "ui_line.h"
#include <sys/time.h>
#include <QMapNode>
#include <QFile>
#include <QStandardItemModel>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>

QMutex Line::motor_mutex;

Line::Line(QString ln, int n, QSettings *s, QWidget *pw, QList<Line*> *p) :
    QWidget(pw),
    ui(new Ui::Line)
{
    setAccessibleName(ln);
    ui->setupUi(this);
    settings = s;
    localpercent = 0;
    localstop = 1;
    parents = p;
    name = ln;
    parent = pw;
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
    flags = (1 << 3);
    QStandardItemModel *model = new QStandardItemModel();
    model->setHorizontalHeaderLabels({"Catalog"});
    QFile index(
#ifdef _WIN32
    QCoreApplication::applicationDirPath()+"\\cat\\index.txt"
#else
    VLBI_CATALOG_PATH
#endif
                );
#ifdef _WIN32
    dir_separator = '\\';
#endif
    index.open(QFile::ReadOnly);
    QMap <QString, QString> rows;
    if(index.isOpen()) {
        int ncatalogs = 0;
        QString catname = index.readLine().replace("\n", "");
        while(!catname.isEmpty()) {
            QStandardItem *catalog = new QStandardItem(QString(catname).replace(dir_separator+QString("index.txt"), ""));
            QFileInfo fileInfo(index.fileName());
            QFile cat(fileInfo.dir().path()+dir_separator+catname);
            catname = catname.replace(dir_separator+QString("index.txt"), "");
            cat.open(QFile::ReadOnly);
            if(cat.isOpen()) {
                int nelement = 0;
                QString element = cat.readLine().replace(".txt\n", "");
                while(!element.isEmpty()) {
                    if(element != "index") {
                        QStandardItem *el = new QStandardItem(element);
                        el->setEditable(false);
                        rows.insert(catname+dir_separator+element, fileInfo.dir().path()+dir_separator+catname+dir_separator+element+".txt");
                        catalog->insertRow(nelement++, el);
                    }
                    element = cat.readLine().replace(".txt\n", "");
                }
                catalog->setData("");
                catalog->setEditable(false);
                model->insertRow(ncatalogs++, catalog);
                cat.close();
            }
            catname = index.readLine().replace("\n", "");
        }
        index.close();
        ui->Catalogs->setModel(model);
    }
    double frequency = ahp_xc_get_frequency();
    double delaysize = ahp_xc_get_delaysize();
    int min_frequency = frequency / delaysize;
    int max_frequency = frequency / 2 - 1;
    ui->EndChannel->setRange(min_frequency, max_frequency);
    ui->StartChannel->setRange(min_frequency, max_frequency - 2);
    ui->AutoChannel->setRange(min_frequency, max_frequency);
    ui->CrossChannel->setRange(min_frequency, max_frequency);
    connect(ui->Catalogs, static_cast<void (QTreeView::*)(const QModelIndex &)>(&QTreeView::clicked), [ = ] (const QModelIndex &index)
    {
        QString catalog = index.parent().data().toString();
        if(!catalog.isEmpty())
            elemental->loadSpectrum(rows[catalog+dir_separator+index.data().toString()]);
    });
    connect(ui->flag0, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        flags &= ~(1 << 0);
        flags |= checked << 0;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag0->text(), checked);
    });
    connect(ui->flag1, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        flags &= ~(1 << 1);
        flags |= checked << 1;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag1->text(), checked);
    });
    connect(ui->flag2, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        flags &= ~(1 << 2);
        flags |= checked << 2;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag2->text(), checked);
    });
    connect(ui->flag3, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        flags &= ~(1 << 3);
        flags |= !checked << 3;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag3->text(), checked);
    });
    connect(ui->flag4, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        flags &= ~(1 << 4);
        flags |= checked << 4;
        ahp_xc_set_leds(line, flags);
        saveSetting(ui->flag4->text(), checked);
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
        ui->MaxDots->setEnabled(ui->ElementalAlign->isChecked());
        ui->Decimals->setEnabled(ui->ElementalAlign->isChecked());
        ui->MinScore->setEnabled(ui->ElementalAlign->isChecked());
        ui->SampleSize->setEnabled(ui->ElementalAlign->isChecked());
    });
    connect(ui->Resolution, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        Resolution = value;
        saveSetting("Resolution", Resolution);
    });
    connect(ui->AutoChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        AutoChannel = value;
        ahp_xc_set_channel_auto(getLineIndex(), fmin(ahp_xc_get_delaysize(), ahp_xc_get_frequency()/2/fmax(AutoChannel, 1)), 1, 1);
        saveSetting("AutoChannel", AutoChannel);
    });
    connect(ui->CrossChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        CrossChannel = value;
        ahp_xc_set_channel_cross(getLineIndex(), fmin(ahp_xc_get_delaysize(), ahp_xc_get_frequency()/2/fmax(CrossChannel, 1)), 1, 1);
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
        setMinFrequency(ui->StartChannel->value());
        saveSetting("StartChannel", ui->StartChannel->value());
    });
    connect(ui->EndChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartChannel->value() > ui->EndChannel->value() - 2)
        {
            ui->StartChannel->setValue(ui->EndChannel->value() - 2);
        }
        setMaxFrequency(ui->EndChannel->value());
        saveSetting("EndChannel", ui->EndChannel->value());
    });
    connect(ui->Clear, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit clearCrosscorrelations();

        clearCorrelations();
        clearCounts();
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
        setLocation();
        saveSetting("location_x", getLocation()->xyz.x);
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getRailIndex())) {
                ahp_gt_select_device(getRailIndex());
                ahp_gt_goto_absolute(0, value*M_PI*2.0/ahp_gt_get_totalsteps(0), 800.0);
            }
        }
    });
    connect(ui->y_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        setLocation();
        saveSetting("location_y", getLocation()->xyz.y);
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getRailIndex())) {
                ahp_gt_select_device(getRailIndex());
                ahp_gt_goto_absolute(1, value*M_PI*2.0/ahp_gt_get_totalsteps(1), 800.0);
            }
        }
    });
    connect(ui->z_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        setLocation();
        saveSetting("location_z", getLocation()->xyz.z);
    });
    connect(ui->Smooth, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        _smooth = Resolution * value / 25;
        saveSetting("Smooth", _smooth);
    });
    connect(ui->Active, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked) {
        saveSetting("scan", ui->Active->isChecked());
        emit scanActiveStateChanging(this);
        emit scanActiveStateChanged(this);
    });
}

void Line::Initialize()
{

    ui->MountMotorIndex->setValue(readInt("MountMotorIndex", getLineIndex() * 2 + 1));
    ui->RailMotorIndex->setValue(readInt("RailMotorIndex", getLineIndex() * 2 + 2));

    ui->Active->setChecked(readBool("scan", false));
    ui->x_location->setValue(readDouble("location_x", 0.0) * 1000);
    ui->y_location->setValue(readDouble("location_y", 0.0) * 1000);
    ui->z_location->setValue(readDouble("location_z", 0.0) * 1000);
    ui->MinScore->setValue(readInt("MinScore", 50));
    ui->Decimals->setValue(readInt("Decimals", 0));
    ui->MaxDots->setValue(readInt("MaxDots", 10));
    ui->Smooth->setValue(readInt("Smooth", 5));
    ui->SampleSize->setValue(readInt("SampleSize", 5));
    if(ahp_xc_get_delaysize() <= 4)
        ui->Resolution->setRange(1, 1048576);
    else
        ui->Resolution->setRange(1, ahp_xc_get_delaysize());
    ui->Resolution->setValue(readInt("Resolution", 100));
    ui->AutoChannel->setValue(readInt("AutoChannel", ui->AutoChannel->maximum()));
    ui->CrossChannel->setValue(readInt("CrossChannel", ui->CrossChannel->maximum()));
    ui->EndChannel->setValue(readInt("EndChannel", ahp_xc_get_frequency()));
    ui->StartChannel->setValue(readInt("StartChannel", ui->StartChannel->minimum()));
    ui->DFT->setChecked(readBool("DFT", false));
    ui->flag3->setEnabled(!ahp_xc_has_cumulative_only());
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
    step = fmax(1, round((double)len / getResolution()));
    setMagnitudeSize(getNumChannels());
    setPhaseSize(getNumChannels());
    emit updateBufferSizes();
}

void Line::runClicked(bool checked)
{
}

void Line::updateLocation()
{
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(getRailIndex())) {
            ahp_gt_select_device(getRailIndex());
            double x = ahp_gt_get_position(0, nullptr);
            x *= ahp_gt_get_totalsteps(0);
            x /= M_PI / 2;
            x /= 1000.0;
            double y = ahp_gt_get_position(1, nullptr);
            y *= ahp_gt_get_totalsteps(1);
            y /= M_PI / 2;
            y /= 1000.0;
        }
    }
    setLocation();
}

void Line::setLocation(int value)
{
    (void)value;
    if(MainWindow::lock_vlbi()) {
        if(stream != nullptr)
        {
            getLocation()->xyz.x = ui->x_location->value() / 1000;
            getLocation()->xyz.y = ui->y_location->value() / 1000;
            getLocation()->xyz.z = ui->z_location->value() / 1000;
            saveSetting("location_x", getLocation()->xyz.x);
            saveSetting("location_y", getLocation()->xyz.y);
            saveSetting("location_z", getLocation()->xyz.z);
            MainWindow::unlock_vlbi();
        }
    }
}

void Line::updateRa()
{
    if(stream != nullptr)
    {
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(getMountIndex())) {
                double v = ahp_gt_get_position(0, nullptr);
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
                double v = ahp_gt_get_position(1, nullptr);
                v *= 180.0;
                v /= M_PI;
                setDec(v);
            }
        }
    }
}

void Line::addCount(double starttime, ahp_xc_packet *packet)
{
    if(packet == nullptr)
        packet = getPacket();
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
    break;
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
                    if(Counts->at(d).x() < packet->timestamp + starttime - getTimeRange())
                        Counts->remove(d);
                }
            }
            switch (z)
            {
                case 0:
                    if(showCounts()) {
                        Counts->append(packet->timestamp + starttime, (double)packet->counts[getLineIndex()] / ahp_xc_get_packettime());
                    }
                    break;
                case 1:
                    if(showAutocorrelations()) {
                        Counts->append(packet->timestamp + starttime, (double)packet->autocorrelations[getLineIndex()].correlations[0].magnitude / ahp_xc_get_packettime());
                    }
                    break;
                default:
                    break;
            }
            smoothBuffer(Counts, Counts->count()-1, 1);
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
                if(Elements->lock()) {
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
                    Elements->unlock();
                }
            }
            else
            {
                Elements->clear();
                Stack->clear();
            }
            if(active)
            {
                if(Elements->lock()) {
                    Elements->getStream()->buf[0] = getMinFrequency();
                    Elements->getStream()->buf[1] = getMaxFrequency();
                    Elements->unlock();
                }
                Elements->normalize(Elements->getStream()->buf[0], Elements->getStream()->buf[1]);
                stack_index ++;
                int size = fmin(Elements->getStreamSize(), getResolution());
                double *histo = Elements->histogram(size);
                double mn = Elements->min(2, Elements->getStream()->len-2);
                double mx = Elements->max(2, Elements->getStream()->len-2);
                Counts->clear();
                for (int x = 1; x < size; x++)
                {
                    stackValue(Counts, Stack, x * (mx-mn) / size + mn, histo[x]);
                }
                smoothBuffer(Counts, 0, Counts->count());
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
            ahp_gt_select_device(getMountIndex());
            ahp_gt_set_location(Latitude, Longitude, Elevation);
            ahp_gt_goto_radec(ra, dec);
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
        if(ui->flag0->isChecked() != value)
            ui->flag0->click();
            break;
        case 1:
        if(ui->flag1->isChecked() != value)
            ui->flag1->click();
            break;
        case 2:
        if(ui->flag2->isChecked() != value)
            ui->flag2->click();
            break;
        case 3:
        if(ui->flag3->isChecked() != value)
            ui->flag3->click();
            break;
        case 4:
        if(ui->flag4->isChecked() != value)
            ui->flag4->click();
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
    *stop = 1;
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
    emit scanActiveStateChanging(this);
    emit scanActiveStateChanged(this);
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
    resetTimestamp();
    if(getMode() != HolographII && getMode() != HolographIQ) return;
    if(!vlbi_has_node(getVLBIContext(), getName().toStdString().c_str()))
        vlbi_add_node(getVLBIContext(), getStream(), getName().toStdString().c_str(), false);
}

void Line::removeFromVLBIContext()
{
    if(stream != nullptr && vlbi_has_node(getVLBIContext(), getName().toStdString().c_str()))
    {
        vlbi_del_node(getVLBIContext(), getName().toStdString().c_str());
    }
}

void Line::stackValue(QLineSeries* series, QMap<double, double>* stacked, double x, double y)
{
    if(y == 0.0) return;
    y /= stack_index;
    if(getDark()->contains(x))
        y -= getDark()->value(x);
    if(stacked->contains(x))
    {
        y += stacked->value(x) * (stack_index-1) / stack_index;
    }
    stacked->insert(x, y);
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
        QMap <double, double> counts, mag, phi;
        switch(getMode()) {
        case Counter:
            output << "time (s)";
            output << ",counts";
            output << ",autocorrelations (magnitude),autocorrelations (phase)";
            output << "\n";
            for(int x = 0; x < getCounts()->count(); x++)
                counts.insert(getCounts()->at(x).x(), getCounts()->at(x).y());
            for(int x = 0; x < getMagnitudes()->count(); x++) {
                mag.insert(getMagnitudes()->at(x).x(), getMagnitudes()->at(x).y());
                phi.insert(getPhases()->at(x).x(), getPhases()->at(x).y());
            }
            for(int i = 0, x = 0, y = 0, z = 0; i < fmax(fmax(getCounts()->count(), getMagnitudes()->count()), getPhases()->count()); i++)
            {
                double ctime = DBL_MAX, mtime = DBL_MAX, ptime = DBL_MAX;
                if(counts.count() > x)
                    ctime = counts.keys().at(x);
                if(mag.count() > y)
                    mtime = mag.keys().at(y);
                if(phi.count() > y)
                    ptime = phi.keys().at(z);
                double time = fmin(fmin(ctime, mtime), ptime);
                output << QString::number(time);
                if(counts.keys().at(x) == time) {
                    output << ","+QString::number(counts[time]);
                    x++;
                }
                if(showAutocorrelations())
                output << ",";
                if(mag.keys().at(y) == time) {
                    output << QString::number(mag[time]);
                    y++;
                }
                output << ",";
                if(phi.keys().at(z) == time) {
                    output << QString::number(phi[time]);
                    z++;
                }
                output << "\n";
            }
            break;
        case Spectrograph:
        case Autocorrelator:
            if(dft())
                output << "'lag (ns)','magnitude','phase'\n";
            else
                output << "'channel','magnitude','phase'\n";
            for(int x = 0, y = 0; x < getMagnitude()->count(); y++, x++)
            {
                output << "'" + QString::number(getMagnitude()->at(y).x()) + "','" + QString::number(getMagnitude()->at(y).y()) + "','"
                       + QString::number(getPhase()->at(y).x()) + "','" + QString::number(getPhase()->at(y).y()) + "'\n";
            }
            break;
        default:
            break;
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
    if(getMode() == Counter || getMode() == Spectrograph) {
        if(a)
            running = (showCounts() || showAutocorrelations());
    } else running = a;
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

void Line::smoothBuffer(QLineSeries* buf, int offset, int len)
{
    if(buf->count() < offset+len)
        return;
    if(buf->count() < smooth())
        return;
    offset = fmax(offset, smooth());
    for(int x = offset/2; x < len-offset/2; x++) {
        double val = 0.0;
        for(int y = -offset/2; y < offset/2; y++) {
            val += buf->at(x-y).y();
        }
        val /= smooth();
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), val);
    }
    if(getMode() == Autocorrelator || getMode() == CrosscorrelatorII || getMode() == CrosscorrelatorIQ) {
        for(int x = len-1; x >= len-offset/2; x--)
            buf->remove(buf->at(x).x(), buf->at(x).y());
        for(int x = offset/2; x >= 0; x--)
            buf->remove(buf->at(x).x(), buf->at(x).y());
    }
}

void Line::smoothBuffer(double* buf, int len)
{
    for(int x = smooth(); x < len; x++) {
        for(int y = 0; y < smooth(); y++)
            buf[x] += buf[x-y];
        buf[x] /= smooth();
    }
}

void Line::stackCorrelations(ahp_xc_sample *spectrum)
{
    scanning = true;
    *stop = 0;
    *percent = 0;
    int npackets = getNumChannels();
    if(spectrum != nullptr && npackets > 0)
    {
        stack_index ++;
        npackets--;
        npackets--;
        int lag = 1;
        int _lag = lag;
        dsp_buffer_set(magnitude_buf, npackets, 0);
        dsp_buffer_set(phase_buf, npackets, 0);
        for (int x = 0, z = 2; x < npackets; x++, z++)
        {
            int lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime();
            ahp_xc_correlation correlation;
            memcpy(&correlation, &spectrum[z].correlations[0], sizeof(ahp_xc_correlation));
            if(correlation.magnitude > 0) {
                if(lag < npackets && lag >= 0)
                {
                    magnitude_buf[lag] = (double)correlation.magnitude / correlation.counts;
                    phase_buf[lag] = (double)correlation.phase;
                    for(int y = lag; y < npackets; y++)
                    {
                        magnitude_buf[y] = magnitude_buf[lag];
                        phase_buf[y] = phase_buf[lag];
                    }
                    _lag = lag;
                }
            }
        }
        elemental->setBuffer(magnitude_buf, npackets);
        elemental->setMagnitude(magnitude_buf, npackets);
        elemental->setPhase(phase_buf, npackets);
        if(dft())
            elemental->dft();
        if(Align())
            elemental->run();
        else
            elemental->finish(false, start, step);
    }
    resetPercentPtr();
    resetStopPtr();
    scanning = false;
}

void Line::plot(bool success, double o, double s)
{
    double timespan = step;
    if(success)
        timespan = s;
    double offset = o;
    int x = 0;
    getMagnitude()->clear();
    getPhase()->clear();
    for (double t = offset + 1; x < elemental->getStreamSize(); t += timespan, x++)
    {
        stackValue(getMagnitude(), getMagnitudeStack(), ahp_xc_get_sampletime() * t, elemental->getBuffer()[x]);
    }
    smoothBuffer(getMagnitude(), 0, getMagnitude()->count());
    smoothBuffer(getPhase(), 0, getPhase()->count());
}

Line::~Line()
{
    ahp_xc_set_leds(getLineIndex(), 0);
    elemental->~Elemental();
    delete ui;
}
