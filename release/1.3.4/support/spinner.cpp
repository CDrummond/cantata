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

#include "spinner.h"
#include <QApplication>
#include <QAbstractItemView>

#ifdef ENABLE_KDE_SUPPORT

Spinner::Spinner(QObject *p, bool inMiddle)
    : KPixmapSequenceOverlayPainter(p)
    , active(false)
{
    setSequence(KPixmapSequence("process-working", KIconLoader::SizeSmallMedium));
    setAlignment(inMiddle ? Qt::AlignCenter : (Qt::AlignTop | (Qt::RightToLeft==QApplication::layoutDirection() ? Qt::AlignLeft : Qt::AlignRight)));
}

void Spinner::start()
{
     active=true;
     KPixmapSequenceOverlayPainter::start();
}

void Spinner::stop()
{
     active=false;
     KPixmapSequenceOverlayPainter::stop();
}

void Spinner::setWidget(QWidget *widget)
{
    if (qobject_cast<QAbstractItemView *>(widget)) {
        KPixmapSequenceOverlayPainter::setWidget(static_cast<QAbstractItemView *>(widget)->viewport());
    } else {
        KPixmapSequenceOverlayPainter::setWidget(widget);
    }
}

#else
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <QScrollBar>
#include "utils.h"

Spinner::Spinner(QObject *p, bool inMiddle)
    : QWidget(0)
    , timer(0)
    , space(Utils::isHighDpi() ? 8 : 4)
    , value(0)
    , active(false)
    , central(inMiddle)
    , onView(false)
{
    Q_UNUSED(p)
    int size=fontMetrics().height()*1.5;
    setVisible(false);
    setMinimumSize(size, size);
    setMaximumSize(size, size);
}
void Spinner::setWidget(QWidget *widget)
{
    setParent(widget);
    onView=qobject_cast<QAbstractItemView *>(widget);
}

void Spinner::start()
{
    value=0;
    setVisible(true);
    setPosition();
    active=true;
    if (!timer) {
        timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    }
    timer->start(75);
}

void Spinner::stop()
{
    setVisible(false);
    active=false;
    if (timer) {
        timer->stop();
    }
}

static const int constSpinnerSteps=64;

void Spinner::paintEvent(QPaintEvent *event)
{
    static const int constParts=8;

    int lineWidth(Utils::isHighDpi() ? 4 : 2);
    QPainter p(this);
    QRectF rectangle(1.5, 1.5, size().width()-3, size().height()-3);
    QColor col(palette().color(QPalette::Text));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(event->rect());
    double size=(360*16)/(2.0*constParts);
    for (int i=0; i<constParts; ++i) {
        col.setAlphaF((constParts-i)/(1.0*constParts));
        p.setPen(QPen(col, lineWidth));
        p.drawArc(rectangle, (((constSpinnerSteps-value)*1.0)/(constSpinnerSteps*1.0)*360*16)+(i*2.0*size), size);
    }
    p.end();
}

void Spinner::timeout()
{
    setPosition();
    update();
    if (++value>=constSpinnerSteps) {
        value=0;
    }
}

void Spinner::setPosition()
{
    QWidget *pw=parentWidget();
    int hSpace=space+(onView && pw && static_cast<QAbstractItemView *>(pw)->verticalScrollBar() &&
                      static_cast<QAbstractItemView *>(pw)->verticalScrollBar()->isVisible()
                      ? static_cast<QAbstractItemView *>(pw)->verticalScrollBar()->width()+2 : 0);

    QPoint current=pos();
    QPoint desired=central
                    ? QPoint((parentWidget()->size().width()-size().width())/2, (parentWidget()->size().height()-size().height())/2)
                    : Qt::RightToLeft==QApplication::layoutDirection()
                        ? QPoint(hSpace, space)
                        : QPoint(parentWidget()->size().width()-(size().width()+hSpace), space);

    if (current!=desired) {
        move(desired);
    }
}
#endif
