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

#include "icon.h"
#include <QtGui/QApplication>
#include <QtGui/QToolButton>

int Icon::stdSize(int v)
{
    if (v<20) {
        return 16;
    } else if (v<28) {
        return 22;
    } else if (v<40) {
        return 32;
    } else if (v<56) {
        return 48;
    } else if (v<90) {
        return 64;
    } else {
        return 128;
    }
}

void Icon::init(QToolButton *btn, bool setFlat)
{
    static int size=-1;

    if (-1==size) {
        size=QApplication::fontMetrics().height();
        if (size>18) {
            size=stdSize(size*1.25);
        } else {
            size=16;
        }
    }
    btn->setIconSize(QSize(size, size));
    if (setFlat) {
        btn->setAutoRaise(true);
    }
}

#ifndef ENABLE_KDE_SUPPORT
Icon Icon::getMediaIcon(const QString &name)
{
    static QList<QIcon::Mode> modes=QList<QIcon::Mode>() << QIcon::Normal << QIcon::Disabled << QIcon::Active << QIcon::Selected;
    Icon icn;
    Icon icon(name);

    foreach (QIcon::Mode mode, modes) {
        icn.addPixmap(icon.pixmap(QSize(64, 64), mode).scaled(QSize(28, 28), Qt::KeepAspectRatio, Qt::SmoothTransformation), mode);
        icn.addPixmap(icon.pixmap(QSize(22, 22), mode), mode);
    }

    return icn;
}
#endif

Icon Icon::create(const QString &name, const QList<int> &sizes)
{
    Icon icon;
    foreach (int s, sizes) {
        icon.addFile(QChar(':')+name+QString::number(s), QSize(s, s));
    }
    return icon;
}
