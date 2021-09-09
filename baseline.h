#ifndef BASELINE_H
#define BASELINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QSettings>
#include <QLineSeries>
#include <QSplineSeries>
#include <ahp_xc.h>
#include <cmath>
#include "types.h"
#include "threads.h"

using namespace QtCharts;
class Line;
class Baseline : public QWidget
{
    Q_OBJECT

public:
    Baseline(QString name, int index, Line* n1, Line* n2, QSettings *settings = nullptr, QWidget *parent = nullptr);
    ~Baseline();

    inline void setName(QString n) {  name = n; series->setName(n); }
    inline QString getName() { return name; }
    inline QList<double>* getDark() { return &dark; }
    inline QLineSeries* getDots() { return series; }
    inline QLineSeries* getAverage() { return average; }
    inline QList<double> *getValues() { return &values; }
    void setMode(Mode m);
    inline void setDelay(double s);
    inline bool isActive() { return (active == 3); }
    inline double getPercent() { return percent; }
    inline double isScanning() { return scanning; }
    inline Line* getLine1() { return line1; }
    inline Line* getLine2() { return line2; }

private:
    int Index;
    QSettings *settings;
    QString name;
    int active;
    bool scanning;
    bool threadRunning;
    int stop;
    double percent;
    Mode mode;
    QList<double> dark;
    QList<double> values;
    QLineSeries *series;
    QLineSeries *average;
    Line* line1;
    Line* line2;
};

#endif // BASELINE_H
