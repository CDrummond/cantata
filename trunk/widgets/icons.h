/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "config.h"

class QColor;

class Icons
{
public:
    static Icons *self();

    Icons();
    void initSidebarIcons();
    void initToolbarIcons(const QColor &color, bool forceLight=false);
    #ifndef ENABLE_KDE_SUPPORT
    Icon appIcon;
    Icon shortcutsIcon;
    #endif
    Icon artistIcon;
    Icon albumIcon;
    Icon audioFileIcon;
    Icon playlistIcon;
    Icon dynamicRuleIcon;
    Icon singleIcon;
    Icon consumeIcon;
    Icon repeatIcon;
    Icon shuffleIcon;
    Icon libraryIcon;
    Icon streamCategoryIcon;
    Icon radioStreamIcon;
    Icon addRadioStreamIcon;
    Icon streamIcon;
    Icon configureIcon;
    Icon connectIcon;
    Icon disconnectIcon;
    Icon speakerIcon;
    Icon variousArtistsIcon;
    Icon editIcon;
    Icon searchIcon;
    Icon clearListIcon;
    Icon menuIcon;
    Icon jamendoIcon;
    Icon magnatuneIcon;
    Icon filesIcon;
    Icon cancelIcon;
    Icon importIcon;

    Icon playqueueIcon;
    Icon artistsIcon;
    Icon albumsIcon;
    Icon foldersIcon;
    Icon playlistsIcon;
    Icon dynamicIcon;
    Icon streamsIcon;
    Icon onlineIcon;
    Icon contextIcon;
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
    Icon toolbarVolumeMutedIcon;
    Icon toolbarVolumeLowIcon;
    Icon toolbarVolumeMediumIcon;
    Icon toolbarVolumeHighIcon;
};

#endif
