#include "baseline.h"
#include "line.h"
Baseline::Baseline(QString n, int index, Line *n1, Line *n2, QSettings *s, QWidget *parent) :
    QWidget(parent)
{
    setAccessibleName("Baseline");
    settings = s;
    name = n;
    Index = index;
    series = new QLineSeries();
    series->setName(name);
    average = new QLineSeries();
    average->setName(name);
    line1 = n1;
    line2 = n2;
    connect(line1, static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged), this,
            [=](Line* sender){
        active &= ~1;
        active |= sender->isActive();
        stop = !isActive();
    });
    connect(line2, static_cast<void (Line::*)(Line*)>(&Line::activeStateChanged), this,
            [=](Line* sender){
        active &= ~2;
        active |= sender->isActive() << 1;
        stop = !isActive();
    });

}

void Baseline::setDelay(double s)
{
    ahp_xc_set_lag_cross((uint32_t)Index, (off_t)s*(ahp_xc_get_frequency()>>ahp_xc_get_frequency_divider()));
}

void Baseline::setMode(Mode m)
{
    mode = m;
    series->clear();
    average->clear();
}

Baseline::~Baseline()
{
    threadRunning = false;
}
