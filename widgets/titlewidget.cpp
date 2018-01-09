/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "titlewidget.h"
#include "support/squeezedtextlabel.h"
#include "support/utils.h"
#include "support/icon.h"
#include "gui/stdactions.h"
#include "toolbutton.h"
#ifdef Q_OS_MAC
#include "support/osxstyle.h"
#endif
#include "gui/covers.h"
#include <QAction>
#include <QImage>
#include <QPixmap>
#include <QGridLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>

static int twHeight=-1;

TitleWidget::TitleWidget(QWidget *p)
    : QWidget(p)
    , pressed(false)
    , underMouse(false)
    , controls(nullptr)
{
    QGridLayout *layout=new QGridLayout(this);
    QVBoxLayout *textLayout=new QVBoxLayout(nullptr);
    image=new QLabel(this);
    mainText=new SqueezedTextLabel(this);
    subText=new SqueezedTextLabel(this);
    QLabel *chevron=new QLabel(QChar(Qt::RightToLeft==layoutDirection() ? 0x203A : 0x2039), this);
    QFont f=mainText->font();
    subText->setFont(Utils::smallFont(f));
    mainText->setFont(f);
    if (f.pixelSize()>0) {
        f.setPixelSize(f.pixelSize()*2);
    } else {
        f.setPointSizeF(f.pointSizeF()*2);
    }
    QPalette pal=mainText->palette();
    QColor col(mainText->palette().windowText().color());
    col.setAlphaF(0.5);
    pal.setColor(QPalette::WindowText, col);
    subText->setPalette(pal);
    chevron->setFont(f);
    int spacing=Utils::layoutSpacing(this);
    mainText->ensurePolished();
    subText->ensurePolished();
    int size=mainText->sizeHint().height()+subText->sizeHint().height()+spacing;
    if (size<72) {
        size=Icon::stdSize(size);
    }
    int pad=Utils::scaleForDpi(6);
    size=qMax(qMax(size, QFontMetrics(mainText->font()).height()+QFontMetrics(subText->font()).height()+spacing), Utils::scaleForDpi(40))+pad;
    image->setFixedSize(size, size);
    setToolTip(tr("Click to go back"));
    spacing=qMin(4, spacing-1);
    layout->addItem(new QSpacerItem(spacing, spacing), 0, 0, 2, 1);
    layout->addWidget(chevron, 0, 1, 2, 1);
    layout->addWidget(image, 0, 2, 2, 1);
    textLayout->addWidget(mainText);
    textLayout->addWidget(subText);
    layout->addItem(textLayout, 0, 3, 2, 1);
    mainText->installEventFilter(this);
    subText->installEventFilter(this);
    image->installEventFilter(this);
    installEventFilter(this);
    setAttribute(Qt::WA_Hover);
    connect(Covers::self(), SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverRetrieved(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(coverUpdated(Song,QImage,QString)), this, SLOT(coverRetrieved(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(coverRetrieved(Song,QImage,QString)));
    layout->setMargin(0);
    layout->setSpacing(spacing);
    textLayout->setMargin(0);
    textLayout->setSpacing(spacing);
    mainText->setAlignment(Qt::AlignBottom);
    subText->setAlignment(Qt::AlignTop);
    image->setAlignment(Qt::AlignCenter);
    chevron->setAlignment(Qt::AlignCenter);
    chevron->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    if (-1==twHeight) {
        ToolButton tb;
        twHeight=qMax((tb.iconSize().height()*2), size);
    }
    setFixedHeight(twHeight);
}

void TitleWidget::update(const Song &sng, const QIcon &icon, const QString &text, const QString &sub, bool showControls)
{
    song=sng;
    image->setVisible(true);
    mainText->setText(text);
    subText->setText(sub);
    if (!showControls) {
        if (controls) {
            controls->setVisible(false);
        }
    } else {
        if (!controls) {
            controls=new QWidget(this);
            QVBoxLayout *l=new QVBoxLayout(controls);
            l->setMargin(0);
            l->setSpacing(0);
            ToolButton *add=new ToolButton(this);
            ToolButton *replace=new ToolButton(this);
            add->QAbstractButton::setIcon(StdActions::self()->appendToPlayQueueAction->icon());
            replace->QAbstractButton::setIcon(StdActions::self()->replacePlayQueueAction->icon());
            int size=qMax(add->iconSize().height()+6, (height()/2)-1);
            if (size>(height()-1)) {
                size--;
            }
            add->setFixedSize(QSize(size, size));
            replace->setFixedSize(add->size());
            add->setToolTip(tr("Add All To Play Queue"));
            replace->setToolTip(tr("Add All And Replace Play Queue"));
            l->addWidget(replace);
            l->addWidget(add);
            connect(add, SIGNAL(clicked()), this, SIGNAL(addToPlayQueue()));
            connect(replace, SIGNAL(clicked()), this, SIGNAL(replacePlayQueue()));
            static_cast<QGridLayout *>(layout())->addWidget(controls, 0, 4, 2, 1);
        }
        controls->setVisible(true);
    }
    subText->setVisible(!sub.isEmpty());
    mainText->setAlignment(sub.isEmpty() ? Qt::AlignVCenter : Qt::AlignBottom);
    if (!sng.isEmpty()) {
        Covers::Image cImg=Covers::self()->requestImage(sng, true);
        if (!cImg.img.isNull()) {
            setImage(cImg.img);
            return;
        }
    }
    if (icon.isNull()) {
        image->setVisible(false);
    } else {
        int iconPad=Utils::scaleForDpi(8);
        int iconSize=image->width()-iconPad;
        if (iconSize<44 && iconSize>=32) {
            iconSize=32;
        }
        double dpr=DEVICE_PIXEL_RATIO();
        QPixmap pix=Icon::getScaledPixmap(icon, iconSize*dpr, iconSize*dpr, 96*dpr);
        pix.setDevicePixelRatio(dpr);
        image->setPixmap(pix);
    }
}

bool TitleWidget::eventFilter(QObject *o, QEvent *event)
{
    switch(event->type()) {
    case QEvent::HoverEnter:
        if (isEnabled() && o==this) {
            /*
            #ifdef Q_OS_MAC
            setStyleSheet(QString("QLabel{color:%1;}").arg(OSXStyle::self()->viewPalette().highlight().color().name()));
            #else
            setStyleSheet(QLatin1String("QLabel{color:palette(highlight);}"));
            #endif
            */
            underMouse = true;
        }
        break;
    case QEvent::HoverLeave:
        if (isEnabled() && o==this) {
            //setStyleSheet(QString());
            underMouse = false;
        }
        break;
    case QEvent::MouseButtonPress:
        if (Qt::LeftButton==static_cast<QMouseEvent *>(event)->button() && Qt::NoModifier==static_cast<QMouseEvent *>(event)->modifiers()) {
            pressed=true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (pressed && Qt::LeftButton==static_cast<QMouseEvent *>(event)->button() && !QApplication::overrideCursor()) {
            actions().first()->trigger();
        }
        pressed=false;
        break;
    default:
        break;
    }
    return QWidget::eventFilter(o, event);
}

void TitleWidget::paintEvent(QPaintEvent *e)
{
    if (pressed || underMouse) {
        QPainter p(this);
        #ifdef Q_OS_MAC
        QColor col = OSXStyle::self()->viewPalette().highlight().color();
        #else
        QColor col = palette().highlight().color();
        #endif

        QPainterPath path = Utils::buildPath(rect().adjusted(1, 1, -1, -1), 2.0);
        p.setRenderHint(QPainter::Antialiasing);
        col.setAlphaF(pressed ? 0.5 : 0.2);
        p.fillPath(path, col);
    }
    QWidget::paintEvent(e);
}

void TitleWidget::coverRetrieved(const Song &s, const QImage &img, const QString &file)
{
    Q_UNUSED(file);
    if (song.isEmpty() || img.isNull()) {
        return;
    }
    if (song.artistOrComposer()!=s.artistOrComposer()) {
        return;
    }
    if (s.isArtistImageRequest()!=song.isArtistImageRequest()) {
        return;
    }
    if (s.isComposerImageRequest()!=song.isComposerImageRequest()) {
        return;
    }
    if (!s.isComposerImageRequest() && !s.isArtistImageRequest() && s.album!=song.album) {
        return;
    }
    setImage(img);
}


void TitleWidget::setImage(const QImage &img)
{
    double dpr=DEVICE_PIXEL_RATIO();
    QPixmap pix=QPixmap::fromImage(img.scaled((image->width()-6)*dpr, (image->height()-6)*dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    pix.setDevicePixelRatio(dpr);
    image->setPixmap(pix);
}
