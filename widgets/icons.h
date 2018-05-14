/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "support/icon.h"
#include "config.h"

class Icons
{
public:
    static Icons *self();

    Icons();
    void initSidebarIcons();
    void initToolbarIcons(QColor toolbarText);
    Icon appIcon;
    Icon genreIcon;
    Icon artistIcon;
    const Icon &albumIcon(int size, bool mono=false) const;
    Icon albumIconLarge;
    Icon albumIconSmall;
    Icon albumMonoIcon;
    Icon podcastIcon;
    Icon folderListIcon;
    Icon audioListIcon;
    Icon singleIcon;
    Icon consumeIcon;
    Icon repeatIcon;
    Icon shuffleIcon;
    Icon streamIcon;
    Icon speakerIcon;
    Icon menuIcon;

    Icon playqueueIcon;
    Icon libraryIcon;
    Icon foldersIcon;
    Icon playlistsIcon;
    Icon onlineIcon;
    Icon searchTabIcon;
    Icon infoIcon;
    Icon infoSidebarIcon;
    #ifdef ENABLE_DEVICES_SUPPORT
    Icon devicesIcon;
    #endif

    Icon toolbarMenuIcon;
    Icon toolbarPrevIcon;
    Icon toolbarPlayIcon;
    Icon toolbarPauseIcon;
    Icon toolbarNextIcon;
    Icon toolbarStopIcon;

    QIcon replacePlayQueueIcon;
    QIcon appendToPlayQueueIcon;
    QIcon centrePlayQueueOnTrackIcon;
    QIcon savePlayQueueIcon;
    QIcon cutIcon;
    QIcon addNewItemIcon;
    QIcon editIcon;
    QIcon stopDynamicIcon;
    QIcon searchIcon;
    QIcon addToFavouritesIcon;
    QIcon reloadIcon;
    QIcon configureIcon;
    QIcon connectIcon;
    QIcon disconnectIcon;
    QIcon downloadIcon;
    QIcon removeIcon;
    QIcon minusIcon;
    QIcon addIcon;
    QIcon addBookmarkIcon;
    QIcon playlistListIcon;
    QIcon dynamicListIcon;
    QIcon rssListIcon;
    QIcon savedRssListIcon;
    QIcon clockIcon;
    QIcon streamListIcon;
    QIcon streamCategoryIcon;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    QIcon httpStreamIcon;
    #endif
    QIcon leftIcon;
    QIcon rightIcon;
    QIcon upIcon;
    QIcon downIcon;
    QIcon cancelIcon;
    QIcon refreshIcon;
};

#endif
