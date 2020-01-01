/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "config.h"
#include "volumeslider.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdstatus.h"
#ifdef ENABLE_HTTP_STREAM_PLAYBACK
#include "mpd-interface/httpstream.h"
#endif
#include "support/action.h"
#include "support/actioncollection.h"
#include "gui/stdactions.h"
#include "support/utils.h"
#include "gui/settings.h"
#include <QStyle>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>

class VolumeSliderProxyStyle : public QProxyStyle
{
public:
    VolumeSliderProxyStyle()
        : QProxyStyle()
    {
        setBaseStyle(qApp->style());
    }

    int styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *returnData) const override
    {
        if (SH_Slider_AbsoluteSetButtons==stylehint) {
            return Qt::LeftButton|QProxyStyle::styleHint(stylehint, opt, widget, returnData);
        } else {
            return QProxyStyle::styleHint(stylehint, opt, widget, returnData);
        }
    }
};

static int widthStep=4;
static int constHeightStep=2;

VolumeSlider::VolumeSlider(bool isMpd, QWidget *p)
    : QSlider(p)
    , isMpdVol(isMpd)
    , isActive(isMpd)
    , lineWidth(0)
    , down(false)
    , muteAction(nullptr)
    , menu(nullptr)
{
    widthStep=4;
    setRange(0, 100);
    setPageStep(Settings::self()->volumeStep());
    lineWidth=Utils::scaleForDpi(1);

    int w=lineWidth*widthStep*19;
    int h=lineWidth*constHeightStep*10;
    setFixedHeight(h+1);
    setFixedWidth(w);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setOrientation(Qt::Horizontal);
    setFocusPolicy(Qt::NoFocus);
    setStyle(new VolumeSliderProxyStyle());
    setStyleSheet(QString("QSlider::groove:horizontal {border: 0px;} "
                          "QSlider::sub-page:horizontal {border: 0px;} "
                          "QSlider::handle:horizontal {width: 0px; height:0px; margin:0;}"));
    textCol=Utils::clampColor(palette().color(QPalette::Active, QPalette::Text));
    generatePixmaps();
}

