/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "timeslider.h"
#include "song.h"
#include <QLabel>
#include <QBoxLayout>
#include <QProxyStyle>
#include <QTimer>
#include <QApplication>

class ProxyStyle : public QProxyStyle
{
public:
    ProxyStyle()
        : QProxyStyle()
    {
        setBaseStyle(qApp->style());
    }

    int styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *returnData) const
    {
        if (QStyle::SH_Slider_AbsoluteSetButtons==stylehint) {
            return Qt::LeftButton|QProxyStyle::styleHint(stylehint, opt, widget, returnData);
        } else {
            return QProxyStyle::styleHint(stylehint, opt, widget, returnData);
        }
    }
};

TimeSlider::TimeSlider(QWidget *p)
    : QWidget(p)
    , timer(0)
    , lastVal(0)
{
    label=new QLabel(this);
    slider=new QSlider(this);
    label->setAlignment((Qt::RightToLeft==layoutDirection() ? Qt::AlignRight : Qt::AlignLeft)|Qt::AlignVCenter);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->addWidget(label);
    layout->addWidget(slider);
    slider->setPageStep(0);
    connect(slider, SIGNAL(sliderPressed()), this, SLOT(pressed()));
    connect(slider, SIGNAL(sliderReleased()), this, SLOT(released()));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(updateTimes()));
    slider->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    // Set minimum height to help with some Gtk themes.
    // BUG:179
    slider->setMinimumHeight(24);
    label->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    slider->setFocusPolicy(Qt::NoFocus);
    slider->setStyle(new ProxyStyle());
}

void TimeSlider::startTimer()
{
    if (!timer) {
        timer=new QTimer(this);
        timer->setInterval(1000);
        connect(timer, SIGNAL(timeout()), this, SLOT(updatePos()));
    }
    startTime.restart();
    lastVal=value();
    timer->start();
}

void TimeSlider::stopTimer()
{
    if (timer) {
        timer->stop();
    }
}

void TimeSlider::setValue(int v)
{
    startTime.restart();
    lastVal=v;
    slider->setValue(v);
    updateTimes();
}

void TimeSlider::setRange(int min, int max)
{
    slider->setRange(min, max);
    slider->setValue(min);
    updateTimes();
}

void TimeSlider::clearTimes()
{
    stopTimer();
    lastVal=0;
    slider->setValue(0);
    label->setText(Song::formattedTime(0)+" / "+Song::formattedTime(0));
}

void TimeSlider::updateTimes()
{
    if (slider->value()<172800 && slider->value() != slider->maximum()) {
        label->setText(Song::formattedTime(slider->value())+" / "+Song::formattedTime(slider->maximum()));
    }
}

void TimeSlider::updatePos()
{
    int elapsed=(startTime.elapsed()/1000.0)+0.5;
    slider->setValue(lastVal+elapsed);
}

void TimeSlider::pressed()
{
    if (timer) {
        timer->stop();
    }
}

void TimeSlider::released()
{
    if (timer) {
        timer->start();
    }
    emit sliderReleased();
}
