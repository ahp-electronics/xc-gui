#ifndef LINE_H
#define LINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QSettings>
#include <QLineSeries>
#include <QLineSeries>
#include <ahp_xc.h>
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
    void setActive(bool a) { running = a; }
    inline int getFlags() { return flags; }
    void setFlag(int flag, bool value);
    bool getFlag(int flag);
    inline QLineSeries* getDots() { return &series; }
    inline QLineSeries* getAverage() { return &average; }
    inline QLineSeries* getCounts() { return &counts; }
    inline QLineSeries* getAutocorrelations() { return &autocorrelations; }
    inline QLineSeries* getCrosscorrelations() { return &crosscorrelations; }
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

private:
    bool running;
    QString name;
    QSettings *settings;
    ahp_xc_sample *spectrum;
    QList<Line*> *parents;
    QList<Baseline*> nodes;
    bool scanning;
    int stop;
    double stack;
    Mode mode;
    QMap<int, double> dark;
    QList<double> values;
    QLineSeries series;
    QLineSeries average;
    QLineSeries counts;
    QLineSeries autocorrelations;
    QLineSeries crosscorrelations;
    unsigned int line;
    int flags;
    Ui::Line *ui;
    int old_index2;
};

#endif // LINE_H