void VolumeSlider::initActions()
{
    if (muteAction) {
        return;
    }
    muteAction = ActionCollection::get()->createAction("mute", tr("Mute"));
    addAction(muteAction);
    connect(muteAction, SIGNAL(triggered()), this, SLOT(muteToggled()));
    connect(StdActions::self()->increaseVolumeAction, SIGNAL(triggered()), this, SLOT(increaseVolume()));
    connect(StdActions::self()->decreaseVolumeAction, SIGNAL(triggered()), this, SLOT(decreaseVolume()));
    if (isMpdVol) {
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
        connect(this, SIGNAL(valueChanged(int)), MPDConnection::self(), SLOT(setVolume(int)));
        connect(this, SIGNAL(toggleMute()), MPDConnection::self(), SLOT(toggleMute()));
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    else {
        connect(this, SIGNAL(valueChanged(int)), HttpStream::self(), SLOT(setVolume(int)));
        connect(this, SIGNAL(toggleMute()), HttpStream::self(), SLOT(toggleMute()));
        connect(HttpStream::self(), SIGNAL(update()), this, SLOT(updateStatus()));
        connect(HttpStream::self(), SIGNAL(isEnabled(bool)), this, SLOT(setEnabled(bool)));
        connect(HttpStream::self(), SIGNAL(isEnabled(bool)), this, SIGNAL(stateChanged()));
    }
    #endif
    addAction(StdActions::self()->increaseVolumeAction);
    addAction(StdActions::self()->decreaseVolumeAction);
}

void VolumeSlider::setColor(const QColor &col)
{
    if (col!=textCol) {
        textCol=col;
        generatePixmaps();
    }
}

void VolumeSlider::paintEvent(QPaintEvent *)
{
    bool reverse=isRightToLeft();
    QPainter p(this);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool muted = isMpdVol ? MPDConnection::self()->isMuted() : HttpStream::self()->isMuted();
    #else
    bool muted = MPDConnection::self()->isMuted();
    #endif
    if (muted || !isEnabled()) {
        p.setOpacity(0.25);
    }

    p.drawPixmap(0, 0, pixmaps[0]);
    #if 1
    int steps=(value()/10.0)+0.5;
    if (steps>0) {
        if (steps<10) {
            int wStep=widthStep*lineWidth;
            p.setClipRect(reverse
                            ? QRect(width()-((steps*wStep*2)-wStep), 0, width(), height())
                            : QRect(0, 0, (steps*wStep*2)-wStep, height()));
            p.setClipping(true);
        }
        p.drawPixmap(0, 0, pixmaps[1]);
        if (steps<10) {
            p.setClipping(false);
        }
    }
    #else // Partial filling of each block?
    if (value()>0) {
        if (value()<100) {
            int fillWidth=(width()*(0.01*value()))+0.5;
            p.setClipRect(reverse
                            ? QRect(width()-fillWidth, 0, width(), height())
                            : QRect(0, 0, fillWidth, height()));
            p.setClipping(true);
        }
        p.drawPixmap(0, 0, *(pixmaps[1]));
        if (value()<100) {
            p.setClipping(false);
        }
    }
    #endif

    if (!muted) {
        p.setOpacity(p.opacity()*0.75);
        p.setPen(textCol);
        QFont f(font());
        f.setPixelSize(qMax(height()/2.5, 8.0));
        p.setFont(f);
        QRect r=rect();
        bool rtl=isRightToLeft();
        if (rtl) {
            r.setX(widthStep*lineWidth*12);
        } else {
            r.setWidth(widthStep*lineWidth*7);
        }
        p.drawText(r, Qt::AlignRight, QString("%1%").arg(value()));
    }
}

void VolumeSlider::mousePressEvent(QMouseEvent *ev)
{
    if (Qt::MiddleButton==ev->buttons()) {
        down=true;
    } else {
        QSlider::mousePressEvent(ev);
    }
}

void VolumeSlider::mouseReleaseEvent(QMouseEvent *ev)
{
    if (down) {
        down=false;
        muteAction->trigger();
        update();
    } else {
        QSlider::mouseReleaseEvent(ev);
    }
}

void VolumeSlider::contextMenuEvent(QContextMenuEvent *ev)
{
    static const char *constValProp="val";
    if (!menu) {
        menu=new QMenu(this);
        muteMenuAction=menu->addAction(tr("Mute"));
        muteMenuAction->setProperty(constValProp, -1);
        for (int i=0; i<11; ++i) {
            menu->addAction(QString("%1%").arg(i*10))->setProperty(constValProp, i*10);
        }
    }

    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool muted = isMpdVol ? MPDConnection::self()->isMuted() : HttpStream::self()->isMuted();
    #else
    bool muted = MPDConnection::self()->isMuted();
    #endif

    muteMenuAction->setText(muted ? tr("Unmute") : tr("Mute"));
    QAction *ret = menu->exec(mapToGlobal(ev->pos()));
    if (ret) {
        int val=ret->property(constValProp).toInt();
        if (-1==val) {
            muteAction->trigger();
        } else {
            setValue(val);
        }
    }
}

void VolumeSlider::wheelEvent(QWheelEvent *ev)
{
    int numDegrees = ev->delta() / 8;
    int numSteps = numDegrees / 15;
    if (numSteps > 0) {
        for (int i = 0; i < numSteps; ++i) {
            increaseVolume();
        }
    } else {
        for (int i = 0; i > numSteps; --i) {
            decreaseVolume();
        }
    }
}

void VolumeSlider::muteToggled()
{
    if (isActive) {
        emit toggleMute();
    }
}

void VolumeSlider::updateStatus()
{
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    int volume=isMpdVol ? MPDStatus::self()->volume() : HttpStream::self()->volume();
    #else
    int volume=MPDStatus::self()->volume();
    #endif

    blockSignals(true);
    if (volume<0) {
        setValue(0);
    } else {
        int unmuteVolume=-1;
        if (0==volume) {
            #ifdef ENABLE_HTTP_STREAM_PLAYBACK
            unmuteVolume=isMpdVol ? MPDConnection::self()->unmuteVolume() : HttpStream::self()->unmuteVolume();
            #else
            unmuteVolume=MPDConnection::self()->unmuteVolume();
            #endif
            if (unmuteVolume>0) {
                volume=unmuteVolume;
            }
        }
        setToolTip(unmuteVolume>0 ? tr("Volume %1% (Muted)").arg(volume) : tr("Volume %1%").arg(volume));
        setValue(volume);
    }
    bool wasEnabled=isEnabled();
    setEnabled(volume>=0);
    update();
    muteAction->setEnabled(isEnabled());
    StdActions::self()->increaseVolumeAction->setEnabled(isEnabled());
    StdActions::self()->decreaseVolumeAction->setEnabled(isEnabled());
    blockSignals(false);
    if (isEnabled()!=wasEnabled) {
        emit stateChanged();
    }
}

void VolumeSlider::increaseVolume()
{
    if (isActive) {
        triggerAction(QAbstractSlider::SliderPageStepAdd);
    }
}

void VolumeSlider::decreaseVolume()
{
    if (isActive) {
        triggerAction(QAbstractSlider::SliderPageStepSub);
    }
}

void VolumeSlider::generatePixmaps()
{
    pixmaps[0]=generatePixmap(false);
    pixmaps[1]=generatePixmap(true);
}

QPixmap VolumeSlider::generatePixmap(bool filled)
{
    bool reverse=isRightToLeft();
    QPixmap pix(size());
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setPen(textCol);
    for (int i=0; i<10; ++i) {
        int barHeight=(lineWidth*constHeightStep)*(i+1);
        QRect r(reverse ? pix.width()-(widthStep+(i*lineWidth*widthStep*2))
                        : i*lineWidth*widthStep*2,
                pix.height()-(barHeight+1), (lineWidth*widthStep)-1, barHeight);
        if (filled) {
            p.fillRect(r.adjusted(1, 1, 0, 0), textCol);
        } else if (lineWidth>1) {
            p.drawRect(r);
            p.drawRect(r.adjusted(1, 1, -1, -1));
        } else {
            p.drawRect(r);
        }
    }
    return pix;
}

#include "moc_volumeslider.cpp"
