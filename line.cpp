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

Line::Line(QString ln, int n, QSettings *s, QWidget *parent, QList<Line*> *p) :
    QWidget(parent),
    ui(new Ui::Line)
{
    setAccessibleName("Line");
    settings = s;
    localpercent = 0;
    parents = p;
    name = ln;
    dark = new QMap<double, double>();
    magnitudeStack = new QMap<double, double>();
    phaseStack = new QMap<double, double>();
    magnitude = new QLineSeries();
    phase = new QLineSeries();
    magnitudes = new QLineSeries();
    phases = new QLineSeries();
    counts = new QLineSeries();
    elemental = new Elemental(this);
    connect(elemental, static_cast<void (Elemental::*)(bool, double, double)>(&Elemental::scanFinished), this, &Line::plot);
    resetPercentPtr();
    getMagnitude()->setName(name + " magnitude");
    getPhase()->setName(name + " phase");
    getCounts()->setName(name + " (counts)");
    getPhases()->setName(name + " (magnitudes)");
    getMagnitudes()->setName(name + " (phases)");
    line = n;
    flags = 0;
    ui->setupUi(this);
    start = ui->StartLine->value();
    end = ui->EndLine->value();
    len = end - start;
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
    connect(ui->Run, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), this, &Line::runClicked);
    connect(ui->Save, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit savePlot();
    });
    connect(ui->TakeDark, static_cast<void (QPushButton::*)(bool)>(&QPushButton::clicked), [ = ](bool checked)
    {
        emit takeDark(this);
    });
    connect(ui->IDFT, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        saveSetting("DFT", ui->IDFT->isChecked());
    });
    connect(ui->Histogram, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [ = ](int state)
    {
        saveSetting("Histogram", ui->Histogram->isChecked());
    });
    connect(ui->ElementalAlign, static_cast<void (QCheckBox::*)(bool)>(&QCheckBox::clicked), [ = ](bool checked)
    {
        saveSetting("ElementalAlign", ui->ElementalAlign->isChecked());
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
    connect(ui->StartLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartLine->value() >= ui->EndLine->value() - 5)
        {
            ui->EndLine->setValue(ui->StartLine->value() + 5);
        }
        start = ui->StartLine->value();
        end = ui->EndLine->value();
        len = end - start;
        setMagnitudeSize(len);
        setPhaseSize(len);
        setDftSize(len);
        emit updateBufferSizes();
        saveSetting("StartLine", ui->StartLine->value());
    });
    connect(ui->EndLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ui->StartLine->value() >= ui->EndLine->value() - 5)
        {
            ui->StartLine->setValue(ui->EndLine->value() - 5);
        }
        start = ui->StartLine->value();
        end = ui->EndLine->value();
        len = end - start;
        setMagnitudeSize(len);
        setPhaseSize(len);
        setDftSize(len);
        emit updateBufferSizes();
        saveSetting("EndLine", ui->EndLine->value());
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
    connect(ui->SpectralLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        ahp_xc_set_channel_auto(line, 0, value);
        saveSetting("SpectralLine", value);
    });
    connect(ui->LineDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        if(ahp_xc_has_crosscorrelator())
            ahp_xc_set_channel_cross(line, 0, value);
        saveSetting("LineDelay", value);
    });
    connect(ui->MountMotorIndex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        MountMotorIndex = value;
        if(ahp_gt_is_connected()) {
            ahp_gt_select_device(ui->MountMotorIndex->value());
            ahp_gt_detect_device();
        }
        saveSetting("MountMotorIndex", value);
    });
    connect(ui->RailMotorIndex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        RailMotorIndex = value;
        if(ahp_gt_is_connected()) {
            ahp_gt_select_device(ui->RailMotorIndex->value());
            ahp_gt_detect_device();
        }
        saveSetting("RailMotorIndex", value);
    });
    connect(ui->x_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.x = (double)value / 1000.0;
        saveSetting("location_x", location.xyz.x);
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(ui->RailMotorIndex->value())) {
                ahp_gt_select_device(ui->RailMotorIndex->value());
                ahp_gt_goto_absolute(0, value*M_PI*2.0/ahp_gt_get_totalsteps(0), 800.0);
            }
        }
    });
    connect(ui->y_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.y = (double)value / 1000.0;
        saveSetting("location_y", location.xyz.y);
        if(ahp_gt_is_connected()) {
            if(ahp_gt_is_detected(ui->RailMotorIndex->value())) {
                ahp_gt_select_device(ui->RailMotorIndex->value());
                ahp_gt_goto_absolute(1, value*M_PI*2.0/ahp_gt_get_totalsteps(1), 800.0);
            }
        }
    });
    connect(ui->z_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.z = (double)value / 1000.0;
        saveSetting("location_z", location.xyz.z);
    });
    ui->x_location->setValue(readDouble("location_x", 0.0) * 1000);
    ui->y_location->setValue(readDouble("location_y", 0.0) * 1000);
    ui->z_location->setValue(readDouble("location_z", 0.0) * 1000);
    ui->MountMotorIndex->setValue(readInt("MountMotorIndex", getLineIndex() * 2 + 1));
    ui->RailMotorIndex->setValue(readInt("RailMotorIndex", getLineIndex() * 2 + 2));
    ui->MinScore->setValue(readInt("MinScore", 50));
    ui->Decimals->setValue(readInt("Decimals", 0));
    ui->MaxDots->setValue(readInt("MaxDots", 10));
    ui->SampleSize->setValue(readInt("SampleSize", 5));
    ui->StartLine->setValue(readInt("StartLine", ui->StartLine->minimum()));
    ui->EndLine->setValue(readInt("EndLine", ui->EndLine->maximum()));
    ui->SpectralLine->setRange(0, ahp_xc_get_delaysize() - 1);
    ui->LineDelay->setRange(0, ahp_xc_get_delaysize() - 1);
    ui->SpectralLine->setValue(readInt("SpectralLine", ui->SpectralLine->minimum()));
    ui->LineDelay->setValue(readInt("LineDelay", ui->LineDelay->minimum()));
    ui->IDFT->setChecked(readBool("IDFT", false));
    ui->Histogram->setChecked(readBool("Histogram", false));
    ui->StartLine->setRange(0, ahp_xc_get_delaysize() * 2 - 7);
    ui->EndLine->setRange(5, ahp_xc_get_delaysize() - 1);
    ui->flag4->setEnabled(!ahp_xc_has_differential_only());
    ui->Crosscorrelations->setEnabled(ahp_xc_has_crosscorrelator());
    ahp_xc_set_leds(line, flags);
    setFlag(0, readBool(ui->flag0->text(), false));
    setFlag(1, readBool(ui->flag1->text(), false));
    setFlag(2, readBool(ui->flag2->text(), false));
    setFlag(3, readBool(ui->flag3->text(), false));
    setFlag(4, readBool(ui->flag4->text(), false));
    ui->Counts->setChecked(readBool(ui->Counts->text(), false));
    ui->Autocorrelations->setChecked(readBool(ui->Autocorrelations->text(), false));
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
    getDark()->clear();
    getMagnitudeStack()->clear();
    getPhaseStack()->clear();
    setActive(false);
}

