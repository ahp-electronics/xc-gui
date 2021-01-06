#ifndef LINE_H
#define LINE_H

#include <QWidget>
#include <QLineSeries>
#include <ahp_xc.h>

using namespace QtCharts;

namespace Ui {
class Line;
}

class Line : public QWidget
{
    Q_OBJECT

public:
    explicit Line(QString name, int n, QWidget *parent = nullptr);
    ~Line();

    int getFlags() { return flags; }
    inline QLineSeries* getDots() { return series; }

private:
    QLineSeries *series;
    int line;
    int flags;
    Ui::Line *ui;
};

#endif // LINE_H
