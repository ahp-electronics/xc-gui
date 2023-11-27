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
    spectrum = new Series();
    counts = new Series();
    connect(getSpectrum()->getElemental(), static_cast<void (Elemental::*)(bool, double, double)>(&Elemental::scanFinished), this, &Line::plot);
    resetPercentPtr();
    resetStopPtr();
    getSpectrum()->setName(name + " spectrum");
    getCounts()->setName(name + " (counts)");
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
    int min_lag = 1.0 * 1000000000.0 * ahp_xc_get_sampletime();
    int max_lag = ahp_xc_get_delaysize() * 1000000000.0 * ahp_xc_get_sampletime();
    ui->EndChannel->setRange(min_lag, max_lag);
    ui->StartChannel->setRange(min_lag, max_lag - 2);
    ui->AutoChannel->setRange(min_lag, max_lag);
    ui->CrossChannel->setRange(min_lag, max_lag);
    connect(ui->Catalogs, static_cast<void (QTreeView::*)(const QModelIndex &)>(&QTreeView::clicked), [ = ] (const QModelIndex &index)
    {
        QString catalog = index.parent().data().toString();
        if(!catalog.isEmpty())
            getSpectrum()->getElemental()->loadSpectrum(rows[catalog+dir_separator+index.data().toString()]);
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
    connect(ui->loadRail, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this, [ = ](bool checked)
    {
        emit loadPositionChart();
    });
    connect(ui->clearRail, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this, [ = ](bool checked)
    {
        emit unloadPositionChart();
    });
    connect(ui->Save, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit savePlot();
    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit takeDark(this);
    });
    connect(ui->iDFT, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        saveSetting("iDFT", ui->iDFT->isChecked());
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
        ahp_xc_set_channel_auto(getLineIndex(), fmin(ahp_xc_get_delaysize(), fmax(AutoChannel*0.000000001/ahp_xc_get_sampletime(), 0)), 1, 1);
        saveSetting("AutoChannel", AutoChannel);
    });
    connect(ui->CrossChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        CrossChannel = value;
        ahp_xc_set_channel_cross(getLineIndex(), fmin(ahp_xc_get_delaysize(), fmax(CrossChannel*0.000000001/ahp_xc_get_sampletime(), 0)), 1, 1);
        saveSetting("CrossChannel", CrossChannel);
    });
    connect(ui->Decimals, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getSpectrum()->getElemental()->setDecimals(value);
        saveSetting("Decimals", ui->Decimals->value());
    });
    connect(ui->MinScore, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getSpectrum()->getElemental()->setMinScore(value);
        saveSetting("MinScore", ui->MinScore->value());
    });
    connect(ui->MaxDots, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getSpectrum()->getElemental()->setMaxDots(value);
        saveSetting("MaxDots", ui->MaxDots->value());
    });
    connect(ui->SampleSize, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getSpectrum()->getElemental()->setSampleSize(value);
        saveSetting("SampleSize", ui->SampleSize->value());
    });
    connect(ui->MinValue, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        saveSetting("MinValue", ui->MinValue->value());
    });
    connect(ui->StartChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartChannel->value() > ui->EndChannel->value() - 2)
        {
            ui->EndChannel->setValue(ui->StartChannel->value() + 2);
        }
        lag_start = fmin(ahp_xc_get_delaysize() * 1000000000.0 * ahp_xc_get_sampletime(), (double)ui->StartChannel->value());
        setMaxFrequency(ahp_xc_get_frequency() * 1000000000.0 / lag_start);
        setBufferSizes();
        saveSetting("StartChannel", ui->StartChannel->value());
    });
    connect(ui->EndChannel, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartChannel->value() > ui->EndChannel->value() - 2)
        {
            ui->StartChannel->setValue(ui->EndChannel->value() - 2);
        }
        lag_end = fmin(ahp_xc_get_delaysize() * 1000000000.0 * ahp_xc_get_sampletime(), (double)ui->EndChannel->value());
        setMinFrequency(ahp_xc_get_frequency() * 1000000000.0 / lag_end);
        setBufferSizes();
        saveSetting("EndChannel", ui->EndChannel->value());
    });
    connect(ui->Clear, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit clear();

        clearCorrelations();
        clearCounts();
    });
    connect(ui->RadixY, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        radix_y = checked;
        getGraph()->setupAxes(radix_x ? 10.0 : 1.0, radix_y ? 10.0 : 1.0, "", "", "%d", "%.03lf");
        gethistogram()->setupAxes(radix_x ? 10.0 : 1.0, radix_y ? 10.0 : 1.0, "", "", "%d", "%.03lf");
        saveSetting("RadixY", radix_y);
    });
    connect(ui->RadixX, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        radix_x = checked;
        getGraph()->setupAxes(radix_x ? 10.0 : 1.0, radix_y ? 10.0 : 1.0, "", "", "%d", "%03lf");
        gethistogram()->setupAxes(radix_x ? 10.0 : 1.0, radix_y ? 10.0 : 1.0, "", "", "%d", "%.03lf");
        saveSetting("RadixX", ui->Counts->isChecked());
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
    connect(ui->LoPass, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        _smooth = Resolution * value / 25;
        saveSetting("LoPass", value);
    });
    connect(ui->Active, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked) {
        saveSetting("scan", ui->Active->isChecked());
        emit scanActiveStateChanging(this);
        emit scanActiveStateChanged(this);
    });
    connect(this, static_cast<void (Line::*)()>(&Line::savePlot), this, &Line::SavePlot);
    connect(this, static_cast<void (Line::*)()>(&Line::loadPositionChart), this, &Line::LoadPositionChart);
    connect(this, static_cast<void (Line::*)()>(&Line::unloadPositionChart), this, &Line::UnloadPositionChart);
    Initialize();
}

