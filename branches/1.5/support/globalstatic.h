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

#ifndef GLOBAL_STATIC_H
#define GLOBAL_STATIC_H

#include <qglobal.h>

#ifdef ENABLE_KDE_SUPPORT

#include <KDE/KGlobal>
#define GLOBAL_STATIC(CLASS, VAR) \
    K_GLOBAL_STATIC(CLASS, VAR) \
    CLASS * CLASS::self() { return VAR; }

#elif QT_VERSION >= 0x050100 || (QT_VERSION < 0x050000 && QT_VERSION >= 0x040800)

#define GLOBAL_STATIC(CLASS, VAR) \
    Q_GLOBAL_STATIC(CLASS, VAR) \
    CLASS * CLASS::self() { return VAR(); }

#else

#define GLOBAL_STATIC(CLASS, VAR) \
    CLASS * CLASS::self() \
    { \
        static CLASS *VAR=0; \
        if (!VAR) { \
            VAR=new CLASS; \
        } \
        return VAR; \
    }

#endif

#endif
