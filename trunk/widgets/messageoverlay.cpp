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

#include "messageoverlay.h"
#include "toolbutton.h"
#include "icons.h"
#include "support/localize.h"
#include "support/utils.h"
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QApplication>

MessageOverlay::MessageOverlay(QObject *p)
    : QWidget(0)
    , timer(0)
    #ifdef Q_OS_MAC
    , closeOnLeft(true)
     #else
    , closeOnLeft(Utils::Unity==Utils::currentDe())
    #endif
{
    Q_UNUSED(p)
    spacing=fontMetrics().height();
    setVisible(false);
    setMinimumHeight(spacing*2);
    setMaximumHeight(spacing*2);
    cancelButton=new ToolButton(this);
    Icon::init(cancelButton);
    cancelButton->setToolTip(i18n("Cancel"));
    Icon icon=Icon("dialog-close");
    if (icon.isNull()) {
        icon=Icon("window-close");
    }
    cancelButton->setIcon(icon);
    cancelButton->adjustSize();
    connect(cancelButton, SIGNAL(clicked()), SIGNAL(cancel()));
}

void MessageOverlay::setWidget(QWidget *widget)
{
    setParent(widget);
    widget->installEventFilter(this);
}

void MessageOverlay::setText(const QString &txt, int timeout, bool allowCancel)
{
    if (txt==text) {
        return;
    }

    text=txt;
    cancelButton->setVisible(allowCancel);
    setAttribute(Qt::WA_TransparentForMouseEvents, !allowCancel);
    setVisible(!text.isEmpty());
    if (!text.isEmpty()) {
        setSizeAndPosition();
        update();
        if (-1!=timeout) {
            if (!timer) {
                timer=new QTimer(this);
                connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
            }
            timer->start(timeout);
        }
    }
}

void MessageOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QRect r(rect());
    QRectF rf(r.x()+0.5, r.y()+0.5, r.width()-1, r.height()-1);
    QColor borderCol=palette().color(QPalette::Highlight).darker(120);
    QColor col=palette().color(QPalette::Window);
    QPainterPath path=Utils::buildPath(rf, r.height()/4.0);
    col.setAlphaF(0.8);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillPath(path, col);
    p.setPen(QPen(borderCol, qMax(2, r.height()/16)));
    p.drawPath(path);

    int pad=r.height()/4;
    if (cancelButton->isVisible()) {
        if (Qt::LeftToRight==layoutDirection() && !closeOnLeft) {
            rf.adjust(pad, pad, -((pad*2)+cancelButton->width()), -pad);
        } else {
            rf.adjust(((pad*2)+cancelButton->width()), pad, -pad, -pad);
        }
    } else {
        rf.adjust(pad, pad, -pad, -pad);
    }
    QFont fnt(QApplication::font());
    fnt.setBold(true);
    QFontMetrics fm(fnt);
    col=palette().color(QPalette::WindowText);

    p.setPen(col);
    p.setFont(fnt);
    p.drawText(rf, fm.elidedText(text, Qt::ElideRight, r.width(), QPalette::WindowText), QTextOption(Qt::LeftToRight==layoutDirection() ? Qt::AlignLeft : Qt::AlignRight));
    p.end();
}

void MessageOverlay::timeout()
{
    setVisible(false);
    text=QString();
}

bool MessageOverlay::eventFilter(QObject *o, QEvent *e)
{
    if (o==parentWidget() && isVisible() && QEvent::Resize==e->type()) {
        setSizeAndPosition();
    }
    return QObject::eventFilter(o, e);
}

void MessageOverlay::setSizeAndPosition()
{
    int currentWidth=width();
    int desiredWidth=parentWidget()->width()-(spacing*2);
    QPoint currentPos=pos();
    QPoint desiredPos=QPoint(spacing, parentWidget()->height()-(height()+spacing));

    if (currentWidth!=desiredWidth) {
        int pad=height()/4;
        resize(desiredWidth, height());
        cancelButton->move(QPoint(Qt::LeftToRight==layoutDirection() && !closeOnLeft
                                    ? desiredWidth-(cancelButton->width()+pad)
                                    : pad,
                                  (height()-cancelButton->height())/2));
    }

    if (currentPos!=desiredPos) {
        move(desiredPos);
    }
}
