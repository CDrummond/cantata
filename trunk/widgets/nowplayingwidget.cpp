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

#include "nowplayingwidget.h"
#include "mpd/song.h"
#include "gui/settings.h"
#include "mpd/mpdconnection.h"
#include "support/squeezedtextlabel.h"
#include "support/utils.h"
#include "support/localize.h"
#include <QLabel>
#include <QGridLayout>
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
        , pressed(false)
        , showRemaining(Settings::self()->showTimeRemaining())
    {
        setAttribute(Qt::WA_Hover, true);
        setAlignment((Qt::RightToLeft==layoutDirection() ? Qt::AlignLeft : Qt::AlignRight)|Qt::AlignVCenter);
    }

    void setRange(int min, int max)
    {
        QLabel::setEnabled(min!=max);
    }

    void paintEvent(QPaintEvent *e)
    {
        if (isEnabled()) {
            QLabel::paintEvent(e);
        }
    }

    void updateTime()
    {
        if (isEnabled()) {
            int value=showRemaining ? slider->maximum()-slider->value() : slider->maximum();
            QString prefix=showRemaining && value ? QLatin1String("-") : QString();
            setText(QString("%1 / %2").arg(Utils::formatTime(slider->value()), prefix+Utils::formatTime(value)));
        } else {
            setText(constEmptyTime);
        }
    }

    void saveConfig()
    {
        Settings::self()->saveShowTimeRemaining(showRemaining);
    }

    bool event(QEvent *e)
    {
        switch (e->type()) {
        case QEvent::MouseButtonPress:
            if (isEnabled() && Qt::NoModifier==static_cast<QMouseEvent *>(e)->modifiers() && Qt::LeftButton==static_cast<QMouseEvent *>(e)->button()) {
                pressed=true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (isEnabled() && pressed) {
                showRemaining=!showRemaining;
                updateTime();
            }
            pressed=false;
            break;
        case QEvent::HoverEnter:
            if (isEnabled()) {
                setStyleSheet(QLatin1String("QLabel{color:palette(highlight);}"));
            }
            break;
        case QEvent::HoverLeave:
            if (isEnabled()) {
                setStyleSheet(QString());
            }
        default:
            break;
        }
        return QLabel::event(e);
    }

protected:
    QSlider *slider;
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

    setStyleSheet(boderFormat.arg(lineWidth).arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha)
                             .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue()).arg(alpha/4).arg(lineWidth*2)+
                  fillFormat.arg(lineWidth).arg(lineWidth).arg(lineWidth*2));
}

void PosSlider::paintEvent(QPaintEvent *e)
{
    if (isActive) {
        QSlider::paintEvent(e);
    }
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

    isActive=active;
}

NowPlayingWidget::NowPlayingWidget(QWidget *p)
    : QWidget(p)
    , timer(0)
    , lastVal(0)
    , pollCount(0)
    , pollMpd(Settings::self()->mpdPoll())
{
    track=new SqueezedTextLabel(this);
    artist=new SqueezedTextLabel(this);
    slider=new PosSlider(this);
    time=new TimeLabel(this, slider);
    QFont f=track->font();
    f.setBold(true);
    track->setFont(f);
    slider->setOrientation(Qt::Horizontal);
    QGridLayout *layout=new QGridLayout(this);
    int space=Utils::layoutSpacing(this);
    layout->setMargin(space);
    layout->setSpacing(space/2);
    layout->addWidget(track, 0, 0, 1, 2);
    layout->addWidget(artist, 1, 0);
    layout->addWidget(time, 1, 1);
    layout->addWidget(slider, 3, 0, 1, 2);
    connect(slider, SIGNAL(sliderPressed()), this, SLOT(pressed()));
    connect(slider, SIGNAL(sliderReleased()), this, SLOT(released()));
    connect(slider, SIGNAL(positionSet()), this, SIGNAL(sliderReleased()));
    connect(slider, SIGNAL(valueChanged(int)), this, SLOT(updateTimes()));
    if (pollMpd>0) {
        connect(this, SIGNAL(mpdPoll()), MPDConnection::self(), SLOT(getStatus()));
    }
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    clearTimes();
    update(Song());
}

void NowPlayingWidget::update(const Song &song)
{
    QString name=song.name();
    if (song.isEmpty()) {
        track->setText(" ");
        artist->setText(" ");
    } else if (song.isStream() && !song.isCantataStream() && !song.isCdda()) {
        track->setText(name.isEmpty() ? Song::unknown() : name);
        if (song.artist.isEmpty() && song.title.isEmpty() && !name.isEmpty()) {
            artist->setText(i18n("(Stream)"));
        } else {
            artist->setText(song.artist.isEmpty() ? song.title : song.artistSong());
        }
    } else {
        if (song.title.isEmpty() && song.artist.isEmpty() && (!name.isEmpty() || !song.file.isEmpty())) {
            track->setText(name.isEmpty() ? song.file : name);
        } else {
            track->setText(song.title);
        }
        if (song.album.isEmpty() && song.artist.isEmpty()) {
            artist->setText(track->fullText().isEmpty() ? QString() : Song::unknown());
        } else if (song.album.isEmpty()) {
            artist->setText(song.artist);
        } else {
            artist->setText(song.artist+QLatin1String(" - ")+song.displayAlbum());
        }
    }
}

void NowPlayingWidget::startTimer()
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

void NowPlayingWidget::stopTimer()
{
    if (timer) {
        timer->stop();
    }
    pollCount=0;
}

void NowPlayingWidget::setValue(int v)
{
    startTime.restart();
    lastVal=v;
    slider->setValue(v);
    updateTimes();
}

void NowPlayingWidget::setRange(int min, int max)
{
    slider->setRange(min, max);
    time->setRange(min, max);
    updateTimes();
}

void NowPlayingWidget::clearTimes()
{
    stopTimer();
    lastVal=0;
    slider->setRange(0, 0);
    time->setRange(0, 0);
    time->updateTime();
}

int NowPlayingWidget::value() const
{
    return slider->value();
}

void NowPlayingWidget::saveConfig()
{
    time->saveConfig();
}

void NowPlayingWidget::updateTimes()
{
    if (slider->value()<172800 && slider->value() != slider->maximum()) {
        time->updateTime();
    }
}

void NowPlayingWidget::updatePos()
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

void NowPlayingWidget::pressed()
{
    if (timer) {
        timer->stop();
    }
}

void NowPlayingWidget::released()
{
    if (timer) {
        timer->start();
    }
    emit sliderReleased();
}
