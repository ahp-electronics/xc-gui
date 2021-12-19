/*
    MIT License

    libahp_xc library to drive the AHP XC correlators
    Copyright (C) 2020  Ilia Platone

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#ifndef THREADS_H
#define THREADS_H

#include <QThread>
#include <QWidget>
#include <QMutex>
#include "types.h"

class Thread : public QThread
{
        Q_OBJECT
    private:
        QWidget* parent;
        QMutex mutex;
        int timer_ms;
        int loop_ms;
    public:
        Thread(int timer = 20, int loop = 10) : QThread()
        {
            timer_ms = 20;
            loop_ms = loop;
        }
        void run()
        {
            while(!isInterruptionRequested())
            {
                if(lock())
                    emit threadLoop(this);
                else
                    QThread::msleep(timer_ms);
                QThread::msleep(loop_ms);
            }
            disconnect(this, 0, 0, 0);
        }
        bool lock()
        {
            return mutex.tryLock();
        }
        void unlock()
        {
            mutex.unlock();
        }
        void setTimer(int timer)
        {
            timer_ms = timer;
        }
    signals:
        void threadLoop(Thread *);
};

#endif // LINE_H
