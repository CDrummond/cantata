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

#ifndef ICON_H
#define ICON_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
#else
#include <QtGui/QIcon>
#endif

class QToolButton;

class Icon : public
    #ifdef ENABLE_KDE_SUPPORT
    KIcon
    #else
    QIcon
    #endif
{
public:
    #ifdef ENABLE_KDE_SUPPORT
    explicit Icon(const QString &icon)
        : KIcon(icon) {
    }
    #else
    explicit Icon(const QString &icon)
        : QIcon(QIcon::fromTheme(icon)) {
    }
    #endif
    Icon() {
    }

    static int stdSize(int s);
    static void init(QToolButton *btn, bool setFlat=true);
    #ifdef ENABLE_KDE_SUPPORT
    static Icon getMediaIcon(const QString &name) {
        return Icon(name);
    }
    #else
    static Icon getMediaIcon(const QString &name);
    #endif

    #ifndef ENABLE_KDE_SUPPORT
    static Icon create(const QString &name, const QList<int> &sizes);
    #endif
};

#endif
