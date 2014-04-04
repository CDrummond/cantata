/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpdconnection.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QProxyStyle>
#include <QTimer>
#include <QApplication>
#include <QMouseEvent>
#include <QSlider>
#include <QToolTip>

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

static const QLatin1String constEmptyTime("00:00");

class TimeLabel : public QLabel
{
public:
    TimeLabel(QWidget *p, QSlider *s)
        : QLabel(p)
        , slider(s)
        , largeTime(false)
        , visible(true)
    {
        setFixedWidth(fontMetrics().width(QLatin1String("-")+constEmptyTime));
    }

    void setRange(int min, int max)
    {
        bool e=min!=max;
        if (e!=visible) {
            visible=e;
            QLabel::setEnabled(e);
        }
        bool large=e && max>=(60*60);
        if (largeTime!=large) {
            setFixedWidth(fontMetrics().width(QLatin1String("-")+(large ? QLatin1String("0:") : QString())+constEmptyTime));
            largeTime=large;
        }
    }

    void paintEvent(QPaintEvent *e)
    {
        if (isEnabled()) {
            QLabel::paintEvent(e);
        }
    }

    void updateTime()
    {
        if (visible) {
            updateTimeDisplay();
        } else {
            setText(largeTime ? QLatin1String("0:") : QString()+constEmptyTime);
        }
    }

private:
    virtual void updateTimeDisplay()=0;

protected:
    QSlider *slider;
    bool largeTime;
    bool visible;
};

class TimeTakenLabel : public TimeLabel
{
public:
    TimeTakenLabel(QWidget *p, QSlider *s)
        : TimeLabel(p, s)
    {
        setAlignment((Qt::RightToLeft==layoutDirection() ? Qt::AlignLeft : Qt::AlignRight)|Qt::AlignVCenter);
    }
    virtual ~TimeTakenLabel() { }

    virtual void updateTimeDisplay()
    {
        setText(Utils::formatTime(slider->value()));
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
                setStyleSheet(QLatin1String("QLabel{color:palette(highlight);}"));
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
        setText(prefix+Utils::formatTime(value));
    }

private:
    bool pressed;
    bool showRemaining;
};

PosSlider::PosSlider(QWidget *p)
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
    setMouseTracking(true);
}

void PosSlider::showEvent(QShowEvent *e)
{
    QSlider::showEvent(e);
    if (!shown) {
        updateStyleSheet();
        shown=true;
    }
}

void PosSlider::updateStyleSheet()
{
    int lineWidth=maximumHeight()>12 ? 2 : 1;

    QString boderFormat=QLatin1String("QSlider::groove:horizontal { border: %1px solid rgba(%2, %3, %4, %5); "
                                      "background: solid rgba(%6, %7, %8, %9); "
                                      "border-radius: %10px } ");
    QString fillFormat=QLatin1String("QSlider::")+QLatin1String(Qt::RightToLeft==layoutDirection() ? "add" : "sub")+
            QLatin1String("-page:horizontal {border: %1px solid palette(highlight); "
                          "background: solid palette(highlight); "
                          "border-radius: %2px; margin: %3px;}");
    QLabel lbl(parentWidget());
    lbl.ensurePolished();
    QColor textColor=lbl.palette().color(QPalette::Active, QPalette::Text);
    int alpha=textColor.value()<32 ? 96 : 64;

    inactiveStyleSheet=boderFormat.arg(lineWidth).arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha/2)
                       .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha/8).arg(lineWidth*2);
    activeStyleSheet=boderFormat.arg(lineWidth).arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha)
                     .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha/4).arg(lineWidth*2);
    activeStyleSheet+=fillFormat.arg(lineWidth).arg(lineWidth).arg(lineWidth*2);
    setStyleSheet(isActive ? activeStyleSheet : inactiveStyleSheet);
}

void PosSlider::mouseMoveEvent(QMouseEvent *e)
{
    if (maximum()!=minimum()) {
        qreal pc = (qreal)e->pos().x()/(qreal)width();
        QPoint pos(e->pos().x(), height());
        QToolTip::showText(mapToGlobal(pos), Utils::formatTime(maximum()*pc), this, rect());
    }

    QSlider::mouseMoveEvent(e);
}

void PosSlider::wheelEvent(QWheelEvent *ev)
{
    if (!isActive) {
        return;
    }

    static const int constStep=5;
    int numDegrees = ev->delta() / 8;
    int numSteps = numDegrees / 15;
    int val=value();
    if (numSteps > 0) {
        int max=maximum();
        if (val!=max) {
            for (int i = 0; i < numSteps; ++i) {
                val+=constStep;
                if (val>max) {
                    val=max;
                    break;
                }
            }
        }
    } else {
        int min=minimum();
        if (val!=min) {
            for (int i = 0; i > numSteps; --i) {
                val-=constStep;
                if (val<min) {
                    val=min;
                    break;
                }
            }
        }
    }
    if (val!=value()) {
        setValue(val);
        emit positionSet();
    }
}

void PosSlider::setRange(int min, int max)
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

TimeSlider::TimeSlider(QWidget *p)
    : QWidget(p)
    , timer(0)
    , lastVal(0)
    , pollCount(0)
    , pollMpd(Settings::self()->mpdPoll())
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
    connect(slider, SIGNAL(positionSet()), this, SIGNAL(sliderReleased()));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(updateTimes()));
    if (pollMpd>0) {
        connect(this, SIGNAL(mpdPoll()), MPDConnection::self(), SLOT(getStatus()));
    }
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
    pollCount=0;
}

void TimeSlider::stopTimer()
{
    if (timer) {
        timer->stop();
    }
    pollCount=0;
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
    timeTaken->setRange(min, max);
    timeLeft->setRange(min, max);
    updateTimes();
}

void TimeSlider::clearTimes()
{
    stopTimer();
    lastVal=0;
    slider->setRange(0, 0);
    timeTaken->setRange(0, 0);
    timeLeft->setRange(0, 0);
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
    MPDStatus::self()->setGuessedElapsed(lastVal+elapsed);
    if (pollMpd>0) {
        if (++pollCount>=pollMpd) {
            pollCount=0;
            emit mpdPoll();
        }
    }
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
