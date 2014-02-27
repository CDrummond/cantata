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
 
#include "thinsplitterhandle.h"
#include "utils.h"

ThinSplitterHandle::ThinSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
{
    size=Utils::isHighDpi() ? 8 : 4;
    updateMask();
    setAttribute(Qt::WA_MouseNoMask, true);
}

void ThinSplitterHandle::resizeEvent(QResizeEvent *event)
{
    updateMask();
    QSplitterHandle::resizeEvent(event);
}

void ThinSplitterHandle::updateMask()
{
    if (Qt::Horizontal==orientation()) {
        setContentsMargins(size, 0, size, 0);
        setMask(QRegion(contentsRect().adjusted(-size, 0, size, 0)));
    } else {
        setContentsMargins(0, size, 0, size);
        setMask(QRegion(contentsRect().adjusted(0, -size, 0, size)));
    }
}


