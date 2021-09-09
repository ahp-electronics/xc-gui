#ifndef TYPES_H
#define TYPES_H

#include <thread>
#include <QThread>
#include <QMainWindow>
#include <ahp_xc.h>

enum Mode {
    Counter = 0,
    Autocorrelator,
    Crosscorrelator,
};

enum Scale {
    Linear = 0,
    Sqrt,
    Log
};
#endif // TYPES_H
