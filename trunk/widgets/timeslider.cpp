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
#include <QHBoxLayout>
#include <QProxyStyle>
#include <QTimer>
#include <QApplication>
#include <QMouseEvent>
#include <QSlider>

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

static const QLatin1String constEmptyTime("0:00:00");

class TimeLabel : public QLabel
{
public:
    TimeLabel(QWidget *p, QSlider *s)
        : QLabel(p)
        , slider(s)
        , visible(true)
    {
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }

    void setEnabled(bool e)
    {
        if (e!=visible) {
            visible=e;
            QLabel::setEnabled(e);
            if (e) {
                setStyleSheet(QString());
            } else {
                QColor col=palette().text().color();
                setStyleSheet(QString("QLabel { color : rgba(%1, %2, %3, %4); }").arg(col.red()).arg(col.green()).arg(col.blue()).arg(128));
            }
        }
    }

    void updateTime()
    {
        if (visible) {
            updateTimeDisplay();
        } else {
            setText(constEmptyTime);
        }
    }

private:
    virtual void updateTimeDisplay()=0;

protected:
    QSlider *slider;
    bool visible;
};

class TimeTakenLabel : public TimeLabel
{
public:
    TimeTakenLabel(QWidget *p, QSlider *s)
        : TimeLabel(p, s)
    {
        setAlignment((Qt::RightToLeft==layoutDirection() ? Qt::AlignLeft : Qt::AlignRight)|Qt::AlignVCenter);
        setMinimumWidth(fontMetrics().width(constEmptyTime));
    }
    virtual ~TimeTakenLabel() { }

    virtual void updateTimeDisplay()
    {
        setText(Song::formattedTime(slider->value()));
    }
};

class RemainingTimeLabel : public TimeLabel
{
public:
    RemainingTimeLabel(QWidget *p, QSlider *s)
        : TimeLabel(p, s)
        , pressed(false)
        , showRemaining(Settings::self()->showTimeRemaining())
    {
        setAttribute(Qt::WA_Hover, true);
        setAlignment((Qt::RightToLeft==layoutDirection() ? Qt::AlignRight : Qt::AlignLeft)|Qt::AlignVCenter);
        setMinimumWidth(fontMetrics().width(QLatin1String("-")+constEmptyTime));
    }

    virtual ~RemainingTimeLabel() { }

    void saveConfig()
    {
        Settings::self()->saveShowTimeRemaining(showRemaining);
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
                updateTime();
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

    void updateTimeDisplay()
    {
        int value=showRemaining ? slider->maximum()-slider->value() : slider->maximum();
        QString prefix=showRemaining && value ? QLatin1String("-") : QString();
        setText(prefix+Song::formattedTime(value));
    }

private:
    bool pressed;
    bool showRemaining;
};

class PosSlider : public QSlider
{
public:
    PosSlider(QWidget *p)
        : QSlider(p)
        , isActive(false)
        , shown(false)
    {
        setPageStep(0);
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setFocusPolicy(Qt::NoFocus);
        setStyle(new ProxyStyle());
        int h=fontMetrics().height()*0.5;
        setMinimumHeight(h);
        setMaximumHeight(h);
        updateStyleSheet();
    }

    void showEvent(QShowEvent *e)
    {
        QSlider::showEvent(e);
        if (!shown) {
            updateStyleSheet();
            shown=true;
        }
    }

    void updateStyleSheet()
    {
        int lineWidth=maximumHeight()>12 ? 2 : 1;

        QString boderFormat=QLatin1String("QSlider::groove:horizontal { border: %1px solid rgba(%2, %3, %4, %5); "
                                          "background: transparent; "
                                          "border-radius: %6px } ");
        QString fillFormat=QLatin1String("QSlider::")+QLatin1String(Qt::RightToLeft==layoutDirection() ? "add" : "sub")+
                           QLatin1String("-page:horizontal {border: %1px solid rgb(%2, %3, %4); "
                                         "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgb(%5, %6, %7), stop:1 rgb(%8, %9, %10)); "
                                         "border-radius: %11px; margin: %12px;}");
        QLabel lbl(parentWidget());
        lbl.ensurePolished();
        QColor textColor=lbl.palette().text().color();
        int alpha=textColor.value()<32 ? 96 : 64;
        QColor fillBorder=QApplication::palette().highlight().color();
        QColor fillTop=fillBorder.lighter(120);
        QColor fillBot=fillBorder.lighter(80);

        inactiveStyleSheet=boderFormat.arg(lineWidth).arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha/2).arg(lineWidth*2);
        activeStyleSheet=boderFormat.arg(lineWidth).arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha).arg(lineWidth*2);
        activeStyleSheet+=fillFormat.arg(lineWidth).arg(fillBorder.red()).arg(fillBorder.green()).arg(fillBorder.blue())
                            .arg(fillTop.red()).arg(fillTop.green()).arg(fillTop.blue())
                            .arg(fillBot.red()).arg(fillBot.green()).arg(fillBot.blue()).arg(lineWidth).arg(lineWidth*2);
        setStyleSheet(inactiveStyleSheet);
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
        bool active=min!=max;
        QSlider::setRange(min, max);
        setValue(min);
        if (!active) {
            setToolTip(QString());
        }

        if (active!=isActive) {
            isActive=active;
            setStyleSheet(isActive ? activeStyleSheet : inactiveStyleSheet);
        }
    }

private:
    bool isActive;
    bool shown;
    QString activeStyleSheet;
    QString inactiveStyleSheet;
};

TimeSlider::TimeSlider(QWidget *p)
    : QWidget(p)
    , timer(0)
    , lastVal(0)
{
    slider=new PosSlider(this);
    timeTaken=new TimeTakenLabel(this, slider);
    timeLeft=new RemainingTimeLabel(this, slider);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::LeftToRight, this);
    layout->setMargin(0);
    layout->addWidget(timeTaken);
    layout->addWidget(slider);
    layout->addWidget(timeLeft);
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
    timeTaken->setEnabled(min!=max);
    timeLeft->setEnabled(min!=max);
    updateTimes();
}

void TimeSlider::clearTimes()
{
    stopTimer();
    lastVal=0;
    slider->setRange(0, 0);
    timeTaken->setEnabled(false);
    timeLeft->setEnabled(false);
    timeTaken->updateTime();
    timeLeft->updateTime();
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
    timeLeft->saveConfig();
}

void TimeSlider::updateTimes()
{
    if (slider->value()<172800 && slider->value() != slider->maximum()) {
        timeTaken->updateTime();
        timeLeft->updateTime();
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
