#ifndef THREADS_H
#define THREADS_H

#include <QThread>
#include <QWidget>
#include "types.h"

class Thread : public QThread {
    Q_OBJECT
private:
    QWidget* parent;
public:
    Thread() : QThread() { }
    void run() {
        while(!isInterruptionRequested()) {
            QThread::msleep(20);
             emit threadLoop();
        }
        disconnect(this, 0, 0, 0);
    }
signals:
    void threadLoop();
};

#endif // LINE_H
