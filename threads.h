#ifndef THREADS_H
#define THREADS_H

#include <QThread>
#include <QWidget>
#include <QMutex>
#include "types.h"

class Thread : public QThread {
    Q_OBJECT
private:
    QWidget* parent;
    QMutex mutex;
    int timer_ms;
public:
    Thread(int timer = 20) : QThread() { timer_ms = 20; }
    void run() {
        while(!isInterruptionRequested()) {
            if(lock())
                emit threadLoop(this);
            else
                QThread::msleep(timer_ms);
        }
        disconnect(this, 0, 0, 0);
    }
    bool lock() { return mutex.tryLock(); }
    void unlock() { mutex.unlock(); }
    void setTimer(int timer) { timer_ms = timer; }
signals:
    void threadLoop(Thread *);
};

#endif // LINE_H
