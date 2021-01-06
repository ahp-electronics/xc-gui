#include "line.h"
#include "ui_line.h"

Line::Line(QString name, int n, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Line)
{
    series = new QLineSeries();
    series->setName(name);
    line = n;
    flags = 0;
    ui->setupUi(this);
    connect(ui->flag0, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 0);
        flags |= ((state >> 1) & 0x1) << 0;
        ahp_xc_set_leds(line, flags);
        if(!state)
            series->clear();
    });
    connect(ui->flag1, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 1);
        flags |= ((state >> 1) & 0x1) << 1;
        ahp_xc_set_leds(line, flags);
    });
    connect(ui->flag2, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 2);
        flags |= ((state >> 1) & 0x1) << 2;
        ahp_xc_set_leds(line, flags);
    });
    connect(ui->flag3, static_cast<void (QCheckBox::*)(int)>(&QCheckBox::stateChanged), [=](int state) {
        flags &= ~(1 << 3);
        flags |= ((state >> 1) & 0x1) << 3;
        ahp_xc_set_leds(line, flags);
    });
}

Line::~Line()
{
    delete ui;
}
