#include "elemental.h"

QList <dsp_stream_p> Elemental::elements = QList <dsp_stream_p>();

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
        QList <dsp_stream_p> catalog = parent->getCatalog();
        if(stream->stars_count > 0 && catalog.length() > 0) {
            matches = 0;
            for(dsp_stream_p element : catalog) {
                if(!dsp_align_get_offset(stream, element, 1.0, true, 50.0)) {
                    pinfo("Element %s matches with %lf\% score", element->name, element->align_info.score);
                    matches++;
                }
            }
        }
        thread->requestInterruption();
    });
}

Elemental::~Elemental()
{
    scanThread->requestInterruption();
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

static int dsp_sort_stars_desc(const void *arg1, const void *arg2)
{
    dsp_star* a = (dsp_star*)arg1;
    dsp_star* b = (dsp_star*)arg2;
    if(a->diameter > b->diameter)
        return -1;
    return 1;
}

void Elemental::loadCatalog(QString path)
{
    unloadCatalog();
    dsp_stream_p *catalog = nullptr;
    dsp_debug = 5;
    int ncats = vlbi_astro_load_spectra_catalog((char*)path.toStdString().c_str(), &catalog);
    for(int c = 0; c < ncats; c++) {
        dsp_stream_p element = catalog[c];
        qsort(element->stars, sizeof(dsp_star), element->stars_count, dsp_sort_stars_desc);
        element->stars_count = Min(element->stars_count, 10);
        elements.append(element);
    }
    free(catalog);
}

void Elemental::unloadCatalog()
{
    for (dsp_stream_p element : elements)
    {
        dsp_stream_free_buffer(element);
        dsp_stream_free(element);
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

void Elemental::setBuffer(double * buf, int len)
{
    stream->stars_count = 0;
    double meandev = dsp_stats_stddev(buf, len);
    dsp_star star;
    star.center.dims = 2;
    star.center.location = (double*)malloc(sizeof(double)*2);
    star.center.location[1] = 0;
    for(int x = 0; x < len-getSampleSize(); x ++) {
        double dev = dsp_stats_stddev(((double*)(&buf[x])), getSampleSize());
        if(dev > meandev) {
            star.diameter = dev-meandev;
            star.center.location[0] = x;
            dsp_stream_add_star(stream, star);
        }
    }
    free(star.center.location);
    qsort(stream->stars, sizeof(dsp_star), stream->stars_count, dsp_sort_stars_desc);
    stream->stars_count = Min(stream->stars_count, 10);
    if(!scanThread->isRunning())
        scanThread->start();
}
