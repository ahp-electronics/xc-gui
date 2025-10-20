#include "series.h"

Series::Series(QObject *parent) : QObject(parent)
{
    series = new QLineSeries();
    magnitude = new QLineSeries();
    phase = new QLineSeries();
    histogram = new QScatterSeries();
    histogram->setMarkerSize(10);
    histogram_magnitude = new QScatterSeries();
    histogram_magnitude->setMarkerSize(10);
    histogram_phase = new QScatterSeries();
    histogram_phase->setMarkerSize(10);
    magnitude_stack = new QMap<double, double>();
    phase_stack = new QMap<double, double>();
    dark = new QMap<double, double>();
    stack = new QMap<double, double>();
    histogram_stack = new QMap<double, double>();
    histogram_stack_magnitude = new QMap<double, double>();
    histogram_stack_phase = new QMap<double, double>();
    elemental = new Elemental();
    raw = new QList<double>();
}

Series::~Series()
{
    getSeries()->~QLineSeries();
    getMagnitude()->~QLineSeries();
    getPhase()->~QLineSeries();
    getHistogram()->~QScatterSeries();
    getHistogramMagnitude()->~QScatterSeries();
    getHistogramPhase()->~QScatterSeries();
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
    if(getSeries()->count() >= 0)
    {
        for(int d = getSeries()->count() - 1; d >= 0; d--)
        {
            if(getSeries()->at(d).x() < min_x) {
                getSeries()->remove(d);
                getRaw()->removeAt(d);
            }
        }
    }
    if(getMagnitude()->count() >= 0)
    {
        for(int d = getMagnitude()->count() - 1; d >= 0; d--)
        {
            if(getMagnitude()->at(d).x() < min_x) {
                getMagnitude()->remove(d);
            }
        }
    }
    if(getPhase()->count() >= 0)
    {
        for(int d = getPhase()->count() - 1; d >= 0; d--)
        {
            if(getPhase()->at(d).x() < min_x) {
                getPhase()->remove(d);
            }
        }
    }
    if(y > -1.0) {
        getRaw()->append(y);
        getSeries()->append(x, y);
    } else y = M_PI * 2;
    if(mag > -1.0)
        getMagnitude()->append(x, mag * y);
    else if(getMagnitude()->count() > 0)
        getMagnitude()->append(x, getMagnitude()->at(getMagnitude()->count()-1).y());
    if(phi > -1.0)
        getPhase()->append(x, phi * y / M_PI / 2);
    else if(getPhase()->count() > 0)
        getPhase()->append(x, getPhase()->at(getPhase()->count()-1).y());
    smoothBuffer(getSeries(), 0, getSeries()->count());
    smoothBuffer(getMagnitude(), 0, getMagnitude()->count());
    smoothBuffer(getPhase(), 0, getSeries()->count());
}

void Series::buildHistogram(QXYSeries *series, dsp_stream_p stream, int histogram_size, int *stack_index, QMap<double, double> *stack, QScatterSeries *histogram)
{
    int size = 1;
    double mn = DBL_MIN;
    double mx = DBL_MAX;
    getElemental()->setStreamSize(series->count()+1);
    if(getElemental()->lock()) {
        for(int x = 0; x < series->count(); x++)
            stream->buf[x] = series->at(x).y();
        size = fmin(stream->len, histogram_size);
        getElemental()->unlock();
    } else return;
    mn = getElemental()->min(0, stream->len);
    mx = getElemental()->max(0, stream->len);
    dsp_stream_p histo = getElemental()->histogram(size, stream);
    if(histo == nullptr) return;
    (*stack_index) ++;
    getHistogram()->clear();
    for (int x = 0; x < size; x++)
    {
        stackHistogram(histo->buf[x], x * (mx - mn) / size + mn, stack_index, stack, histogram);
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

void Series::stackHistogram(double x, double y, int* stack_index, QMap<double, double> *stack, QScatterSeries *series)
{
    x /= *stack_index;
    if(getHistogramStack()->contains(x))
        x += x * ((*stack_index)-1.0) / (*stack_index);
    stack->insert(x, y);
    series->append(x, y);
}

void Series::stackBuffer(QXYSeries *series, QMap<double, double> *stack, double *buf, off_t offset, size_t len, double x_scale, double x_offset, double y_scale, double y_offset)
{
    offset = fmax(0, offset);
    series->clear();
    stack_index ++;
    for(off_t x = offset + 1; x < offset+len; x ++)
    {
        stackValue(series, stack, x * x_scale + x_offset, buf[x] * y_scale + y_offset);
    }
    smoothBuffer(series, 0, series->count());
}

void Series::stackValue(QXYSeries *buf, QMap<double, double>* stack, double x, double y)
{
    if(y == 0.0)
    {
        if(getStack()->keys().contains(x))
            getSeries()->append(x, getStack()->value(x));
        return;
    }
    y /= stack_index;
    if(getDark()->contains(x))
        y -= getDark()->value(x);
    if(getStack()->keys().contains(x))
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
