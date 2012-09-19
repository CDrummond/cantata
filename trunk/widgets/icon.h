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
#define Icn KIcon
#else

#include <QtGui/QIcon>
#define Icon(X) QIcon::fromTheme(X)
#define MediaIcon(X) Icon::getMediaIcon(X)
#define Icn QIcon
#endif

class QToolButton;

namespace Icon
{
    extern QIcon createSingleIcon();
    extern QIcon createConsumeIcon();
    extern int stdSize(int s);
    extern void init(QToolButton *btn, bool setFlat=true);
    extern void setupIconTheme();
    #if !defined ENABLE_KDE_SUPPORT
    extern QIcon getMediaIcon(const char *name);
    extern QIcon create(const QStringList &sizes);
    #endif

    extern Icn configureIcon;
    extern Icn connectIcon;
    extern Icn disconnectIcon;
    extern Icn speakerIcon;
    extern Icn lyricsIcon;
    extern Icn dynamicIcon;
    extern Icn playlistIcon;
    extern Icn variousArtistsIcon;
    extern Icn artistIcon;
}

#endif