void Line::runClicked(bool checked)
{

    if(ui->Run->text() == "Run")
    {
        setActive(true);
        ui->Run->setText("Stop");
    }
    else
    {
        setActive(false);
        ui->Run->setText("Run");
    }
    ui->Counter->setEnabled(mode == Counter);
    ui->Correlator->setEnabled(false);
    if(mode != Counter)
    {
        ui->Correlator->setEnabled(!isActive());
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
    getMagnitudeStack()->clear();
    getPhaseStack()->clear();
    if(m != Counter)
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
    ui->Leds->setEnabled(ahp_xc_has_leds());
    ui->Counter->setEnabled(m == Counter);
    resetPercentPtr();
    if(!isActive())
        ui->Correlator->setEnabled(m != Counter);
    else
        runClicked();
    mode = m;
}

void Line::paint()
{
    stop = !isActive();
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
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    resetTimestamp();
    vlbi_add_node(getVLBIContext(), getStream(), getLastName().toStdString().c_str(), false);
    unlock();
}

void Line::removeFromVLBIContext()
{
    if(stream != nullptr)
    {
        lock();
        vlbi_del_node(getVLBIContext(), getLastName().toStdString().c_str());
        unlock();
    }
}

void Line::stackValue(QLineSeries* series, QMap<double, double>* stacked, int idx, double x, double y)
{
    y /= stack;
    if(stacked->count() > idx)
    {
        y += stacked->values().at(idx) * (stack - 1) / stack;
        stacked->keys().replace(idx, x);
        stacked->values().replace(idx, y);
    }
    else
    {
        stacked->insert(x, y);
    }
    series->append(x, y);
}

void Line::setLocation(dsp_location l)
{
    saveSetting("location_x", l.xyz.x);
    saveSetting("location_y", l.xyz.y);
    saveSetting("location_z", l.xyz.z);
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

bool Line::Differential()
{
    return getFlag(4);
}

bool Line::Histogram()
{
    return ui->Histogram->isChecked();
}

bool Line::Idft()
{
    return ui->IDFT->isChecked();
}

bool Line::Align()
{
    return ui->ElementalAlign->isChecked();
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

void Line::gotoRaDec(double ra, double dec)
{
    if(ahp_gt_is_connected()) {
        if(ahp_gt_is_detected(ui->MountMotorIndex->value())) {
            ahp_gt_select_device(ui->MountMotorIndex->value());
            ahp_gt_goto_absolute(0, ra*M_PI*2.0/ahp_gt_get_totalsteps(0), 800.0);
            ahp_gt_goto_absolute(1, dec*M_PI*2.0/ahp_gt_get_totalsteps(1), 800.0);
        }
    }
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
    running = a;
    activeStateChanged(this);
}

void Line::startTracking(double ra_rate, double dec_rate)
{
    ahp_gt_set_address(ui->RailMotorIndex->value());
    ahp_gt_stop_motion(0, 1);
    ahp_gt_stop_motion(1, 1);
    ahp_gt_start_motion(0, ra_rate);
    ahp_gt_start_motion(1, dec_rate);
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

void Line::stackCorrelations()
{
    scanning = true;
    ahp_xc_sample *spectrum = nullptr;
    stop = 0;
    int npackets = ahp_xc_scan_autocorrelations(line, &spectrum, start, len, &stop, &localpercent);
    *percent = 0;
    if(spectrum != nullptr && npackets == len)
    {
        int lag = 1;
        for (int x = 0, z = 0; x < len; x++, z++)
        {
            int _lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime() - start;
            if(_lag < len && _lag >= 0)
            {
                for(int y = lag + 1; y < _lag && y < len; y++)
                {
                    magnitude_buf[y] = magnitude_buf[lag];
                    phase_buf[y] = phase_buf[lag];
                }
                lag = _lag;
                if(mode == Autocorrelator)
                {
                    magnitude_buf[lag] = (double)spectrum[z].correlations[0].magnitude / pow(spectrum[z].correlations[0].real +
                                         spectrum[z].correlations[0].imaginary, 2);
                    phase_buf[lag] = (double)spectrum[z].correlations[0].phase;
                }
            }
        }
        if(Idft())
        {
            if(mode == Autocorrelator)
            {
                elemental->setMagnitude(magnitude_buf, len);
                elemental->setPhase(phase_buf, len);
                elemental->idft();
            }
        }
        else
            elemental->setBuffer(magnitude_buf, len);
        if(Align())
            elemental->run();
        else
            elemental->finish();
        free(spectrum);
    }
    scanning = false;
}

void Line::plot(bool success, double o, double s)
{
    double timespan = ahp_xc_get_sampletime();
    if(success)
    {
        timespan = ahp_xc_get_sampletime() / s;
        offset = o * timespan;
    }
    getMagnitude()->clear();
    getPhase()->clear();
    stack += 1.0;
    elemental->getStream()->len --;
    if(Histogram())
    {
        int size = fmin(len, 256);
        double *histo = dsp_stats_histogram(elemental->getStream(), size);
        for (int x = 1; x < size; x++)
        {
            if(histo[x] != 0)
                stackValue(getMagnitude(), getMagnitudeStack(), x, x * M_PI * 2 / size, histo[x]);
        }
        free(histo);
    }
    else
    {
        for (int x = 1; x < elemental->getStream()->len - 1; x++)
        {
            stackValue(getMagnitude(), getMagnitudeStack(), x, x * timespan + offset, (double)elemental->getStream()->buf[x]);
            if(!Idft())
                stackValue(getPhase(), getPhaseStack(), x, x * timespan + offset, (double)phase_buf[x]);
        }
    }
    elemental->getStream()->len ++;
    stretch(getMagnitude());
}

Line::~Line()
{
    elemental->~Elemental();
    delete ui;
}
