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

#ifndef ICON_H
#define ICON_H

#include <QIcon>

class QToolButton;

namespace Icon
{
    extern int stdSize(int s);
    extern int dlgIconSize();
    extern void init(QToolButton *btn, bool setFlat=true);
    inline QIcon get(const QString &name) { return name.startsWith(":") || name.startsWith("/") ? QIcon(name) : QIcon::fromTheme(name); }

    enum Std {
        Close,
        Clear
    };
    extern QPixmap getScaledPixmap(const QIcon &icon, int w, int h, int base);
}

#endif