void Line::Initialize()
{

    ui->MountMotorIndex->setValue(readInt("MountMotorIndex", getLineIndex() * 2 + 1));
    ui->RailMotorIndex->setValue(readInt("RailMotorIndex", getLineIndex() * 2 + 2));

    ui->Active->setChecked(readBool("scan", false));
    ui->z_location->setValue(readDouble("location_z", 0.0) * 1000);
    ui->MinScore->setValue(readInt("MinScore", 50));
    ui->Decimals->setValue(readInt("Decimals", 0));
    ui->MaxDots->setValue(readInt("MaxDots", 10));
    ui->LoPass->setValue(readInt("LoPass", 0));
    ui->SampleSize->setValue(readInt("SampleSize", 5));
    ui->Resolution->setRange(1000000000.0 * ahp_xc_get_sampletime(), ahp_xc_get_delaysize() * 1000000000.0 * ahp_xc_get_sampletime());
    ui->Resolution->setValue(readInt("Resolution", 100));
    ui->AutoChannel->setValue(readInt("AutoChannel", ui->AutoChannel->maximum()));
    ui->CrossChannel->setValue(readInt("CrossChannel", ui->CrossChannel->maximum()));
    ui->EndChannel->setValue(readInt("EndChannel", ahp_xc_get_frequency()));
    ui->StartChannel->setValue(readInt("StartChannel", ui->StartChannel->minimum()));
    ui->MinValue->setValue(readInt("MinValue", ui->MinValue->minimum()));
    ui->iDFT->setChecked(readBool("iDFT", false));
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
    setActive(false);
    setLocation();
}

void Line::setBufferSizes()
{
    lag_len = lag_end - lag_start;
    lag_step = fmax(1, lag_len / getResolution());
    channel_start = lag_start / ahp_xc_get_sampletime() / 1000000000.0;
    channel_end = lag_end / ahp_xc_get_sampletime() / 1000000000.0;
    channel_len = lag_len / ahp_xc_get_sampletime() / 1000000000.0;
    channel_step = fmax(1, lag_step / ahp_xc_get_sampletime() / 1000000000.0);
    Resolution = fmin(getResolution(), channel_len / channel_step);
    setSpectrumSize(getResolution());
    emit updateBufferSizes();
}

void Line::runClicked(bool checked)
{
    current_location = 0;
}

