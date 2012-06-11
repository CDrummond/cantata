/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "textbrowser.h"
#include <QtGui/QPainter>

void TextBrowser::paintEvent(QPaintEvent *e)
{
    if (drawImage && isReadOnly() && !image.isNull()) {
        QPainter p(viewport());
        p.setOpacity(0.15);
        p.fillRect(rect(), QBrush(image));
    } else if (!viewport()->autoFillBackground()) {
        QPainter p(viewport());
        p.fillRect(rect(), viewport()->palette().base());
    }

    QTextBrowser::paintEvent(e);
}
