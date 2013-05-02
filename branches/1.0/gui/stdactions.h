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

#ifndef STDACTIONS_H
#define STDACTIONS_H

#include "config.h"
#include "action.h"

class StdActions
{
public:

    static StdActions *self();

    StdActions();

    Action *savePlayQueueAction;
    Action *addToPlayQueueAction;
    Action *replacePlayQueueAction;
    Action *addWithPriorityAction;
    Action *addToStoredPlaylistAction;
    Action *addPrioHighestAction;  // 255
    Action *addPrioHighAction;     // 200
    Action *addPrioMediumAction;   // 125
    Action *addPrioLowAction;      // 50
    Action *addPrioDefaultAction;  // 0
    Action *addPrioCustomAction;
    #ifdef TAGLIB_FOUND
    Action *editTagsAction;
    Action *organiseFilesAction;
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    Action *replaygainAction;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    Action *devicesTabAction;
    Action *copyToDeviceAction;
    Action *deleteSongsAction;
    #endif
    Action *setCoverAction;
    Action *refreshAction;
    Action *removeAction;
    Action *backAction;
};
#endif
