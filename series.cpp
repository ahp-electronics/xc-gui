#include "series.h"

Series::Series(QObject *parent) : QObject(parent)
{
    series = new QLineSeries();
    magnitude = new QLineSeries();
    phase = new QLineSeries();
    histogram = new QScatterSeries();
    magnitude_stack = new QMap<double, double>();
    phase_stack = new QMap<double, double>();
    dark = new QMap<double, double>();
    stack = new QMap<double, double>();
    histogram_stack = new QMap<double, double>();
    elemental = new Elemental();
    raw = new QList<double>();
}

Series::~Series()
{
    getSeries()->~QLineSeries();
    getMagnitude()->~QLineSeries();
    getPhase()->~QLineSeries();
    getHistogram()->~QScatterSeries();
    getDark()->~QMap<double, double>();
    getStack()->~QMap<double, double>();
    getHistogramStack()->~QMap<double, double>();
    getElemental()->~Elemental();
    getRaw()->~QList();
}

void Series::setName(QString name)
{
    getSeries()->setName(name);
}

void Series::addCount(double min_x, double x, double y, double mag, double phi)
{
    if(getSeries()->count() > 0)
    {
        for(int d = getSeries()->count() - 1; d >= 0; d--)
        {
            if(getSeries()->at(d).x() < min_x) {
                getSeries()->remove(d);
                getRaw()->removeAt(d);
            }
        }
    }
    if(getMagnitude()->count() > 0)
    {
        for(int d = getMagnitude()->count() - 1; d >= 0; d--)
        {
            if(getMagnitude()->at(d).x() < min_x) {
                getMagnitude()->remove(d);
            }
        }
    }
    if(getPhase()->count() > 0)
    {
        for(int d = getPhase()->count() - 1; d >= 0; d--)
        {
            if(getPhase()->at(d).x() < min_x) {
                getPhase()->remove(d);
            }
        }
    }
    getRaw()->append(y);
    getSeries()->append(x, y);
    smoothBuffer(getSeries(), 0, getSeries()->count());
    if(mag > -1.0)
        getMagnitude()->append(x, mag);
    if(phi > -1.0)
        getPhase()->append(x, phi);
    smoothBuffer(getSeries(), 0, getSeries()->count());
}

void Series::buildHistogram(QXYSeries *series, dsp_stream_p stream, int histogram_size)
{
    if(getElemental()->lock()) {
        getElemental()->clear();
        getElemental()->setStreamSize(series->count()+2);
        stream->buf[0] = 0;
        stream->buf[1] = ahp_xc_get_frequency();
        for(int x = 0; x < series->count(); x++)
            stream->buf[x+2] = series->at(x).y();
        getElemental()->unlock();
    }
    int size = fmin(getElemental()->getStreamSize(), histogram_size);
    double mn = getElemental()->min(2, stream->len-2);
    double mx = getElemental()->max(2, stream->len-2);
    getElemental()->stretch(0, size);
    dsp_stream_p histo = getElemental()->histogram(size, stream);
    getElemental()->stretch(mn, mx);
    stack_index_histogram ++;
    getHistogram()->clear();
    for (int x = 2; x < size; x++)
    {
        stackHistogram(histo->buf[x], x * mx / mn + mn);
    }
}

void Series::smoothBuffer(QXYSeries *buf, int offset, int len)
{
    if(raw->count() < offset+len)
        return;
    if(raw->count() < getCache())
        return;
    if(buf->count() < offset+len)
        return;
    if(buf->count() < getCache())
        return;
    offset = fmax(offset, getCache());
    int x = 0;
    for(x = offset/2; x < len-offset/2; x++) {
        double val = 0.0;
        for(int y = -offset/2; y < offset/2; y++) {
            val += raw->at(x+y);
        }
        val /= getCache();
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), val);
    }
    for(x = raw->count()-1; x >= raw->count()-offset/2; x--)
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), buf->at(raw->count()-offset/2-1).y());
    for(x = offset/2; x >= 0; x--)
        buf->replace(buf->at(x).x(), buf->at(x).y(), buf->at(x).x(), buf->at(offset/2).y());
}

void Series::stackHistogram(double x, double y)
{
    if(y == 0.0) {
        if(getHistogramStack()->contains(x))
            getHistogram()->append(x, getHistogramStack()->value(x));
        return;
    }
    y /= stack_index_histogram;
    if(getHistogramStack()->contains(x))
        y += getHistogramStack()->value(x) * (stack_index_histogram-1.0) / stack_index_histogram;
    if(y != 0) {
        getHistogramStack()->insert(x, y);
        getHistogram()->append(x, y);
    }
}

void Series::stackBuffer(double *buf, off_t offset, size_t len, double x_scale, double x_offset, double y_scale, double y_offset)
{
    offset = fmax(0, offset);
    getMagnitude()->clear();
    for(off_t x = offset + 1; x < offset+len; x ++)
    {
        stackValue(getMagnitude(), getStack(), x * x_scale + x_offset, buf[x] * y_scale + y_offset);
    }
}

void Series::stackValue(QXYSeries *buf, QMap<double, double>* stack, double x, double y)
{
    if(y == 0.0)
    {
        if(getStack()->contains(x))
            getSeries()->append(x, getStack()->value(x));
        return;
    }
    y /= stack_index;
    if(getDark()->contains(x))
        y -= getDark()->value(x);
    if(getStack()->contains(x))
    {
        y += getStack()->value(x) * (stack_index-1.0) / stack_index;
    }
    if(y != 0) {
        stack->insert(x, y);
        buf->append(x, y);
    }
}

void Series::stretch(Series* series)
{
    double mx = DBL_MIN;
    double mn = DBL_MAX;
    for (int x = 0; x < series->getSeries()->count(); x++)
    {
        mn = fmin(series->getSeries()->at(x).y(), mn);
        mx = fmax(series->getSeries()->at(x).y(), mx);
    }
    for (int x = 0; x < series->getSeries()->count(); x++)
    {
        QPointF p = series->getSeries()->at(x);
        series->getSeries()->replace(x, p.x(), (p.y() - mn) * M_PI * 2.0 / (mx - mn));
    }
}
