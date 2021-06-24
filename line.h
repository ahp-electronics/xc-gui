#ifndef LINE_H
#define LINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QSettings>
#include <QScatterSeries>
#include <QSplineSeries>
#include <QLineSeries>
#include <ahp_xc.h>
#include <fftw3.h>
#include <cmath>
#include "types.h"
#include "threads.h"
#include "baseline.h"

using namespace QtCharts;

namespace Ui {
class Line;
}

class Line : public QWidget
{
    Q_OBJECT

public:
    explicit Line(QString name, int n, QSettings *settings = nullptr, QWidget *parent = nullptr, QList<Line*> *p = nullptr);
    ~Line();

    inline bool isActive() { return running; }
    void setActive(bool a) { running = a; emit activeStateChanged(this); }
    inline int getFlags() { return flags; }
    void setFlag(int flag, bool value);
    bool getFlag(int flag);
    inline QSplineSeries* getDots() { return &autocorrelations_dft; }
    inline QSplineSeries* getCounts() { return &counts; }
    inline QSplineSeries* getAutocorrelations() { return &autocorrelations; }
    inline QSplineSeries* getCrosscorrelations() { return &crosscorrelations; }
    bool showCounts();
    bool showAutocorrelations();
    bool showCrosscorrelations();

    void setMode(Mode m);
    Scale getYScale();
    void stackCorrelations();
    void stackCorrelations(unsigned int line2);
    inline void clearCorrelations() { stack = 0.0; }
    inline double getPercent() { return percent; }
    inline double isScanning() { return !stop; }
    inline void addBaseline(Baseline* b) { nodes.append(b); }
    inline unsigned int getLineIndex() { return line; }
    unsigned int getLine2();
    bool haveSetting(QString setting);
    void removeSetting(QString setting);
    void saveSetting(QString name, QVariant value);
    QVariant readSetting(QString name, QVariant defaultValue);
    inline QString readString(QString setting, QString defaultValue) { return readSetting(setting, defaultValue).toString(); }
    inline double readDouble(QString setting, double defaultValue) { return readSetting(setting, defaultValue).toDouble(); }
    inline int readInt(QString setting, int defaultValue) { return readSetting(setting, defaultValue).toInt(); }
    inline bool readBool(QString setting, bool defaultValue) { return readSetting(setting, defaultValue).toBool(); }
    inline bool isRunning() { return running; }
    double percent;

    void setPercent();

signals:
    void activeStateChanged(Line* line);

private:
    double *ac;
    fftw_complex *dft;
    bool running;
    QString name;
    QSettings *settings;
    QList<Line*> *parents;
    QList<Baseline*> nodes;
    bool scanning;
    int stop;
    double stack;
    Mode mode;
    QMap<double, double> dark;
    QMap<double, double> crossdark;
    QMap<double, double>  average;
    QSplineSeries series;
    QSplineSeries counts;
    QSplineSeries autocorrelations_dft;
    QSplineSeries autocorrelations;
    QSplineSeries crosscorrelations;
    unsigned int line;
    int flags;
    Ui::Line *ui;
    int old_index2;
};

#endif // LINE_H
