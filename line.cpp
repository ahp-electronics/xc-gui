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
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    line = n;
    flags = 0;
    ui->setupUi(this);
    ui->x_location->setRange(-ahp_xc_get_delaysize() * 1000, ahp_xc_get_delaysize() * 1000);
    ui->y_location->setRange(-ahp_xc_get_delaysize() * 1000, ahp_xc_get_delaysize() * 1000);
    ui->z_location->setRange(-ahp_xc_get_delaysize() * 1000, ahp_xc_get_delaysize() * 1000);

    setLocation((dsp_location)
    {
        .xyz =
        {
            .x = readDouble("location_x", 0.0) / 1000.0,
            .y = readDouble("location_y", 0.0) / 1000.0,
            .z = readDouble("location_z", 0.0) / 1000.0
        }
    });
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
        if(getFlag(4) && mode == Autocorrelator)
        {
            ui->ElementalAlign->setChecked(settings->value("ElementalAlign", false).toBool());
            ui->ElementalAlign->setEnabled(true);
            ui->maxdots->setEnabled(true);
            ui->decimals->setEnabled(true);
            ui->minscore->setEnabled(true);
            ui->samplesize->setEnabled(true);
            ui->MaxDots->setEnabled(true);
            ui->Decimals->setEnabled(true);
            ui->MinScore->setEnabled(true);
            ui->SampleSize->setEnabled(true);
        }
        else
        {
            ui->ElementalAlign->setChecked(false);
            ui->ElementalAlign->setEnabled(false);
            ui->maxdots->setEnabled(false);
            ui->decimals->setEnabled(false);
            ui->minscore->setEnabled(false);
            ui->samplesize->setEnabled(false);
            ui->MaxDots->setEnabled(false);
            ui->Decimals->setEnabled(false);
            ui->MinScore->setEnabled(false);
            ui->SampleSize->setEnabled(false);
        }
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
        saveSetting("IDFT", ui->IDFT->isChecked());
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
    connect(ui->SpectralLine, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        ahp_xc_set_channel_auto(line, 0, value);
        saveSetting("SpectralLine", value);
    });
    connect(ui->LineDelay, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        ahp_xc_set_channel_cross(line, 0, value);
        saveSetting("LineDelay", value);
    });
    connect(ui->MotorIndex, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        motorIndex = value;
        saveSetting("MotorIndex", value);
    });
    connect(ui->x_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.x = (double)value / 1000.0;
        saveSetting("x_location", value);
    });
    connect(ui->y_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.y = (double)value / 1000.0;
        saveSetting("y_location", value);
    });
    connect(ui->z_location, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [ = ](int value)
    {
        getLocation()->xyz.z = (double)value / 1000.0;
        saveSetting("z_location", value);
    });
    ui->x_location->setValue(readInt("x_location", 0));
    ui->y_location->setValue(readInt("y_location", 0));
    ui->z_location->setValue(readInt("z_location", 0));
    ui->MotorIndex->setValue(readInt("MotorIndex", getLineIndex()+1));
    ui->MinScore->setValue(readInt("MinScore", 50));
    ui->Decimals->setValue(readInt("Decimals", 0));
    ui->MaxDots->setValue(readInt("MaxDots", 10));
    ui->SampleSize->setValue(readInt("SampleSize", 5));
    ui->StartLine->setValue(readInt("StartLine", ui->StartLine->minimum()));
    ui->EndLine->setValue(readInt("EndLine", ui->EndLine->maximum()));
    ui->SpectralLine->setValue(readInt("SpectralLine", ui->SpectralLine->minimum()));
    ui->IDFT->setChecked(readBool("IDFT", false));
    ui->SpectralLine->setRange(0, ahp_xc_get_delaysize() - 7);
    ui->StartLine->setRange(0, ahp_xc_get_delaysize() * 2 - 7);
    ui->EndLine->setRange(5, ahp_xc_get_delaysize() - 1);
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

