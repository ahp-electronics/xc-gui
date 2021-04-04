#ifndef LINE_H
#define LINE_H

#include <thread>
#include <QThread>
#include <QWidget>
#include <QLineSeries>
#include <QLineSeries>
#include <ahp_xc.h>
#include <cmath>
#include "types.h"
#include "baseline.h"

using namespace QtCharts;

namespace Ui {
class Line;
}

class Line : public QWidget
{
    Q_OBJECT

public:
    explicit Line(QString name, int n, QWidget *parent = nullptr, QList<Line*> *p = nullptr);
    ~Line();

    inline bool isActive() { return (flags&1)==1; }
    void setActive(bool a);
    inline int getFlags() { return flags; }
    void setFlag(int flag, bool value);
    bool getFlag(int flag);
    inline QLineSeries* getDots() { return series; }
    inline QLineSeries* getAverage() { return average; }
    void setMode(Mode m);
    Scale getYScale();
    void stackCorrelations();
    inline void clearCorrelations() { stack = 0.0; }
    inline double getPercent() { return *percent; }
    inline double isScanning() { return scanning; }
    void setPercent();
    inline void setPercentAddr(double *addr) { percent = addr; }
    inline void addBaseline(Baseline* b) { nodes.append(b); }
    inline int getLineIndex() { return line; }

private:
    ahp_xc_sample *spectrum;
    QList<Line*> *parents;
    QList<Baseline*> nodes;
    bool scanning;
    bool threadRunning;
    int stop;
    double *percent;
    std::thread runThread;
    static void RunThread(Line *wnd);
    double stack;
    Mode mode;
    QList<double> dark;
    QList<double> values;
    QLineSeries *series;
    QLineSeries *average;
    int line;
    int flags;
    Ui::Line *ui;
};

#endif // LINE_H
