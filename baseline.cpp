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
#include "graph.h"
Baseline::Baseline(QString n, int index, Line *n1, Line *n2, QSettings *s, QWidget *parent) :
    QWidget(parent)
{
    setAccessibleName("Baseline");
    settings = s;
    name = n;
    Index = index;
    localpercent = 0;
    localstop = 1;
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
    readThread = new Thread(this, 0.01, 0.01, "baseline " +name+" read thread");
    resetPercentPtr();
    resetStopPtr();
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    stream->samplerate = 1.0/ahp_xc_get_packettime();
    connect(readThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), this, [ = ](Thread* thread)
    {
        Baseline * line = (Baseline *)thread->getParent();
        line->addCount();
        thread->unlock();
    });
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
        bool newstate = getLine1()->isActive() && getLine2()->isActive() && getLine1()->showCrosscorrelations() && getLine2()->showCrosscorrelations();
        *stop = !newstate;
        if(oldstate != newstate)
        {
            emit activeStateChanging(this);
            if(newstate)
            {
                if(stream != nullptr)
                {
                    lock();
                    getLine2()->resetTimestamp();
                    if(getMode() == HolographIQ)
                        addToVLBIContext();
                    else
                        removeFromVLBIContext();
                    unlock();
                }
                emit activeStateChanged(this);
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
        *stop = !newstate;
        if(oldstate != newstate)
        {
            if(newstate)
            {
                if(stream != nullptr)
                {
                    lock();
                    getLine1()->resetTimestamp();
                    if(getMode() == HolographIQ)
                        addToVLBIContext();
                    else
                        removeFromVLBIContext();
                    unlock();
                }
                emit activeStateChanged(this);
            }
        }
        running = newstate;
        oldstate = newstate;
    });
}

