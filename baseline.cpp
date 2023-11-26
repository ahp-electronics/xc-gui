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

#include <cstdio>
#include <cstring>
#include "baseline.h"
#include "line.h"
#include "graph.h"
#include "mainwindow.h"

Baseline::Baseline(QString n, int index, QList<Line *>nodes, QSettings *s, QWidget *parent) :
    QWidget(parent)
{
    setAccessibleName("Baseline");
    settings = s;
    name = n;
    Index = index;
    Nodes = nodes;
    localpercent = 0;
    localstop = 1;
    mode = Counter;
    start = (int*)malloc(sizeof(int));
    end = (int*)malloc(sizeof(int));
    step = (int*)malloc(sizeof(int));
    size = (int*)malloc(sizeof(int));
    lag_start = (double*)malloc(sizeof(double));
    lag_end = (double*)malloc(sizeof(double));
    lag_step = (double*)malloc(sizeof(double));
    lag_size = (double*)malloc(sizeof(double));
    spectrum = new Series();
    counts = new Series();
    resetPercentPtr();
    resetStopPtr();
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    stream->samplerate = 1.0/ahp_xc_get_packettime();
    connect(getSpectrum()->getElemental(), static_cast<void (Elemental::*)(bool, double, double)>(&Elemental::scanFinished), this, &Baseline::plot);
}

void Baseline::setBufferSizes()
{
    lock();
    for(int x = 0; x < getCorrelationOrder(); x++) {
        start[x] = getLine(x)->getStartChannel();
        end[x] = getLine(x)->getEndChannel();
        step[x] = getLine(x)->getScanStep();
        size[x] = getLine(x)->getChannelBandwidth();
        lag_start[x] = getLine(x)->getStartLag();
        lag_end[x] = getLine(x)->getEndLag();
        lag_step[x] = getLine(x)->getLagStep();
        lag_size[x] = getLine(x)->getLagBandwidth();
    }
    tail_size = size[0];
    head_size = size[1];
    step_size = fmax(1, fmax(step[0], step[1]));
    size_2nd = fmax(3, (tail_size+head_size)/step_size+1);
    setSpectrumSize(size_2nd);
    lag_head_size = lag_size[0];
    lag_tail_size = lag_size[1];
    lag_size_2nd = lag_tail_size+lag_head_size;
    lag_step_size = lag_size_2nd / size_2nd;
    unlock();
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
    if(ahp_xc_has_crosscorrelator()) {
        for(int x = 0; x < getCorrelationOrder(); x++) {
            ahp_xc_set_channel_cross((uint32_t)getLine(x)->getLineIndex(), (off_t)fmax(0, s * ahp_xc_get_frequency()), 1, 0);
        }
    }
}

