#ifndef BASELINE_H
#define BASELINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QLineSeries>
#include <QSplineSeries>
#include <ahp_xc.h>
#include <cmath>
#include "types.h"

using namespace QtCharts;

class Line;
class Baseline : public QWidget
{
    Q_OBJECT

public:
    explicit Baseline(QString name, Line* n1, Line* n2, QWidget *parent = nullptr);
    ~Baseline();

    inline void setName(QString n) {  name = n; series->setName(n); }
    inline QString getName() { return name; }
    inline QList<double>* getDark() { return &dark; }
    inline QLineSeries* getDots() { return series; }
    inline QLineSeries* getAverage() { return average; }
    void setMode(Mode m);
    void stackCorrelations(Scale scale);
    inline void clearCorrelations() { stack = 0.0; }
    inline void setActive(bool a) { active = a; }
    inline bool isActive() { return active; }
    inline double getPercent() { return percent; }
    inline double isScanning() { return scanning; }
    inline Line* getLine1() { return line1; }
    inline Line* getLine2() { return line2; }

    void setPercent();
private:
    QString name;
    bool active;
    bool scanning;
    bool threadRunning;
    int stop;
    double percent;
    std::thread runThread;
    static void RunThread(Baseline *wnd);
    double stack;
    Mode mode;
    QList<double> dark;
    QList<double> values;
    QLineSeries *series;
    QLineSeries *average;
    Line* line1;
    Line* line2;
};

#endif // BASELINE_H
