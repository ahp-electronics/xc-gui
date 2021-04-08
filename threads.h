#ifndef THREADS_H
#define THREADS_H

#include <QThread>
#include <QWidget>
#include "types.h"

class progressThread : public QThread {
    Q_OBJECT
private:
    QWidget* line;
public:
    progressThread(QWidget *parent = nullptr) : QThread() { line = parent; }
    void run() {
        while(!isInterruptionRequested()) {
            QThread::msleep(20);
             emit progressChanged(line);
        }
        disconnect(this, 0, 0, 0);
    }
    inline void setLine(QWidget* l)  { line = l; }
signals:
    void progressChanged(QWidget* l);
};

#endif // LINE_H
