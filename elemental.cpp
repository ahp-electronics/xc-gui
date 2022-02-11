#include "elemental.h"

QList <dsp_stream_p> Elemental::elements = QList <dsp_stream_p>();
dsp_stream_p Elemental::reference = NULL;

Elemental::Elemental(QObject *parent) : QObject(parent)
{
    stream = dsp_stream_new();
    dsp_stream_add_dim(stream, 1);
    dsp_stream_add_dim(stream, 1);
    dsp_stream_alloc_buffer(stream, stream->len);
    scanThread = new Thread(this);
    connect(scanThread, static_cast<void (Thread::*)(Thread*)>(&Thread::threadLoop), [=] (Thread* thread) {
        Elemental* parent = (Elemental*)thread->getParent();
        dsp_stream_p stream = parent->getStream();
        success = false;
        offset = 0.0;
        scale = 1.0;
        if(stream->stars_count > 2 && reference->stars_count > 2) {
            dsp_align_info info = vlbi_astro_align_spectra(stream, reference, parent->getMaxDots(), parent->getDecimals(), parent->getMinScore());
            success = (info.err & DSP_ALIGN_NO_MATCH) == 0;
            if(success) {
                pwarn("Match found - score: %lf%%\n offset: %lf\n scale: %lf\n", 100.0-info.score*100.0, info.offset[0], info.factor[0]);
                offset = info.offset[0];
                scale = info.factor[0];
                matches++;
            }
        }
        emit scanFinished(success, offset, scale);
        thread->requestInterruption();
        thread->unlock();
    });
}

Elemental::~Elemental()
{
    scanThread->requestInterruption();
    scanThread->wait();
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

void Elemental::loadCatalog(QString path)
{
    unloadCatalog();
    dsp_stream_p *catalog = nullptr;
    int catalog_size = 0;
    vlbi_astro_load_spectra_catalog((char*)path.toStdString().c_str(), &catalog, &catalog_size);
    reference = vlbi_astro_create_reference_catalog(catalog, catalog_size);
    for(int c = 0; c < catalog_size; c++) {
        elements.append(catalog[c]);
    }
}

void Elemental::unloadCatalog()
{
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
        if (QString(element->name) == name) {
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

void Elemental::finish()
{
    emit scanFinished(success, offset, scale);
}

void Elemental::setBuffer(double * buf, int len)
{
    success = false;
    offset = 0.0;
    scale = 1.0;
    stream->sizes[0] = len;
    stream->len = len;
    dsp_stream_alloc_buffer(stream, stream->len);
    dsp_buffer_copy(buf, stream->buf, stream->len);
}