void Baseline::setMode(Mode m)
{
    mode = m;
    getDark()->clear();
    getCounts()->clear();
    getSpectrum()->clear();
    if(mode == CrosscorrelatorIQ || mode == CrosscorrelatorII)
    {
        for(int x = 0; x < getLines().count(); x++) {
            connect(getLine(x), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
            connect(getLine(x), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
        }
        setCorrelationOrder(getCorrelationOrder());
    }
    else
    {
        *stop = 1;
        for(int x = 0; x < getLines().count(); x++) {
            disconnect(getLine(x), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
            disconnect(getLine(x), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
        }
        setCorrelationOrder(getCorrelationOrder());
    }
}

void Baseline::addCount(double starttime, ahp_xc_packet *packet)
{
    if(packet == nullptr)
        packet = getPacket();
    bool active = false;
    switch(getMode()) {
    default: break;
    case HolographIQ:
    case HolographII:
        if(getVLBIContext() == nullptr) break;
        if(scanActive())
        {
            dsp_stream_p stream = getStream();
            if(stream == nullptr) break;
            if(MainWindow::lock_vlbi()) {
                double offset = 0;
                for(int x = 0; x < getCorrelationOrder(); x++) {
                    if(vlbi_has_node(getVLBIContext(), getLine(x)->getName().toStdString().c_str())) {
                        offset = vlbi_get_offset(getVLBIContext(), packet->timestamp + starttime, getLine(x)->getName().toStdString().c_str(),
                                         getGraph()->getRa(), getGraph()->getDec(), getGraph()->getDistance());
                        offset /= ahp_xc_get_sampletime();
                        offset ++;
                        if(ahp_xc_intensity_crosscorrelator_enabled())
                        {
                            ahp_xc_set_channel_auto(getLine(x)->getLineIndex(), offset, 1, 0);
                        } else {
                            ahp_xc_set_channel_cross(getLine(x)->getLineIndex(), offset, 1, 0);
                        }
                    }
                }
                stream->dft.complex[0].real = packet->crosscorrelations[Index].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].real;
                stream->dft.complex[0].imaginary = packet->crosscorrelations[Index].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].imaginary;
                MainWindow::unlock_vlbi();
            }
        }
        break;
    case Counter:
        if(isActive())
        {
            int index = ahp_xc_get_crosscorrelation_index(indexes.toVector().toStdVector().data(), indexes.count());
            active = index == Index;
            for(int x = 0; x < getCorrelationOrder(); x++)
                if(!getLine(x)->showCrosscorrelations())
                    active &= false;
            if(active) {
                double mag = -1.0;
                double phi = -1.0;
                if(ahp_xc_intensity_crosscorrelator_enabled())
                    for(int x = 0; x < getCorrelationOrder(); x++) {
                        mag += pow(packet->counts[getLine(x)->getLineIndex()], 2);
                    mag = (double)sqrt(mag) / ahp_xc_get_packettime();
                } else
                    mag = (double)packet->crosscorrelations[Index].correlations[0].magnitude / (double)packet->crosscorrelations[Index].correlations[0].counts;
                getCounts()->addCount(packet->timestamp + starttime - getTimeRange(), packet->timestamp + starttime, packet->crosscorrelations[Index].correlations[0].counts / ahp_xc_get_packettime(), mag, phi);
                getCounts()->buildHistogram(getCounts()->getMagnitude(), getCounts()->getElemental()->getStream()->magnitude, 100);
            }
            else
            {
                getCounts()->clear();
            }
        }
        break;
    }
}

void Baseline::setCorrelationOrder(int order)
{
    while(!MainWindow::lock_vlbi());
    correlation_order = fmax(order, 2);
    start = (int*)realloc(start, sizeof(int) * correlation_order);
    end = (int*)realloc(end, sizeof(int) * correlation_order);
    step = (int*)realloc(step, sizeof(int) * correlation_order);
    size = (int*)realloc(size, sizeof(int) * correlation_order);
    lag_start = (double*)realloc(lag_start, sizeof(double) * correlation_order);
    lag_end = (double*)realloc(lag_end, sizeof(double) * correlation_order);
    lag_step = (double*)realloc(lag_step, sizeof(double) * correlation_order);
    lag_size = (double*)realloc(lag_size, sizeof(double) * correlation_order);
    lines.clear();
    indexes.clear();
    for(int x = 0; x < correlation_order; x++) {
        int idx = ahp_xc_get_line_index(Index, x);
        lines.append(Nodes.at(idx));
        indexes.append(idx);
    }
    const char** names = (const char**)malloc(sizeof(const char*)*getCorrelationOrder());
    for(int x = 0; x < getCorrelationOrder(); x++)
        names[x] = getLine(x)->getName().toStdString().c_str();
    if(getMode() == HolographII || getMode() == HolographIQ)
        vlbi_set_baseline_stream(getVLBIContext(), names, getStream());
    free(names);
    lines.clear();
    indexes.clear();
    for(int x = 0; x < getCorrelationOrder(); x++) {
        int idx = ahp_xc_get_line_index(Index, x);
        lines.append(Nodes[idx]);
        indexes.append(idx);
        connect(getLine(x), static_cast<void (Line::*)()>(&Line::clear),
                [ = ]()
        {
            getCounts()->clear();
            getSpectrum()->clear();
        });
        connect(getLine(x), static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged),
                [ = ](Line * line)
        {
            getCounts()->clear();
            bool newstate = line->showCrosscorrelations();
            *stop = !line->isActive();
            for(int y = 0; y < getLines().count(); y++) {
                if(y == x) continue;
                line = getLine(y);
                newstate &= line->showCrosscorrelations();
                if(line != nullptr)
                *stop = !(*stop && line->isActive());
            }
            if(oldstate != newstate)
            {
                emit activeStateChanging(this);
                if(newstate)
                {
                    emit activeStateChanged(this);
                }
            }
            running = newstate;
            oldstate = newstate;
        });
        getLine(x)->setActive(getLine(x)->isActive());
        if(mode == CrosscorrelatorIQ || mode == CrosscorrelatorII)
            connect(getLine(x), static_cast<void (Line::*)()>(&Line::updateBufferSizes), this, &Baseline::setBufferSizes);
        else
            disconnect(getLine(x), static_cast<void (Line::*)()>(&Line::updateBufferSizes), this, &Baseline::setBufferSizes);
    }
    MainWindow::unlock_vlbi();
}

void Baseline::addToVLBIContext(int index)
{
    if(index < 0)
    {
        index = getMode() - HolographIQ;
        if(index < 0) return;
    }
}

void Baseline::removeFromVLBIContext(int index)
{
    if(index < 0)
    {
        index = getMode() - HolographIQ;
        if (index < 0) return;
    }
    const char** names = (const char**)malloc(sizeof(const char*)*getCorrelationOrder());
    for(int x = 0; x < getCorrelationOrder(); x++)
        names[x] = getLine(x)->getName().toStdString().c_str();
    vlbi_unlock_baseline(getVLBIContext(), names);
}

bool Baseline::isActive(bool atleast1)
{
    bool active = true;
    for(int x = 0; x < getLines().count(); x++)
        active |= getLine(x)->isActive();
    if(atleast1)
        return active;
    return running;
}

bool Baseline::scanActive(bool atleast1)
{
    if(getLines().count() == 0)
        return false;
    bool active = getLine(0)->scanActive();
    for(int x = 1; x < getCorrelationOrder(); x++) {
        if(atleast1)
            active |= getLine(x)->scanActive();
        else
            active &= getLine(x)->scanActive();
    }
    return active;
}

void Baseline::TakeMeanValue(Line *sender)
{
    if(sender->DarkTaken())
    {
        MinValue = 0;
    } else {
        double min = DBL_MAX;
        for(int d = 0; d < getCounts()->getMagnitude()->count(); d++)
        {
            min = fmin(min, getCounts()->getMagnitude()->at(d).y());
        }
        MinValue = min;
    }
}

void Baseline::TakeDark(Line* sender)
{
    if(sender->DarkTaken())
    {
        getDark()->clear();
        for(int x = 0; x < getSpectrum()->count(); x++)
        {
            getDark()->insert(getSpectrum()->getMagnitude()->at(x).x(), getSpectrum()->getMagnitude()->at(x).y());
            QString darkstring = readString("Dark", "");
            if(!darkstring.isEmpty())
                saveSetting("Dark", darkstring + ";");
            darkstring = readString("Dark", "");
            saveSetting("Dark", darkstring + QString::number(getSpectrum()->getMagnitude()->at(x).x()) + "," + QString::number(getSpectrum()->getMagnitude()->at(
                            x).y()));
        }
        getSpectrum()->setName(name + " magnitude (residuals)");
    }
    else
    {
        removeSetting("Dark");
        getDark()->clear();
        getSpectrum()->setName(name + " magnitude");
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
        for(int x = 0, y = 0; x < getSpectrum()->count(); y++, x++)
        {
            output << "'" + QString::number(getSpectrum()->getMagnitude()->at(y).x()) + "';'" + QString::number(getSpectrum()->getMagnitude()->at(y).y()) + "';'"
                   + QString::number(getSpectrum()->getPhase()->at(y).x()) + "';'" + QString::number(getSpectrum()->getPhase()->at(y).y()) + "'\n";
        }
    }
    data.close();
}

bool Baseline::idft() {
    bool dft = getLine(0)->idft();
    for(int x = 0; x < getCorrelationOrder(); x++)
        dft &= getLine(x)->idft();
    return dft;
}

int Baseline::smooth()
{
    int value = INT32_MAX;
    for(int x = 0; x < getCorrelationOrder(); x++)
        value = fmin(getLine(x)->smooth(), value);
    return value;
}

QMap<double, double>* Baseline::getDark()
{
    return getCounts()->getDark();
}

void Baseline::stackCorrelations()
{
    scanning = true;
    ahp_xc_sample *spectrum = nullptr;
    getLine(0)->setPercentPtr(percent);
    getLine(1)->setPercentPtr(percent);
    getLine(0)->resetStopPtr();
    getLine(1)->resetStopPtr();

    *stop = 0;
    int npackets = 0;
    setBufferSizes();
    int step = fmax(getLine(0)->getScanStep(), getLine(1)->getScanStep());
    npackets = ahp_xc_scan_crosscorrelations(getLine(0)->getLineIndex(), getLine(1)->getLineIndex(), &spectrum, start[0],
               head_size, start[1], tail_size, step, stop, percent);
    getLine(0)->setLocation();
    getLine(1)->setLocation();
    if(spectrum != nullptr && npackets > 0)
    {
        int _head = head_size/step;
        int _tail = tail_size/step;
        int ofs = _head;
        int lag = ofs-1;
        int _lag = lag;
        bool tail = false;
        for (int x = 0, z = 0; x < npackets; x++, z++)
        {
            lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime();
            tail = (lag >= 0);
            if (tail)
                lag --;
            ahp_xc_correlation correlation;
            memcpy(&correlation, &spectrum[z].correlations[0], sizeof(ahp_xc_correlation));
            if(correlation.magnitude > 0) {
                if(lag > -_head && lag < _tail)
                {
                    getSpectrum()->getElemental()->getMagnitude()[lag+ofs] = (double)correlation.magnitude;
                    getSpectrum()->getElemental()->getPhase()[lag+ofs] = (double)correlation.phase;
                    for(int y = lag; y >= -_head && y < _tail; y += (tail ? 1 : -1))
                    {
                        getSpectrum()->getElemental()->getMagnitude()[y+ofs] = (double)correlation.magnitude;
                        getSpectrum()->getElemental()->getPhase()[y+ofs] = (double)correlation.phase;
                    }
                    _lag = lag;
                }
            }
        }
        if(getLine(0)->idft() && getLine(1)->idft())
        {
            getSpectrum()->getElemental()->idft();
        }
        if(getLine(0)->Align() && getLine(1)->Align())
            getSpectrum()->getElemental()->run();
        else
            getSpectrum()->getElemental()->finish(false, -getStartLag(), getLagStep());
        free(spectrum);
    }
    getLine(0)->resetPercentPtr();
    getLine(1)->resetPercentPtr();
    scanning = false;
}

void Baseline::plot(bool success, double o, double s)
{
    double timespan = s;
    if(success)
        timespan = s;
    double offset = o;
    if(!idft()) {
        getSpectrum()->stackBuffer(getSpectrum()->getMagnitude(), getSpectrum()->getElemental()->getMagnitude(), 0, getSpectrum()->getElemental()->getStreamSize(), timespan, offset, 1.0, 0.0);
        getSpectrum()->stackBuffer(getSpectrum()->getPhase(), getSpectrum()->getElemental()->getPhase(), 0, getSpectrum()->getElemental()->getStreamSize(), timespan, offset, 1.0, 0.0);
    } else
        getSpectrum()->stackBuffer(getSpectrum()->getSeries(), getSpectrum()->getElemental()->getBuffer(), 0, getSpectrum()->getElemental()->getStreamSize(), timespan, offset, 1.0, 0.0);
    getSpectrum()->buildHistogram(getSpectrum()->getMagnitude(), getSpectrum()->getElemental()->getStream()->magnitude, 100);
    getGraph()->repaint();
    gethistogram()->repaint();
}

Baseline::~Baseline()
{
    threadRunning = false;
}
