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
    resetPercentPtr();
    resetStopPtr();
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    stream->samplerate = 1.0/ahp_xc_get_packettime();
    connect(elemental, static_cast<void (Elemental::*)(bool, double, double)>(&Elemental::scanFinished), this, &Baseline::plot);
    lines.clear();
    for(int x = 0; x < getCorrelationOrder(); x++) {
        int idx = (index + x * (index / ahp_xc_get_nlines() + 1)) % ahp_xc_get_nlines();
        lines.append(nodes[idx]);
        connect(getLine(x), static_cast<void (Line::*)()>(&Line::updateBufferSizes), this, &Baseline::updateBufferSizes);
        connect(getLine(x), static_cast<void (Line::*)()>(&Line::clearCrosscorrelations),
                [ = ]()
        {
            getMagnitudeStack()->clear();
            getPhaseStack()->clear();
            getMagnitude()->clear();
            getPhase()->clear();
            getMagnitudes()->clear();
            getPhases()->clear();
            getCounts()->clear();
            stack_index = 0.0;
        });
        connect(getLine(x), static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged),
                [ = ](Line * sender)
        {
            getCounts()->clear();
            bool newstate = getLine(x)->showCrosscorrelations();
            *stop = !getLine(x)->isActive();
            for(int y = 0; y < getCorrelationOrder(); y++) {
                if(y == x) continue;
                newstate = newstate && getLine(y)->showCrosscorrelations();
                *stop = !(*stop && getLine(y)->isActive());
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
    }
}

void Baseline::updateBufferSizes()
{
    for(int x = 0; x < getCorrelationOrder(); x++) {
        start[x] = getLine(x)->getStartChannel();
        head_size = getLine(x)->getEndChannel();
    }
    len = head_size + tail_size;
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
    getMagnitude()->clear();
    getPhase()->clear();
    getCounts()->clear();
    getMagnitudes()->clear();
    getPhases()->clear();
    getCountStack()->clear();
    getMagnitudeStack()->clear();
    getPhaseStack()->clear();
    if(mode == CrosscorrelatorIQ || mode == CrosscorrelatorII)
    {
        for(int x = 0; x < getCorrelationOrder(); x++) {
            connect(getLine(x), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
            connect(getLine(x), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
        }
    }
    else
    {
        *stop = 1;
        for(int x = 0; x < getCorrelationOrder(); x++) {
            disconnect(getLine(x), static_cast<void (Line::*)()>(&Line::savePlot), this, &Baseline::SavePlot);
            disconnect(getLine(x), static_cast<void (Line::*)(Line*)>(&Line::takeDark), this, &Baseline::TakeDark);
        }
    }
}

void Baseline::addCount(double starttime, ahp_xc_packet *packet)
{
    if(packet == nullptr)
        packet = getPacket();
    QLineSeries *Counts = getMagnitudes();
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
            for(int x = 0; x < getCorrelationOrder(); x++) {
                if(getLine(x)->showCrosscorrelations())
                {
                    if(Counts->count() > 0)
                    {
                        for(int d = Counts->count() - 1; d >= 0; d--)
                        {
                            if(Counts->at(d).x() < packet->timestamp + starttime - getTimeRange())
                                Counts->remove(d);
                        }
                    }
                    double mag = 0.0;
                    if(ahp_xc_intensity_crosscorrelator_enabled())
                        for(int x = 0; x < getCorrelationOrder(); x++) {
                            mag += pow(packet->counts[getLine(x)->getLineIndex()], 2);
                        mag = (double)sqrt(mag) / ahp_xc_get_packettime();
                    } else
                        mag = (double)packet->crosscorrelations[Index].correlations[0].magnitude / ahp_xc_get_packettime();
                    if(mag > 0)
                        Counts->append(packet->timestamp + starttime, mag);
                    active = true;
                    smoothBuffer(Counts, Counts->count()-1, 1);
                }
            }
        }
        if(!active)
        {
            Counts->clear();
        }
        break;
    case Spectrograph:
        QLineSeries *counts[2] =
        {
            getMagnitudes(),
            getPhases(),
        };
        QMap<double, double>*stacks[2] =
        {
            getMagnitudeStack(),
            getPhaseStack(),
        };
        Elemental *elementals[2] =
        {
            getMagnitudeElemental(),
            getPhaseElemental(),
        };
        double mag = 0.0;
        double rad = 0.0;
        if(ahp_xc_intensity_crosscorrelator_enabled()) {
            double mag = 0.0;
            for(int x = 0; x < getCorrelationOrder(); x++)
                mag += pow(packet->counts[getLine(x)->getLineIndex()], 2);
            mag = (double)sqrt(mag) / ahp_xc_get_packettime();
        }
        else
        {
            mag = (double)packet->crosscorrelations[getLineIndex()].correlations[0].magnitude / ahp_xc_get_packettime();
            rad = (double)packet->crosscorrelations[getLineIndex()].correlations[0].phase;
        }
        for (int z = 0; z < 2; z++)
        {
            QLineSeries *Counts = counts[z];
            QMap<double, double> *Stack = stacks[z];
            Elemental *Elements = elementals[z];
            if(isActive())
            {
                bool show_correlations = false;
                for(int x = 0; x < getCorrelationOrder(); x++)
                    show_correlations |= getLine(x)->showCrosscorrelations();
                if(show_correlations)
                {
                    Elements->setStreamSize(fmax(2, Elements->getStreamSize()+1));
                    if(Elements->lock()) {
                        Elements->getStream()->buf[Elements->getStreamSize()-1] = mag;
                        double mn = DBL_MAX;
                        double mx = DBL_MIN;
                        for(int x = 0; x < getCorrelationOrder(); x++) {
                            mn = fmin(getLine(x)->getMinFrequency(), mn);
                            mx = fmax(getLine(x)->getMinFrequency(), mx);
                        }
                        Elements->getStream()->buf[0] = mn;
                        Elements->getStream()->buf[1] = mx;
                        Elements->unlock();
                    }
                    Elements->normalize(Elements->getStream()->buf[0], Elements->getStream()->buf[1]);
                    stack_index ++;
                    int size = INT32_MAX;
                    for(int x = 0; x < getCorrelationOrder(); x++)
                        fmin(Elements->getStreamSize(), fmax(getLine(x)->getResolution(), size));
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
            else
            {
                Counts->clear();
                Stack->clear();
                Elements->setStreamSize(2);
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
    lines.clear();
    for(int x = 0; x < correlation_order; x++) {
        int idx = (Index + x * (Index / ahp_xc_get_nlines() + 1)) % ahp_xc_get_nlines();
        lines.append(Nodes.at(idx));
    }
    const char** names = (const char**)malloc(sizeof(const char*)*getCorrelationOrder());
    for(int x = 0; x < getCorrelationOrder(); x++)
        names[x] = getLine(x)->getName().toStdString().c_str();
    if(getMode() == HolographII || getMode() == HolographIQ)
        vlbi_set_baseline_stream(getVLBIContext(), names, getStream());
    MainWindow::unlock_vlbi();
}

void Baseline::stackValue(QLineSeries* series, QMap<double, double>* stacked, double x, double y)
{
    if(y == 0.0) return;
    y /= 2;
    if(getDark()->contains(x))
        y -= getDark()->value(x);
    if(stacked->contains(x))
    {
        y += stacked->value(x) / 2;
    }
    stacked->insert(x, y);
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
    bool active = false;
    for(int x = 0; x < getCorrelationOrder(); x++)
        active |= getLine(x)->isActive();
    if(atleast1)
        return active;
    return running;
}

bool Baseline::scanActive(bool atleast1)
{
    bool active = getLine(0)->scanActive();
    for(int x = 1; x < getCorrelationOrder(); x++) {
        if(atleast1)
            active |= getLine(x)->scanActive();
        else
            active &= getLine(x)->scanActive();
    }
    return active;
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

bool Baseline::dft() {
    bool dft = getLine(0)->dft();
    for(int x = 0; x < getCorrelationOrder(); x++)
        dft &= getLine(x)->dft();
    return dft;
}

int Baseline::smooth()
{
    int value = INT32_MAX;
    for(int x = 0; x < getCorrelationOrder(); x++)
        value = fmin(getLine(x)->smooth(), value);
    return value;
}

void Baseline::smoothBuffer(QLineSeries* buf, int offset, int len)
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

void Baseline::smoothBuffer(double* buf, int len)
{
    for(int x = smooth(); x < len; x++) {
        for(int y = 0; y < smooth(); y++)
            buf[x] += buf[x-y];
        buf[x] /= smooth();
    }
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
    step = fmax(getLine(0)->getScanStep(), getLine(1)->getScanStep());
    npackets = ahp_xc_scan_crosscorrelations(getLine(0)->getLineIndex(), getLine(1)->getLineIndex(), &spectrum, start[0],
               head_size, start[1], tail_size, step, stop, percent);
    if(spectrum != nullptr && npackets > 0)
    {
        int ofs = head_size/step;
        int lag = ofs-1;
        int _lag = lag;
        bool tail = false;
        dsp_buffer_set(magnitude_buf, npackets, 0);
        dsp_buffer_set(phase_buf, npackets, 0);
        for (int x = 0, z = 0; x < npackets; x++, z++)
        {
            lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime();
            if(lag == 0) continue;
            tail = (lag >= 0);
            if (tail)
                lag --;
            lag += ofs;
            ahp_xc_correlation correlation;
            memcpy(&correlation, &spectrum[z].correlations[0], sizeof(ahp_xc_correlation));
            if(correlation.magnitude > 0) {
                if(lag < npackets && lag >= 0)
                {
                    magnitude_buf[lag] = (double)correlation.magnitude / correlation.counts;
                    phase_buf[lag] = (double)correlation.phase;
                    for(int y = lag; y >= 0 && y < len; y += (!tail ? -1 : 1))
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
        if(getLine(0)->dft() && getLine(1)->dft())
        {
            elemental->dft();
        }
        if(getLine(0)->Align() && getLine(1)->Align())
            elemental->run();
        else
            elemental->finish(false, -head_size, step);
        free(spectrum);
    }
    getLine(0)->resetPercentPtr();
    getLine(1)->resetPercentPtr();
    scanning = false;
}

void Baseline::plot(bool success, double o, double s)
{
    double timespan = step;
    if(success)
        timespan = s;
    double offset = o;
    int x = 0;
    getMagnitude()->clear();
    getPhase()->clear();
    for (double t = offset; x < elemental->getStreamSize(); t += timespan, x++)
    {
        stackValue(getMagnitude(), getMagnitudeStack(), ahp_xc_get_sampletime() * t, elemental->getMagnitude()[x]);
    }
    smoothBuffer(getMagnitude(), 0, getMagnitude()->count());
    smoothBuffer(getPhase(), 0, getPhase()->count());
}

Baseline::~Baseline()
{
    threadRunning = false;
}
