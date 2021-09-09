#ifndef THREADS_H
#define THREADS_H

#include <QMetaMethod>
#include <QWidget>
#include <QThread>
#include <thread>
#include "types.h"

class MainWindow;

class Thread : QObject
{
    Q_OBJECT
public:
    void start()
    {
        std::thread(Thread::run, this).detach();
    }
    Thread(QObject* p, const char* signal) : QObject(p) {
        running = true;
        callback = signal;
        parent = p;
    }

    ~Thread() {
        running = false;
    }

    inline bool isRunning() { return running; }
    inline bool Locked() { return locked; }
    inline void Lock() { locked = true; }
    inline void Unlock() { locked = false; }
    inline QObject *getParent() { return parent; }
    inline const char* getCallback() { return callback; }

private:
    static void run(Thread* t)
    {
        while (t->isRunning()) {
            if(!t->Locked())
                QMetaObject::invokeMethod(t->getParent(), t->getCallback());
            else
                QThread::msleep(100);
        }
    }
    const char* callback;
    QObject *parent;
    bool locked { false };
    bool running { false };
};


#endif // LINE_H
