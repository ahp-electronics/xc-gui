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

#ifndef TYPES_H
#define TYPES_H

#include <QThread>
#include <QMainWindow>
#include <ahp_gt.h>
#include <ahp_xc.h>
#include <vlbi.h>
#include "threads.h"
#include <fftw3.h>

extern vlbi_context context;

enum VlbiContext
{
    vlbi_context_ii = 0,
    vlbi_context_iq,
    vlbi_total_contexts
};

enum Mode
{
    Counter = 0,
    AutocorrelatorI,
    AutocorrelatorIQ,
    CrosscorrelatorII,
    CrosscorrelatorIQ,
    HolographII,
    HolographIQ
};

enum Scale
{
    Linear = 0,
    Sqrt,
    Log
};
#endif // TYPES_H
