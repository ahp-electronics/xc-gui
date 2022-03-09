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

#include "baseline.h"
#include "line.h"
Baseline::Baseline(QString n, int index, Line *n1, Line *n2, QSettings *s, QWidget *parent) :
    QWidget(parent)
{
    setAccessibleName("Baseline");
    settings = s;
    name = n;
    Index = index;
    magnitudeStack = new QMap<double, double>();
    phaseStack = new QMap<double, double>();
    dark = new QMap<double, double>();
    magnitude = new QLineSeries();
    phase = new QLineSeries();
    magnitudes = new QLineSeries();
    phases = new QLineSeries();
    complex = new QList<double>();
    elemental = new Elemental(this);
    connect(elemental, static_cast<void (Elemental::*)(bool, double, double)>(&Elemental::scanFinished), this, &Baseline::plot);
    line1 = n1;
    line2 = n2;
    connect(line1, static_cast<void (Line::*)()>(&Line::updateBufferSizes), this, &Baseline::updateBufferSizes);
    connect(line2, static_cast<void (Line::*)()>(&Line::updateBufferSizes), this, &Baseline::updateBufferSizes);
    connect(line1, static_cast<void (Line::*)()>(&Line::clearCrosscorrelations),
            [ = ]()
    {
        getMagnitude()->clear();
        getPhase()->clear();
        getMagnitudes()->clear();
        getPhases()->clear();
        getCounts()->clear();
        stack = 0.0;
    });
    connect(line2, static_cast<void (Line::*)()>(&Line::clearCrosscorrelations),
            [ = ]()
    {
        getMagnitude()->clear();
        getPhase()->clear();
        getMagnitudes()->clear();
        getPhases()->clear();
        getCounts()->clear();
        stack = 0.0;
    });
    connect(line1, static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged),
            [ = ](Line * sender)
    {
        getCounts()->clear();
        bool newstate = getLine1()->isActive() && getLine2()->isActive();
        stop = !newstate;
        if(oldstate != newstate)
        {
            if(newstate)
            {
                if(stream != nullptr)
                {
                    lock();
                    dsp_stream_set_dim(stream, 0, 0);
                    dsp_stream_alloc_buffer(stream, stream->len + 1);
                    getLine2()->resetTimestamp();
                    if(getMode() == HolographIQ)
                        addToVLBIContext();
                    else
                        removeFromVLBIContext();
                    unlock();
                }
            }
        }
        running = newstate;
        oldstate = newstate;
    });
    connect(line2, static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged),
            [ = ](Line * sender)
    {
        getCounts()->clear();
        bool newstate = getLine1()->isActive() && getLine2()->isActive();
        stop = !newstate;
        if(oldstate != newstate)
        {
            if(newstate)
            {
                if(stream != nullptr)
                {
                    lock();
                    dsp_stream_set_dim(stream, 0, 0);
                    dsp_stream_alloc_buffer(stream, stream->len + 1);
                    getLine1()->resetTimestamp();
                    if(getMode() == HolographIQ)
                        addToVLBIContext();
                    else
                        removeFromVLBIContext();
                    unlock();
                }
            }
        }
        running = newstate;
        oldstate = newstate;
    });
}

void Baseline::updateBufferSizes()
{
    start1 = getLine1()->getStartLine();
    start2 = getLine2()->getStartLine();
    end1 = getLine1()->getEndLine();
    end2 = getLine2()->getEndLine();
    len = end1 - start1 + end2 - start2;
    setMagnitudeSize(len);
    setPhaseSize(len);
}

bool Baseline::haveSetting(QString setting)
{
    return settings->contains(QString(name + "_" + setting).replace(' ', ""));
}

void Baseline::removeSetting(QString setting)
{
    settings->remove(QString(name + "_" + setting).replace(' ', ""));
}

void Baseline::saveSetting(QString setting, QVariant value)
{
    settings->setValue(QString(name + "_" + setting).replace(' ', ""), value);
    settings->sync();
}

QVariant Baseline::readSetting(QString setting, QVariant defaultValue)
{
    return settings->value(QString(name + "_" + setting).replace(' ', ""), defaultValue);
}

void Baseline::setDelay(double s)
{
    if(ahp_xc_has_crosscorrelator())
        ahp_xc_set_channel_cross((uint32_t)Index, (off_t)s * (ahp_xc_get_frequency() >> ahp_xc_get_frequency_divider()), 0);
}

