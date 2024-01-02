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

#include "mainwindow.h"
#include <config.h>

#include <QApplication>

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    int argc = 0;
    char **argv = {NULL};
    QApplication a(argc, argv);
#else
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#endif
    dsp_set_app_name((char*)"xc-gui");
    dsp_set_debug_level(5);
    MainWindow w;
    w.setWindowTitle("XC Gui - Version " XC_GUI_VERSION " Engine " + QString::number(ahp_xc_get_version(), 16));
    QFont font = w.font();
    font.setPixelSize(12);
    w.setFont(font);
    w.show();
    a.setWindowIcon(QIcon(":/icons/icon.ico"));
    return a.exec();
}