void Line::setMode(Mode m)
{
    mode = m;
    getDark()->clear();
    getMagnitude()->clear();
    getPhase()->clear();
    getCounts()->clear();
    getMagnitudes()->clear();
    getPhases()->clear();
    getMagnitudeStack()->clear();
    getPhaseStack()->clear();
    if(mode != Counter)
    {
        stack = 0.0;
        mx = 0.0;
    }
    if(mode == Autocorrelator)
    {
        connect(this, static_cast<void (Line::*)()>(&Line::savePlot), this, &Line::SavePlot);
        connect(this, static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Line::TakeDark);
        if((flags & 0x10) != 0)
        {
            ui->ElementalAlign->setChecked(settings->value("ElementalAlign", false).toBool());
            ui->ElementalAlign->setEnabled(true);
            ui->maxdots->setEnabled(true);
            ui->decimals->setEnabled(true);
            ui->minscore->setEnabled(true);
            ui->samplesize->setEnabled(true);
            ui->MaxDots->setEnabled(true);
            ui->Decimals->setEnabled(true);
            ui->MinScore->setEnabled(true);
            ui->SampleSize->setEnabled(true);
        }
        else
        {
            ui->ElementalAlign->setChecked(false);
            ui->ElementalAlign->setEnabled(false);
            ui->maxdots->setEnabled(false);
            ui->decimals->setEnabled(false);
            ui->minscore->setEnabled(false);
            ui->samplesize->setEnabled(false);
            ui->MaxDots->setEnabled(false);
            ui->Decimals->setEnabled(false);
            ui->MinScore->setEnabled(false);
            ui->SampleSize->setEnabled(false);
        }
    } else {
        disconnect(this, static_cast<void (Line::*)()>(&Line::savePlot), this, &Line::SavePlot);
        disconnect(this, static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Line::TakeDark);
    }
    resetPercentPtr();
    ui->Counter->setEnabled(mode == Counter);
    if(!isActive())
    {
        ui->Correlator->setEnabled(mode != Counter);
    } else
        runClicked();
}

void Line::paint()
{
    stop = !isActive();
    if(ui->Progress != nullptr)
    {
        if(mode != Counter && mode != Holograph)
        {
            if(getPercent() > ui->Progress->minimum() && getPercent() < ui->Progress->maximum())
                ui->Progress->setValue(getPercent());
        }
    }
    update(rect());
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

void Line::setLocation(dsp_location location)
{
    ui->x_location->setValue(location.xyz.x);
    ui->y_location->setValue(location.xyz.y);
    ui->z_location->setValue(location.xyz.z);
    saveSetting("location_x", location.xyz.x);
    saveSetting("location_y", location.xyz.y);
    saveSetting("location_z", location.xyz.z);
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
                       "CSV files (.csv);;Zip files (.zip, *.7z)", 0, 0); // getting the filename (full path)
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
    ahp_gt_set_address(ui->MotorIndex->value());
    ahp_gt_goto_absolute(0, ra, 800);
    ahp_gt_goto_absolute(1, dec, 800);
}

void Line::setActive(bool a)
{
    running = a;
    activeStateChanged(this);
}

void Line::startTracking(double ra_rate, double dec_rate)
{
    ahp_gt_set_address(ui->MotorIndex->value());
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
    int npackets = ahp_xc_scan_autocorrelations(line, &spectrum, start, len+1, &stop, &localpercent);
    if(spectrum != nullptr && npackets == len + 1)
    {
        int lag = 1;
        for (int x = 0, z = 1; x < len; x++, z++)
        {
            int _lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime() - start;
            for(int y = lag+1; y < _lag && y < len; y++) {
                magnitude_buf[y] = magnitude_buf[lag];
                phase_buf[y] = phase_buf[lag];
            }
            if(_lag < len && _lag >= 0) {
                lag = _lag;
                magnitude_buf[lag] = (double)spectrum[z].correlations[0].magnitude / pow(spectrum[z].correlations[0].real + spectrum[z].correlations[0].imaginary, 2);
                phase_buf[lag] = (double)spectrum[z].correlations[0].phase;
            }
        }
        if(Idft()) {
            elemental->setMagnitude(magnitude_buf, len);
            elemental->setPhase(phase_buf, len);
            elemental->idft();
        } else
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
    for (int x = 0; x < len; x++) {
        stackValue(getMagnitude(), getMagnitudeStack(), x, x * timespan + offset, (double)elemental->getStream()->buf[x]);
        if(!Idft())
            stackValue(getPhase(), getPhaseStack(), x, x * timespan + offset, (double)phase_buf[x]);
    }
    stretch(getMagnitude());
}

Line::~Line()
{
    setActive(false);
    elemental->~Elemental();
    delete ui;
}