void Baseline::setMode(Mode m)
{
    mode = m;
    if(mode == CrosscorrelatorIQ || mode == CrosscorrelatorII)
    {
        connect(getLine1(), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
        connect(getLine2(), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
        connect(getLine1(), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
        connect(getLine2(), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
    }
    else
    {
        disconnect(getLine1(), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
        disconnect(getLine2(), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
        disconnect(getLine1(), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
        disconnect(getLine2(), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
    }
}

void Baseline::stackValue(QLineSeries* series, QMap<double, double>* stacked, int idx, double x, double y)
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

void Baseline::stretch(QLineSeries* series)
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

void Baseline::addToVLBIContext(int index)
{
    if(index < 0)
    {
        index = getMode() - HolographIQ;
        if(index < 0) return;
    }
    lock();
    if(stream == nullptr)
    {
        stream = dsp_stream_new();
        dsp_stream_add_dim(stream, 0);
        dsp_stream_alloc_buffer(stream, stream->len + 1);
    }
    vlbi_set_baseline_stream(getVLBIContext(vlbi_context_iq), getLine1()->getLastName().toStdString().c_str(),
                             getLine2()->getLastName().toStdString().c_str(), getStream());
    unlock();
}

void Baseline::removeFromVLBIContext(int index)
{
    if(index < 0)
    {
        index = getMode() - HolographIQ;
        if (index < 0) return;
    }
    vlbi_unlock_baseline(getVLBIContext(index), getLine1()->getLastName().toStdString().c_str(),
                         getLine2()->getLastName().toStdString().c_str());
}

bool Baseline::isActive(bool atleast1)
{
    if(atleast1)
        return getLine1()->isActive() || getLine2()->isActive();
    return running;
}

void Baseline::TakeDark(Line* sender)
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
        removeSetting("Dark");
        getDark()->clear();
        getMagnitude()->setName(name + " magnitude");
    }
}

void Baseline::SavePlot()
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

void Baseline::stackCorrelations()
{
    scanning = true;
    ahp_xc_sample *spectrum = nullptr;
    getLine1()->setPercentPtr(&percent);
    getLine2()->setPercentPtr(&percent);
    updateBufferSizes();
    int tail_size = end2 - start2;
    int head_size = end1 - start1;

    stop = 0;
    int npackets = 0;
    npackets = ahp_xc_scan_crosscorrelations(getLine1()->getLineIndex(), getLine2()->getLineIndex(), &spectrum, start1,
               head_size, start2, tail_size, &stop, &percent);
    if(spectrum != nullptr && npackets >= len)
    {
        int lag = head_size;
        for (int x = 0, z = 0; x < len; x++, z++)
        {
            if(x == head_size)
            {
                lag = head_size;
                continue;
            }
            int _lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime() + head_size;
            if(_lag < len && _lag >= 0)
            {
                for(int y = lag; y != _lag; y += (lag > _lag ? -1 : (lag < _lag) ? 1 : 0))
                {
                    magnitude_buf[y] = magnitude_buf[lag];
                    phase_buf[y] = phase_buf[lag];
                }
                lag = _lag;
                if(mode == CrosscorrelatorIQ)
                {
                    magnitude_buf[lag] = (double)spectrum[z].correlations[0].magnitude / pow(spectrum[z].correlations[0].real +
                                         spectrum[z].correlations[0].imaginary, 2);
                    phase_buf[lag] = (double)spectrum[z].correlations[0].phase;
                }
                else if(mode == CrosscorrelatorII)
                {
                    magnitude_buf[lag] = (double)spectrum[z].correlations[0].real / pow(spectrum[z].correlations[0].real +
                                         spectrum[z].correlations[0].imaginary, 2);
                    phase_buf[lag] = (double)spectrum[z].correlations[0].imaginary / pow(spectrum[z].correlations[0].real +
                                     spectrum[z].correlations[0].imaginary, 2);
                }
            }
        }
        if(getLine1()->Idft() && getLine2()->Idft())
        {
            if(mode == CrosscorrelatorIQ)
            {
                elemental->setMagnitude(&magnitude_buf[1], len);
                elemental->setPhase(&phase_buf[1], len);
                elemental->idft();
            }
            else
            {
                elemental->setReal(&magnitude_buf[1], len);
                elemental->setImaginary(&phase_buf[1], len);
                elemental->dft(1);
            }
        }
        else
            elemental->setBuffer(&magnitude_buf[1], len);
        if(getLine1()->Align() && getLine2()->Align())
            elemental->run();
        else
            elemental->finish(true, -head_size);
        free(spectrum);
    }
    getLine1()->resetPercentPtr();
    getLine2()->resetPercentPtr();
    scanning = false;
}

void Baseline::plot(bool success, double o, double s)
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
    elemental->getStream()->len --;
    if(getLine1()->Histogram() && getLine2()->Histogram())
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
        for (int x = 0; x < elemental->getStream()->len; x++)
        {
            stackValue(getMagnitude(), getMagnitudeStack(), x, x * timespan + offset, (double)elemental->getStream()->buf[x]);
            if(!getLine1()->Idft() || !getLine2()->Idft())
                stackValue(getPhase(), getPhaseStack(), x, x * timespan + offset, (double)phase_buf[x]);
        }
    }
    elemental->getStream()->len ++;
    elemental->getStream()->len ++;
    stretch(getMagnitude());
    if(mode == CrosscorrelatorII)
        stretch(getPhase());
}

Baseline::~Baseline()
{
    threadRunning = false;
}
