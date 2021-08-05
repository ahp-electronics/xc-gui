#ifndef THREADS_H
#define THREADS_H

#include <QThread>
#include <QWidget>
#include "types.h"

class xcThread : public QThread {
    Q_OBJECT
private:
    QWidget* parent;
public:
    xcThread(QWidget *p = nullptr) : QThread() { parent = p; }
    void run() {
        while(!isInterruptionRequested()) {
            QThread::msleep(20);
             emit threadLoop(parent);
        }
        disconnect(this, 0, 0, 0);
    }
    inline void setParent(QWidget* p)  { parent = p; }
signals:
    void threadLoop(QWidget* p);
};

#endif // LINE_H
