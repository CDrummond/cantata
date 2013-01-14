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

#ifndef ICONS_H
#define ICONS_H

#include "icon.h"

namespace Icons
{
    extern void init();
    #ifndef ENABLE_KDE_SUPPORT
    extern Icon appIcon;
    extern Icon shortcutsIcon;
    #endif
    extern Icon singleIcon;
    extern Icon consumeIcon;
    extern Icon repeatIcon;
    extern Icon shuffleIcon;
    extern Icon libraryIcon;
    extern Icon radioStreamIcon;
    extern Icon wikiIcon;
    extern Icon albumIcon;
    extern Icon streamIcon;
    extern Icon configureIcon;
    extern Icon connectIcon;
    extern Icon disconnectIcon;
    extern Icon speakerIcon;
    extern Icon lyricsIcon;
    extern Icon dynamicIcon;
    extern Icon playlistIcon;
    extern Icon variousArtistsIcon;
    extern Icon artistIcon;
    extern Icon editIcon;
    extern Icon clearListIcon;
    extern Icon menuIcon;
    extern Icon jamendoIcon;
    extern Icon magnatuneIcon;
}

#endif
