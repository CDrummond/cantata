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
#define Icon(X) KIcon(X)
#define MediaIcon(X) KIcon(X)

#else

#include <QtGui/QIcon>
#define Icon(X) QIcon::fromTheme(X)
#define MediaIcon(X) Icon::getMediaIcon(X)
#endif

class QToolButton;

namespace Icon
{
    extern int stdSize(int s);
    extern void init(QToolButton *btn, bool setFlat=true);
    #if !defined ENABLE_KDE_SUPPORT
    extern QIcon getMediaIcon(const char *name);
    extern void setupIconTheme();
    #endif
}

#endif
