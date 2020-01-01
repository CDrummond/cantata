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

#include "titlewidget.h"
#include "support/squeezedtextlabel.h"
#include "support/utils.h"
#include "support/icon.h"
#include "support/proxystyle.h"
#include "support/monoicon.h"
#include "gui/stdactions.h"
#include "toolbutton.h"
#include "groupedview.h"
#ifdef Q_OS_MAC
#include "support/osxstyle.h"
#endif
#include "gui/covers.h"
#include <QAction>
#include <QImage>
#include <QPixmap>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>

TitleWidget::TitleWidget(QWidget *p)
    : QListView(p)
    , pressed(false)
    , controls(nullptr)
{
    setProperty(ProxyStyle::constModifyFrameProp, ProxyStyle::VF_Side|ProxyStyle::VF_Top);
    QHBoxLayout *layout=new QHBoxLayout(this);
    QVBoxLayout *textLayout=new QVBoxLayout(nullptr);
    image=new QLabel(this);
    mainText=new SqueezedTextLabel(this);
    subText=new SqueezedTextLabel(this);
    QLabel *chevron=new QLabel(this);
    ToolButton tb(this);
    tb.setIcon(StdActions::self()->appendToPlayQueueAction->icon());
    tb.ensurePolished();
    chevron->setPixmap(MonoIcon::icon(Qt::LeftToRight==layoutDirection() ? FontAwesome::chevronleft : FontAwesome::chevronright, Utils::monoIconColor()).pixmap(tb.iconSize()));
    chevron->setFixedSize(tb.iconSize());
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
    mainText->ensurePolished();
    subText->ensurePolished();
    setToolTip(tr("Click to go back"));
    int spacing = Utils::layoutSpacing(this);
    layout->addItem(new QSpacerItem(spacing, 1, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(chevron);
    layout->addItem(new QSpacerItem(spacing, 1, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(image);
    layout->addItem(new QSpacerItem(spacing, 1, QSizePolicy::Fixed, QSizePolicy::Fixed));
    textLayout->addWidget(mainText);
    textLayout->addWidget(subText);
    layout->addItem(textLayout);
    mainText->setAttribute(Qt::WA_TransparentForMouseEvents);
    subText->setAttribute(Qt::WA_TransparentForMouseEvents);
    image->setAttribute(Qt::WA_TransparentForMouseEvents);
    chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
    viewport()->installEventFilter(this);
    viewport()->setAttribute(Qt::WA_Hover);
    connect(Covers::self(), SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverRetrieved(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(coverUpdated(Song,QImage,QString)), this, SLOT(coverRetrieved(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(coverRetrieved(Song,QImage,QString)));
    layout->setMargin(0);
    layout->setSpacing(2);
    textLayout->setMargin(0);
    textLayout->setSpacing(2);
    mainText->setAlignment(Qt::AlignBottom);
    subText->setAlignment(Qt::AlignTop);
    image->setAlignment(Qt::AlignCenter);
    chevron->setAlignment(Qt::AlignCenter);
    chevron->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    image->setFixedSize(GroupedView::coverSize(), GroupedView::coverSize());
    setFixedHeight(image->height()+((frameWidth()+2)*2));
    setFocusPolicy(Qt::NoFocus);
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
            add->setIcon(StdActions::self()->appendToPlayQueueAction->icon());
            replace->setIcon(StdActions::self()->replacePlayQueueAction->icon());
            int size=qMin(add->iconSize().height()+6, height()/2);
            add->setFixedSize(QSize(size, size));
            replace->setFixedSize(add->size());
            add->setToolTip(tr("Add All To Play Queue"));
            replace->setToolTip(tr("Add All And Replace Play Queue"));
            l->addWidget(replace);
            l->addWidget(add);
            connect(add, SIGNAL(clicked()), this, SIGNAL(addToPlayQueue()));
            connect(replace, SIGNAL(clicked()), this, SIGNAL(replacePlayQueue()));
            static_cast<QHBoxLayout *>(layout())->addWidget(controls);
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
    if (isEnabled()) {
        switch(event->type()) {
        case QEvent::HoverEnter: {
            QPalette pal = qApp->palette();
            #ifdef Q_OS_MAC
            QColor col(OSXStyle::self()->viewPalette().color(QPalette::Highlight));
            #else
            QColor col(pal.color(QPalette::Highlight));
            #endif
            col.setAlphaF(0.2);
            pal.setColor(QPalette::Base, col);
            setPalette(pal);
            break;
        }
        case QEvent::HoverLeave: {
            QPalette pal = qApp->palette();
            QColor col(pal.color(QPalette::Base));
            pal.setColor(QPalette::Base, col);
            setPalette(pal);
            break;
        }
        case QEvent::MouseButtonPress:
            if (Qt::LeftButton==static_cast<QMouseEvent *>(event)->button() && Qt::NoModifier==static_cast<QMouseEvent *>(event)->modifiers()) {
                QPalette pal = qApp->palette();
                #ifdef Q_OS_MAC
                QColor col(OSXStyle::self()->viewPalette().color(QPalette::Highlight));
                #else
                QColor col(pal.color(QPalette::Highlight));
                #endif
                col.setAlphaF(0.5);
                pal.setColor(QPalette::Base, col);
                setPalette(pal);
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
    }
    return QWidget::eventFilter(o, event);
}

void TitleWidget::coverRetrieved(const Song &s, const QImage &img, const QString &file)
{
    Q_UNUSED(file);
    if (song.isEmpty() || img.isNull()) {
        return;
    }
    if (song.albumArtistOrComposer()!=s.albumArtistOrComposer()) {
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
    QPixmap pix=QPixmap::fromImage(img.scaled(image->width()*dpr, image->height()*dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    pix.setDevicePixelRatio(dpr);
    image->setPixmap(pix);
}

#include "moc_titlewidget.cpp"