void Baseline::updateBufferSizes()
{
    start1 = getLine1()->getStartChannel();
    start2 = getLine2()->getStartChannel();
    head_size = getLine1()->getEndChannel();
    tail_size = getLine2()->getEndChannel();
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
        ahp_xc_set_channel_cross((uint32_t)getLine1()->getLineIndex(), (off_t)fmax(0, s * ahp_xc_get_frequency()), 1, 0);
        ahp_xc_set_channel_cross((uint32_t)getLine2()->getLineIndex(), (off_t)fmax(0, -s * ahp_xc_get_frequency()), 1, 0);
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
    getCountElemental()->setStreamSize(1);
    getMagnitudeElemental()->setStreamSize(1);
    getPhaseElemental()->setStreamSize(1);
    getElemental()->setStreamSize(1);
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

void Baseline::addCount()
{
    ahp_xc_packet *packet = getPacket();
    double mag = 0.0;
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
            lock();
            double offset1 = 0, offset2 = 0;
            vlbi_context ctx = getVLBIContext();
            if(vlbi_has_node(getVLBIContext(), getLine1()->getName().toStdString().c_str()) && vlbi_has_node(ctx, getLine2()->getName().toStdString().c_str())) {
                vlbi_get_offsets(getVLBIContext(), getPacketTime(), getLine1()->getName().toStdString().c_str(), getLine2()->getName().toStdString().c_str(),
                                 getGraph()->getRa(), getGraph()->getDec(), &offset1, &offset2);
                offset1 /= ahp_xc_get_sampletime();
                offset2 /= ahp_xc_get_sampletime();
                offset1 ++;
                offset2 ++;
                if(ahp_xc_intensity_crosscorrelator_enabled())
                {
                    ahp_xc_set_channel_cross(getLine1()->getLineIndex(), offset1, 1, 0);
                    ahp_xc_set_channel_cross(getLine2()->getLineIndex(), offset2, 1, 0);
                }
                stream->dft.complex[0].real = packet->crosscorrelations[getLineIndex()].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].real;
                stream->dft.complex[0].imaginary = packet->crosscorrelations[getLineIndex()].correlations[ahp_xc_get_crosscorrelator_lagsize() / 2].imaginary;
            } else {
                ahp_xc_set_channel_cross(getLine1()->getLineIndex(), offset1, 1, 0);
                ahp_xc_set_channel_cross(getLine2()->getLineIndex(), offset2, 1, 0);
            }
            unlock();
        }
        break;
    case Counter:
        if(isActive())
        {
            if(getLine1()->showCrosscorrelations() && getLine2()->showCrosscorrelations())
            {
                if(Counts->count() > 0)
                {
                    for(int d = Counts->count() - 1; d >= 0; d--)
                    {
                        if(Counts->at(d).x() < getPacketTime() - getTimeRange())
                            Counts->remove(d);
                    }
                }
                if(ahp_xc_intensity_crosscorrelator_enabled())
                    mag = (double)sqrt(pow(packet->counts[getLine1()->getLineIndex()], 2) + pow(packet->counts[getLine2()->getLineIndex()], 2)) / ahp_xc_get_packettime();
                else
                    mag = (double)packet->crosscorrelations[getLineIndex()].correlations[0].magnitude / ahp_xc_get_packettime();
                if(mag > 0)
                    Counts->append(getPacketTime(), mag);
                active = true;
                smoothBuffer(Counts, Counts->count()-1, 1);
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
            mag = (double)sqrt(pow(packet->counts[getLine1()->getLineIndex()], 2) + pow(packet->counts[getLine2()->getLineIndex()],
                               2)) * M_PI * 2 / pow(packet->counts[getLine1()->getLineIndex()] + packet->counts[getLine2()->getLineIndex()], 2);
            if(mag > 0.0)
            {
                double r = packet->counts[getLine1()->getLineIndex()] * M_PI * 2 / pow(packet->counts[getLine1()->getLineIndex()] + packet->counts[getLine2()->getLineIndex()], 2) / mag;
                double i = packet->counts[getLine2()->getLineIndex()] * M_PI * 2 / pow(packet->counts[getLine1()->getLineIndex()] + packet->counts[getLine2()->getLineIndex()], 2) / mag;
                rad = acos(i);
                if(r < 0.0)
                    rad = M_PI * 2 - rad;
            }
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
                if(getLine1()->showCrosscorrelations() && getLine2()->showCrosscorrelations())
                {
                    Elements->setStreamSize(fmax(2, Elements->getStreamSize()+1));
                    Elements->getStream()->buf[Elements->getStreamSize()-1] = mag;
                    Elements->getStream()->buf[0] = fmin(getLine1()->getMinFrequency(), getLine1()->getMinFrequency());
                    Elements->getStream()->buf[1] = fmax(getLine2()->getMaxFrequency(), getLine2()->getMaxFrequency());
                    dsp_buffer_normalize(Elements->getStream()->buf, Elements->getStreamSize(), Elements->getStream()->buf[0], Elements->getStream()->buf[1]);
                    stack ++;
                    int size = fmin(Elements->getStreamSize(), fmax(getLine1()->getResolution(), getLine2()->getResolution()));
                    dsp_t *buf = Elements->getStream()->buf;
                    dsp_stream_set_buffer(Elements->getStream(), &buf[2], Elements->getStreamSize()-2);
                    double *histo = dsp_stats_histogram(Elements->getStream(), size);
                    dsp_stream_set_buffer(Elements->getStream(), buf, Elements->getStreamSize()+2);
                    Counts->clear();
                    for (int x = 1; x < size; x++)
                    {
                        if(histo[x] != 0)
                            stackValue(Counts, Stack, x, Elements->getStream()->buf[0] + x * (Elements->getStream()->buf[1]-Elements->getStream()->buf[0]) / size, histo[x]);
                    }
                    free(histo);
                    smoothBuffer(Counts, 0, Counts->count());
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

void Baseline::stackValue(QLineSeries* series, QMap<double, double>* stacked, int idx, double x, double y)
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
    vlbi_set_baseline_stream(getVLBIContext(), getLine1()->getLastName().toStdString().c_str(),
                             getLine2()->getLastName().toStdString().c_str(), getStream());
}

void Baseline::removeFromVLBIContext(int index)
{
    if(index < 0)
    {
        index = getMode() - HolographIQ;
        if (index < 0) return;
    }
    vlbi_unlock_baseline(getVLBIContext(), getLine1()->getLastName().toStdString().c_str(),
                         getLine2()->getLastName().toStdString().c_str());
}

bool Baseline::isActive(bool atleast1)
{
    if(atleast1)
        return getLine1()->isActive() || getLine2()->isActive();
    return getLine1()->isActive() && getLine2()->isActive();
}

bool Baseline::scanActive(bool atleast1)
{
    if(atleast1)
        return getLine1()->scanActive() || getLine2()->scanActive();
    return getLine1()->scanActive() && getLine2()->scanActive();
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
    return getLine1()->dft() && getLine2()->dft();
}

int Baseline::smooth()
{
    return fmin(getLine1()->smooth(), getLine2()->smooth());
}

void Baseline::smoothBuffer(QLineSeries* buf, int offset, int len)
{
    if(buf->count() < offset+len)
        return;
    if(buf->count() < smooth())
        return;
    offset = fmax(offset, smooth());
    for(int x = offset; x < offset+len; x++) {
        double val = 0.0;
        for(int y = 0; y < smooth(); y++) {
            val += buf->at(x-y).y();
        }
        val /= smooth();
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), val);
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
    getLine1()->setPercentPtr(percent);
    getLine2()->setPercentPtr(percent);
    getLine1()->resetStopPtr();
    getLine2()->resetStopPtr();
    updateBufferSizes();

    *stop = 0;
    int npackets = 0;
    step = fmax(getLine1()->getScanStep(), getLine2()->getScanStep());
    npackets = ahp_xc_scan_crosscorrelations(getLine1()->getLineIndex(), getLine2()->getLineIndex(), &spectrum, start1,
               head_size, start2, tail_size, step, stop, percent);
    if(spectrum != nullptr && npackets > 0)
    {
        int ofs = head_size/step;
        int lag = ofs-1;
        int _lag = lag;
        bool tail = false;
        for (int x = 0, z = 0; x < npackets; x++, z++)
        {
            lag = spectrum[z].correlations[0].lag / ahp_xc_get_packettime();
            tail = (lag >= 0);
            if (tail)
                lag --;
            lag += ofs;
            if(lag < npackets && lag >= 0)
            {
                magnitude_buf[lag] = (double)spectrum[z].correlations[0].magnitude / spectrum[z].correlations[0].counts;
                phase_buf[lag] = (double)spectrum[z].correlations[0].phase;
                for(int y = lag; y >= 0 && y < len; y += (!tail ? -1 : 1))
                {
                    magnitude_buf[y] = magnitude_buf[lag];
                    phase_buf[y] = phase_buf[lag];
                }
                _lag = lag;
            }
        }
        elemental->setMagnitude(magnitude_buf, npackets);
        elemental->setPhase(phase_buf, npackets);
        if(getLine1()->dft() && getLine2()->dft())
        {
            elemental->dft();
        }
        if(getLine1()->Align() && getLine2()->Align())
            elemental->run();
        else
            elemental->finish(false, -head_size/step, 1.0/step);
        free(spectrum);
    }
    getLine1()->resetPercentPtr();
    getLine2()->resetPercentPtr();
    scanning = false;
}

void Baseline::plot(bool success, double o, double s)
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
            stackValue(getMagnitude(), getMagnitudeStack(), x, ((x + offset) * timespan), elemental->getMagnitude()[x]);
        }
    }
    smoothBuffer(getMagnitude(), 0, getMagnitude()->count());
}

Baseline::~Baseline()
{
    readThread->~Thread();
    threadRunning = false;
}
