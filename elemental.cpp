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

#include "elemental.h"

Elemental::Elemental(QObject *parent) : QObject(parent)
{
    stream = dsp_stream_new();
    stream->magnitude = dsp_stream_new();
    stream->phase = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    scanThread = new Thread(this);
    connect(scanThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [ = ] (Thread * thread)
    {
        Elemental* parent = (Elemental*)thread->getParent();
        dsp_stream_p stream = parent->getStream();
        success = false;
        offset = 0.0;
        scale = 1.0;
        if(stream->stars_count > 2 && reference->stars_count > 2)
        {
            dsp_align_info info = vlbi_astro_align_spectra(stream, reference, parent->getMaxDots(), parent->getDecimals(),
                                  parent->getMinScore());
            success = (info.err & DSP_ALIGN_NO_MATCH) == 0;
            if(success)
            {
                pwarn("Match found - score: %lf%%\n offset: %lf\n scale: %lf\n", 100.0 - info.score * 100.0, info.offset[0],
                      info.factor[0]);
                offset = info.offset[0];
                scale = info.factor[0];
                matches++;
            }
        }
        finish(success, offset, scale);
        thread->stop();
        thread->unlock();
    });
}

Elemental::~Elemental()
{
    scanThread->~Thread();
    dsp_stream_free_buffer(stream);
    dsp_stream_free(stream);
}

QStringList Elemental::getElementNames()
{
    QStringList names;
    for (dsp_stream_p element : getCatalog())
    {
        names.append(element->name);
    }
    return names;
}

void Elemental::loadSpectrum(QString spectrumPath)
{
    unloadCatalog();
    reference = vlbi_astro_load_spectrum((char*)spectrumPath.toStdString().c_str());
}

void Elemental::loadCatalog(QString catalogPath)
{
    unloadCatalog();
    dsp_stream_p *catalog = nullptr;
    int catalog_size = 0;
    vlbi_astro_load_spectra_catalog((char*)catalogPath.toStdString().c_str(), &catalog, &catalog_size);
    reference = vlbi_astro_create_reference_catalog(catalog, catalog_size);
    for(int c = 0; c < catalog_size; c++)
    {
        elements.append(catalog[c]);
    }
}

void Elemental::unloadCatalog()
{
    if(elements.empty())
        return;
    for (dsp_stream_p element : elements)
    {
        dsp_stream_free_buffer(element);
        dsp_stream_free(element);
    }
    if(reference != nullptr)
    {
        dsp_stream_free_buffer(reference);
        dsp_stream_free(reference);
    }
}

dsp_align_info *Elemental::stats(QString name)
{
    for (dsp_stream_p element : getCatalog())
    {
        if (QString(element->name) == name)
        {
            return &element->align_info;
        }
    }
    return nullptr;
}

void Elemental::run()
{
    vlbi_astro_scan_spectrum(stream, getSampleSize());
    pwarn("Found %d lines\n", stream->stars_count);
    if(!scanThread->isRunning())
        scanThread->start();
}

void Elemental::finish(bool done, double ofs, double sc)
{
    emit scanFinished(done, ofs, sc);
}

void Elemental::setBuffer(double * buf, int len)
{
    success = false;
    offset = 0.0;
    scale = 1.0;
    dsp_stream_set_dim(stream, 0, len);
    dsp_stream_alloc_buffer(stream, stream->len);
    dsp_buffer_copy(buf, stream->buf, stream->len);
}

void Elemental::setMagnitude(double * buf, int len)
{
    dsp_stream_set_dim(stream, 0, len);
    dsp_stream_alloc_buffer(stream, stream->len);
    dsp_buffer_copy(buf, stream->magnitude->buf, stream->len);
}

void Elemental::setPhase(double * buf, int len)
{
    dsp_stream_set_dim(stream, 0, len);
    dsp_stream_alloc_buffer(stream, stream->len);
    dsp_buffer_copy(buf, stream->phase->buf, stream->len);
}

void Elemental::setReal(double * buf, int len)
{
    dsp_stream_set_dim(stream, 0, len);
    dsp_stream_alloc_buffer(stream, stream->len);
    for(int i = 0; i < stream->len; i++)
        stream->dft.complex[i].real = buf[i];
    dsp_fourier_2dsp(stream);
}

void Elemental::setImaginary(double * buf, int len)
{
    dsp_stream_set_dim(stream, 0, len);
    dsp_stream_alloc_buffer(stream, stream->len);
    for(int i = 0; i < stream->len; i++)
        stream->dft.complex[i].imaginary = buf[i];
    dsp_fourier_2dsp(stream);
}

void Elemental::idft()
{
    dsp_fourier_2complex_t(stream);
    dsp_fourier_idft(stream);
    dsp_buffer_shift(stream);
}

void Elemental::dft(int depth)
{
    dsp_fourier_dft(stream, depth);
    dsp_fourier_2dsp(stream);
    dsp_buffer_shift(stream);
}

void Elemental::clear()
{
    dsp_stream_set_dim(stream, 0, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
}

double *Elemental::histogram(int size, dsp_stream_p str)
{
    if(str == nullptr)
        str = getStream();
    dsp_stream_p tmp = dsp_stream_copy(str);
    dsp_t *buf = tmp->buf;
    dsp_buffer_stretch(tmp->buf, tmp->len, 0.0, size);
    dsp_stream_set_buffer(tmp, &buf[2], tmp->len-2);
    double *histo = dsp_stats_histogram(tmp, size);
    dsp_stream_set_buffer(tmp, buf, tmp->len+2);
    dsp_stream_free_buffer(tmp);
    dsp_stream_free(tmp);
    return histo;
}
