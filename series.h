#ifndef SERIES_H
#define SERIES_H

#include <QObject>
#include <QScatterSeries>
#include <QSplineSeries>
#include <QLineSeries>
#include "graph.h"
#include "types.h"
#include "elemental.h"

class Series : public QObject
{
    Q_OBJECT
private:
    void smoothBuffer(QXYSeries *buf, int offset, int len);
    void stackValue(QXYSeries *buf, QMap<double, double>* stack, double x, double y);
    void stackHistogram(double x, double y);
    void stretch(Series* series);

    int stack_index_histogram { 0 };
    int stack_index { 0 };
    int cache { 0 };
    QList<double> *raw;
    QLineSeries *series;
    QLineSeries *magnitude;
    QLineSeries *phase;
    QScatterSeries *histogram;
    QMap<double, double>* histogram_stack;
    QMap<double, double>* stack;
    QMap<double, double>* magnitude_stack;
    QMap<double, double>* phase_stack;
    QMap<double, double>* dark;
    Elemental* elemental;
public:
    explicit Series(QObject *parent = nullptr);
    ~Series();

    inline void reset()
    {
        stack_index = 0;
        stack_index_histogram = 0;
    }

    inline void clear()
    {
        getSeries()->clear();
        getMagnitude()->clear();
        getMagnitudeStack()->clear();
        getPhase()->clear();
        getPhaseStack()->clear();
        getHistogram()->clear();
        getStack()->clear();
        getHistogramStack()->clear();
        getDark()->clear();
        getElemental()->clear();
        getRaw()->clear();
        stack_index = 0;
        stack_index_histogram = 0;
    }
    inline QLineSeries *getSeries()
    {
        return series;
    }
    inline QLineSeries *getMagnitude()
    {
        return magnitude;
    }
    inline QMap<double, double> *getMagnitudeStack()
    {
        return magnitude_stack;
    }
    inline QMap<double, double> *getPhaseStack()
    {
        return phase_stack;
    }
    inline QLineSeries *getPhase()
    {
        return phase;
    }
    inline QMap<double, double> *getStack()
    {
        return stack;
    }
    inline QScatterSeries *getHistogram()
    {
        return histogram;
    }
    inline QMap<double, double> *getHistogramStack()
    {
        return histogram_stack;
    }
    inline QMap<double, double> *getDark()
    {
        return dark;
    }
    inline Elemental *getElemental()
    {
        return elemental;
    }
    inline QList<double> *getRaw()
    {
        return raw;
    }
    inline int getCache()
    {
        return cache;
    }
    inline void setCache(int len)
    {
        cache = len;
    }
    inline int count() { return getSeries()->count(); }
    void setName(QString name);
    void fill(double* buf, off_t offset, size_t len);
    void addCount(double min_x, double x, double y, double mag, double phi);
    void stackBuffer(QXYSeries *series, double *buf, off_t offset, size_t len, double x_scale, double x_offset, double y_scale, double y_offset);
    void buildHistogram(QXYSeries *series, dsp_stream_p stream, int histogram_size);
signals:

};

#endif // SERIES_H
