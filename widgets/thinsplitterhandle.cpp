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
 
#include "thinsplitterhandle.h"
#include "support/utils.h"
#include <QResizeEvent>
#include <QPainter>

ThinSplitterHandle::ThinSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
    , highlightUnderMouse(false)
    , underMouse(false)
{
    sz=Utils::scaleForDpi(4);
    updateMask();
    setAttribute(Qt::WA_MouseNoMask, true);
}

void ThinSplitterHandle::resizeEvent(QResizeEvent *event)
{
    updateMask();
    if (event->size()!=size()) {
        QSplitterHandle::resizeEvent(event);
    }
}

void ThinSplitterHandle::paintEvent(QPaintEvent *event)
{
    if (underMouse) {
        QColor col(palette().highlight().color());
        QPainter p(this);
        int width=Utils::scaleForDpi(2);
        QRect r=event->rect();
        r=QRect(r.x()+((r.width()-width)/2), r.y(), width, r.height());
        col.setAlphaF(0.5);
        p.fillRect(r, col);
        col.setAlphaF(0.1);
        p.fillRect(r.adjusted(-(width/2), 0, width/2, 0), col);
    }
}

bool ThinSplitterHandle::event(QEvent *event)
{
    if (highlightUnderMouse) {
        switch(event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            underMouse = true;
            update();
            break;
        case QEvent::ContextMenu:
        case QEvent::Leave:
        case QEvent::HoverLeave:
            underMouse = false;
            update();
            break;
        default:
            break;
        }
    }

    return QWidget::event(event);
}

void ThinSplitterHandle::updateMask()
{
    if (Qt::Horizontal==orientation()) {
        setContentsMargins(sz, 0, sz, 0);
        setMask(QRegion(contentsRect().adjusted(-sz, 0, sz, 0)));
    } else {
        setContentsMargins(0, sz, 0, sz);
        setMask(QRegion(contentsRect().adjusted(0, -sz, 0, sz)));
    }
}
