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

#ifndef STDACTIONS_H
#define STDACTIONS_H

#include "config.h"
#include "support/action.h"

class StdActions
{
public:
    static StdActions *self();

    StdActions();

    Action *nextTrackAction;
    Action *prevTrackAction;
    Action *playPauseTrackAction;
    Action *stopPlaybackAction;
    Action *stopAfterCurrentTrackAction;
    Action *stopAfterTrackAction;
    Action *increaseVolumeAction;
    Action *decreaseVolumeAction;
    Action *savePlayQueueAction;
    Action *appendToPlayQueueAction;
    Action *appendToPlayQueueAndPlayAction;
    Action *addToPlayQueueAndPlayAction;
    Action *insertAfterCurrentAction;
    Action *replacePlayQueueAction;
    Action *addWithPriorityAction;
    Action *addToStoredPlaylistAction;
    Action *setPriorityAction;
    Action *addToPlayQueueMenuAction;
    Action *addRandomAlbumToPlayQueueAction;
    #ifdef TAGLIB_FOUND
    Action *editTagsAction;
    Action *organiseFilesAction;
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    Action *replaygainAction;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    Action *copyToDeviceAction;
    Action *deleteSongsAction;
    #endif
    Action *setCoverAction;
    Action *removeAction;
    Action *searchAction;
    Action *zoomInAction;
    Action *zoomOutAction;

    void enableAddToPlayQueue(bool en);

private:
    StdActions(const StdActions &o);
    StdActions & operator=(const StdActions &o);
};
#endif