void Line::updateLocation()
{
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(getRailIndex())) {
            ahp_gt_select_device(getRailIndex());
            if(ahp_gt_is_axis_moving(0)) {
                double x = ahp_gt_get_position(0, nullptr);
                x *= ahp_gt_get_totalsteps(0);
                x /= M_PI * 2;
                x /= 1000.0;
                getLocation()->xyz.x = x;
            } else
                getLocation()->xyz.x = xyz_locations[current_location].xyz.x;
            if(ahp_gt_is_axis_moving(1)) {
                double y = ahp_gt_get_position(1, nullptr);
                y *= ahp_gt_get_totalsteps(1);
                y /= M_PI * 2;
                y /= 1000.0;
                getLocation()->xyz.y = y;
            } else
                getLocation()->xyz.y = xyz_locations[current_location].xyz.y;
        }
    } else
        dsp_buffer_copy(xyz_locations[current_location].coordinates, getLocation()->coordinates, 3);
}

void Line::setLocation(int value)
{
    (void)value;
    while(!MainWindow::lock_vlbi());
    if(stream != nullptr)
    {
        if(xyz_locations.length() > current_location) {
            bool update_location = false;
            update_location |= (targetLocation()->xyz.x != xyz_locations[current_location].xyz.x);
            update_location |= (targetLocation()->xyz.y != xyz_locations[current_location].xyz.y);
            update_location |= (targetLocation()->xyz.x == getLocation()->xyz.x);
            update_location |= (targetLocation()->xyz.y == getLocation()->xyz.y);
            targetLocation()->xyz.x = xyz_locations[current_location].xyz.x;
            targetLocation()->xyz.y = xyz_locations[current_location].xyz.y;
            targetLocation()->xyz.z = (double)ui->z_location->value() / 1000.0;
            if(update_location) {
                if(ahp_gt_is_connected()) {
                    if(ahp_gt_is_detected(getRailIndex())) {
                        ahp_gt_select_device(getRailIndex());
                        ahp_gt_goto_absolute(0, xyz_locations[current_location].xyz.x*1000.0*2.0*M_PI/ahp_gt_get_totalsteps(0), 800.0);
                        ahp_gt_goto_absolute(1, xyz_locations[current_location].xyz.y*1000.0*2.0*M_PI/ahp_gt_get_totalsteps(1), 800.0);
                    }
                }
            }
            current_location++;
        }
    }
    MainWindow::unlock_vlbi();
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
    double now = getTime();
    if((now - starttime) > ui->Duration->value())
        return;
    if(packet == nullptr)
        packet = getPacket();
    setLocation();
    switch(getMode()) {
        default: break;
        case HolographIQ:
        case HolographII:
        break;
        case Counter:
        if(isActive())
        {
            if(showCounts()) {/*
                nsamples = fmin(Raw->length(), ui->MinValue->value());
                if(nsamples > 0) {
                    if(Raw->count() > 3)
                    {
                        MinValue = 0;
                        for(int x = Raw->length() - 1; x >= Raw->length()-nsamples; x--)
                            MinValue += Raw->at(x);
                        MinValue /= nsamples;
                    }
                } else MinValue = 0;*/
                getCounts()->setCache(smooth());
                double mag = -1.0;
                double phi = -1.0;

                if(showAutocorrelations()) {
                    mag = (double)packet->autocorrelations[getLineIndex()].correlations[0].magnitude,
                    phi = (double)packet->autocorrelations[getLineIndex()].correlations[0].phase;
                }
                getCounts()->addCount(
                            packet->timestamp + starttime - getTimeRange(),
                            packet->timestamp + starttime,
                            (double)packet->counts[getLineIndex()] / ahp_xc_get_packettime(),
                            mag,
                            phi
                );
                getCounts()->buildHistogram(getCounts()->getSeries(), getCounts()->getElemental()->getStream(), getResolution());
            }
        }
        else
        {
            getCounts()->clear();
        }
        break;
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
    getSpectrum()->clear();
    getCounts()->clear();
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
        ui->iDFT->setEnabled(false);
        ui->AutoChannel->setEnabled(false);
        ui->CrossChannel->setEnabled(false);
        ui->StartChannel->setEnabled(false);
        ui->EndChannel->setEnabled(false);
        break;
    case Counter:
        ui->Counts->setEnabled(true);
        ui->Autocorrelations->setEnabled(true);
        ui->Crosscorrelations->setEnabled(true);
        ui->Resolution->setEnabled(true);
        ui->Active->setEnabled(false);
        ui->iDFT->setEnabled(true);
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
        ui->iDFT->setEnabled(true);
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
    while(!MainWindow::lock_vlbi());
    for(int x = 0; x < vlbi_total_contexts; x++) {
        if(!vlbi_has_node(context[x], getName().toStdString().c_str()))
            vlbi_add_node(context[x], getStream(), getName().toStdString().c_str(), 0);
    }
    MainWindow::unlock_vlbi();
}

void Line::removeFromVLBIContext()
{
    while(!MainWindow::lock_vlbi());
    for(int x = 0; x < vlbi_total_contexts; x++)
        vlbi_del_node(context[x], getName().toStdString().c_str());
    MainWindow::unlock_vlbi();
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

void Line::UnloadPositionChart()
{
    xyz_locations.clear();
}

void Line::LoadPositionChart()
{
    QString filename = QFileDialog::getOpenFileName(this, "Load rail position file", ".",
                       "CSV files (*.csv)", 0, 0);
    QFile data(filename);
    if(data.open(QFile::ReadOnly))
    {
        QString csv = data.readAll();
        saveSetting("location_chart", csv);
        QStringList locations = csv.split("\n");
        if(locations.length() > 0) {
            xyz_locations.clear();
            for(QString location : locations) {
                QStringList xyz = location.split(",");
                if(xyz.length() > 1) {
                    double x = atof(xyz[0].toStdString().c_str());
                    double y = atof(xyz[1].toStdString().c_str());
                    double z = (double)ui->z_location->value() / 1000.0;
                    if(xyz.length() > 2)
                        z = atof(xyz[2].toStdString().c_str());
                    dsp_location dsp_xyz;
                    dsp_xyz.xyz.x = x;
                    dsp_xyz.xyz.y = y;
                    dsp_xyz.xyz.z = z;
                    xyz_locations.append(dsp_xyz);
                }
            }
        }
    }
}

void Line::SavePlot()
{
    if(!isActive())
        return;
    QString filename = QFileDialog::getSaveFileName(this, "Save plot into file", "filename.csv",
                       "CSV files (*.csv)", 0, 0);
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
                counts.insert(getCounts()->getSeries()->at(x).x(), getCounts()->getSeries()->at(x).y());
            for(int x = 0; x < getSpectrum()->count(); x++) {
                mag.insert(getCounts()->getMagnitude()->at(x).x(), getCounts()->getMagnitude()->at(x).y());
                phi.insert(getCounts()->getPhase()->at(x).x(), getCounts()->getPhase()->at(x).y());
            }
            for(int i = 0, x = 0, y = 0, z = 0; i < fmax(fmax(getCounts()->count(), getSpectrum()->count()), getSpectrum()->getPhase()->count()); i++)
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
        case Autocorrelator:
            if(idft())
                output << "'lag (ns)','magnitude','phase'\n";
            else
                output << "'channel','magnitude','phase'\n";
            for(int x = 0, y = 0; x < getSpectrum()->count(); y++, x++)
            {
                output << "'" + QString::number(getSpectrum()->getMagnitude()->at(y).x()) + "','" + QString::number(getSpectrum()->getMagnitude()->at(y).y()) + "','"
                       + QString::number(getSpectrum()->getPhase()->at(y).x()) + "','" + QString::number(getSpectrum()->getPhase()->at(y).y()) + "'\n";
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

bool Line::idft()
{
    return ui->iDFT->isChecked();
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
    if(getMode() == Counter) {
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
            getSpectrum()->setName(name + " magnitude (residuals)");
        }
    }
}

void Line::TakeDark(Line *sender)
{
    if(sender->DarkTaken())
    {
        getDark()->clear();
        for(int x = 0; x < getSpectrum()->count(); x++)
        {
            getDark()->insert(getSpectrum()->getSeries()->at(x).x(), getSpectrum()->getSeries()->at(x).y());
            QString darkstring = readString("Dark", "");
            if(!darkstring.isEmpty())
                saveSetting("Dark", darkstring + ";");
            darkstring = readString("Dark", "");
            saveSetting("Dark", darkstring + QString::number(getSpectrum()->getSeries()->at(x).x()) + "," + QString::number(getSpectrum()->getSeries()->at(
                            x).y()));
        }
        getSpectrum()->setName(name + " magnitude (residuals)");
    }
    else
    {
        ui->TakeDark->setText("Apply Dark");
        removeSetting("Dark");
        getDark()->clear();
        getSpectrum()->setName(name + " magnitude");
    }
}

void Line::smoothBuffer(QXYSeries* buf, QList<double> *raw, int offset, int len)
{/*
    if(smooth() == 0)
        return;
    if(raw->count() < offset+len)
        return;
    if(raw->count() < smooth())
        return;
    if(buf->count() < offset+len)
        return;
    if(buf->count() < smooth())
        return;
    offset = fmax(offset, smooth());
    int x = 0;
    for(x = offset/2; x < len-offset/2; x++) {
        double val = 0.0;
        for(int y = -offset/2; y < offset/2; y++) {
            val += raw->at(x-y);
        }
        val /= smooth();
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), val-MinValue);
    }
    for(x = raw->count()-1; x >= raw->count()-offset/2; x--)
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), buf->at(raw->count()-offset/2-1).y());
    for(x = offset/2; x >= 0; x--)
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), buf->at(offset/2).y());*/
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
    setLocation();
    scanning = true;
    *stop = 0;
    *percent = 0;
    int npackets = getResolution();
    if(spectrum != nullptr && npackets > 0)
    {
        int lag = 1;
        int _lag = lag;
        setSpectrumSize(npackets);
        for (int x = 0, z = 0; z < npackets && x < npackets; x++, z++)
        {
            int lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime()-1;
            ahp_xc_correlation correlation;
            memcpy(&correlation, &spectrum[z].correlations[0], sizeof(ahp_xc_correlation));
            if(lag < npackets && lag >= 0)
            {
                getSpectrum()->getElemental()->getMagnitude()[lag] = (double)correlation.magnitude;
                getSpectrum()->getElemental()->getPhase()[lag] = (double)correlation.phase;
                for(int y = lag; y < npackets; y++)
                {
                    getSpectrum()->getElemental()->getMagnitude()[y] = getSpectrum()->getElemental()->getMagnitude()[lag];
                    getSpectrum()->getElemental()->getPhase()[y] = getSpectrum()->getElemental()->getPhase()[lag];
                }
                _lag = lag;
            }
        }
        if(idft())
            getSpectrum()->getElemental()->idft();
        if(Align())
            getSpectrum()->getElemental()->run();
        else
            getSpectrum()->getElemental()->finish(false, getStartLag(), getLagStep());
    }
    resetPercentPtr();
    resetStopPtr();
    scanning = false;
}

void Line::plot(bool success, double o, double s)
{
    double timespan = s;
    if(success)
        timespan = s;
    double offset = o;
    if(!idft()) {
        getSpectrum()->stackBuffer(getSpectrum()->getMagnitude(), getSpectrum()->getElemental()->getMagnitude(), 0, getSpectrum()->getElemental()->getStreamSize(), timespan, offset, 1.0, 0.0);
        getSpectrum()->stackBuffer(getSpectrum()->getPhase(), getSpectrum()->getElemental()->getPhase(), 0, getSpectrum()->getElemental()->getStreamSize(), timespan, offset, 1.0, 0.0);
    } else
        getSpectrum()->stackBuffer(getSpectrum()->getMagnitude(), getSpectrum()->getElemental()->getBuffer(), 0, getSpectrum()->getElemental()->getStreamSize(), timespan, offset, 1.0, 0.0);
    getSpectrum()->buildHistogram(getSpectrum()->getMagnitude(), getSpectrum()->getElemental()->getStream()->magnitude, 100);
    getGraph()->repaint();
    gethistogram()->repaint();
}

Line::~Line()
{
    ahp_xc_set_leds(getLineIndex(), 0);
    getSpectrum()->getElemental()->~Elemental();
    delete ui;
}
