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
    void initToolbarIcons(const QColor &toolbarText);
    QIcon appIcon;
    QIcon genreIcon;
    QIcon artistIcon;
    const QIcon &albumIcon(int size, bool mono=false) const;
    QIcon albumIconLarge;
    QIcon albumIconSmall;
    QIcon albumMonoIcon;
    QIcon podcastIcon;
    QIcon folderListIcon;
    QIcon audioListIcon;
    QIcon singleIcon;
    QIcon consumeIcon;
    QIcon repeatIcon;
    QIcon shuffleIcon;
    QIcon streamIcon;
    QIcon speakerIcon;
    QIcon menuIcon;

    QIcon playqueueIcon;
    QIcon libraryIcon;
    QIcon foldersIcon;
    QIcon playlistsIcon;
    QIcon onlineIcon;
    QIcon searchTabIcon;
    QIcon infoIcon;
    QIcon infoSidebarIcon;
    #ifdef ENABLE_DEVICES_SUPPORT
    QIcon devicesIcon;
    #endif

    QIcon toolbarMenuIcon;
    QIcon toolbarPrevIcon;
    QIcon toolbarPlayIcon;
    QIcon toolbarPauseIcon;
    QIcon toolbarNextIcon;
    QIcon toolbarStopIcon;

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
    QIcon smartPlaylistIcon;
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
