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
#include "settings.h"
#include <QLabel>
#include <QGridLayout>
#include <QProxyStyle>
#include <QTimer>
#include <QApplication>
#include <QMouseEvent>

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

class TimeLabel : public QLabel
{
public:
    TimeLabel(QWidget *p, QSlider *s)
        : QLabel(p)
        , slider(s)
        , visible(true)
        , pressed(false)
        , showRemaining(Settings::self()->showTimeRemaining())
    {
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        setAttribute(Qt::WA_Hover, true);
    }

    virtual ~TimeLabel() { }

    void saveConfig()
    {
        Settings::self()->saveShowTimeRemaining(showRemaining);
    }

    void setEnabled(bool e)
    {
        if (e!=visible) {
            visible=e;
            QLabel::setEnabled(e);
            setStyleSheet(e ? QString() : QLatin1String("QLabel { color : transparent; }"));
        }
    }

    bool event(QEvent *e)
    {
        switch (e->type()) {
        case QEvent::MouseButtonPress:
            if (visible && Qt::NoModifier==static_cast<QMouseEvent *>(e)->modifiers() && Qt::LeftButton==static_cast<QMouseEvent *>(e)->button()) {
                pressed=true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (visible && pressed) {
                showRemaining=!showRemaining;
                updateTimes();
            }
            pressed=false;
            break;
        case QEvent::HoverEnter:
            if (visible) {
                QColor col=palette().highlight().color();
                setStyleSheet(QString("QLabel { color : rgb(%1, %2, %3); }").arg(col.red()).arg(col.green()).arg(col.blue()));
            }
            break;
        case QEvent::HoverLeave:
            if (visible) {
                setStyleSheet(QString());
            }
        default:
            break;
        }
        return QLabel::event(e);
    }

    void updateTimes()
    {
        int value=showRemaining ? slider->maximum()-slider->value() : slider->value();
        QString prefix=showRemaining && value ? QLatin1String("-") : QString();
        setText(prefix+Song::formattedTime(value)+" / "+Song::formattedTime(slider->maximum()));
    }

private:
    QSlider *slider;
    bool visible;
    bool pressed;
    bool showRemaining;
};

class PosSlider : public QSlider
{
public:
    PosSlider(QWidget *p)
        : QSlider(p)
    {
        setPageStep(0);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        // Set minimum height to help with some Gtk themes.
        // BUG:179
        setMinimumHeight(24);
        setFocusPolicy(Qt::NoFocus);
        setStyle(new ProxyStyle());
    }

    virtual ~PosSlider() { }

    bool event(QEvent *e)
    {
        if (QEvent::ToolTip==e->type() && maximum()!=minimum()) {
            QHelpEvent *he = dynamic_cast<QHelpEvent *>(e);
            if (he) {
                qreal pc = (qreal)he->x()/(qreal)width();
                setToolTip(Song::formattedTime(maximum()*pc));
            }
        }

        return QSlider::event(e);
    }

    void setRange(int min, int max)
    {
        QSlider::setRange(min, max);
        setValue(min);
        if (min==max) {
            setToolTip(QString());
        }
    }
};

TimeSlider::TimeSlider(QWidget *p)
    : QWidget(p)
    , timer(0)
    , lastVal(0)
{
    slider=new PosSlider(this);
    label=new TimeLabel(this, slider);
    label->setAlignment((Qt::RightToLeft==layoutDirection() ? Qt::AlignRight : Qt::AlignLeft)|Qt::AlignVCenter);
    QGridLayout *layout=new QGridLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->addWidget(label, 0, 0, 1, 1);
    layout->addWidget(slider, 1, 0, 1, 2);
    connect(slider, SIGNAL(sliderPressed()), this, SLOT(pressed()));
    connect(slider, SIGNAL(sliderReleased()), this, SLOT(released()));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(updateTimes()));
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    clearTimes();
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
    label->setEnabled(min!=max);
    updateTimes();
}

void TimeSlider::clearTimes()
{
    stopTimer();
    lastVal=0;
    slider->setRange(0, 0);
    label->setEnabled(false);
    label->updateTimes();
}

void TimeSlider::setOrientation(Qt::Orientation o)
{
    slider->setOrientation(o);
}

int TimeSlider::value() const
{
    return slider->value();
}

void TimeSlider::saveConfig()
{
    label->saveConfig();
}

void TimeSlider::updateTimes()
{
    if (slider->value()<172800 && slider->value() != slider->maximum()) {
        label->updateTimes();
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
