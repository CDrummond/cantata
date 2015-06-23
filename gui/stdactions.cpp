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

#include "stdactions.h"
#include "support/localize.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "models/playlistsmodel.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#endif
#include "support/icon.h"
#include "widgets/icons.h"
#include "support/globalstatic.h"
#include <QMenu>
#include <QCoreApplication>

GLOBAL_STATIC(StdActions, instance)

StdActions::StdActions()
{
    UNITY_MENU_ICON_CHECK
    prevTrackAction = ActionCollection::get()->createAction("prevtrack", i18n("Previous Track"), Icons::self()->toolbarPrevIcon);
    nextTrackAction = ActionCollection::get()->createAction("nexttrack", i18n("Next Track"), Icons::self()->toolbarNextIcon);
    playPauseTrackAction = ActionCollection::get()->createAction("playpausetrack", i18n("Play/Pause"), Icons::self()->toolbarPlayIcon);
    stopPlaybackAction = ActionCollection::get()->createAction("stopplayback", i18n("Stop"), Icons::self()->toolbarStopIcon);
    stopAfterCurrentTrackAction = ActionCollection::get()->createAction("stopaftercurrenttrack", i18n("Stop After Current Track"), Icons::self()->toolbarStopIcon);
    stopAfterTrackAction = ActionCollection::get()->createAction("stopaftertrack", i18n("Stop After Track"), Icons::self()->toolbarStopIcon);
    increaseVolumeAction = ActionCollection::get()->createAction("increasevolume", i18n("Increase Volume"));
    decreaseVolumeAction = ActionCollection::get()->createAction("decreasevolume", i18n("Decrease Volume"));
    savePlayQueueAction = ActionCollection::get()->createAction("saveplaylist", i18n("Save As"), HIDE_MENU_ICON_NAME("document-save-as"));
    addToPlayQueueAction = ActionCollection::get()->createAction("addtoplaylist", i18n("Add To Play Queue"), "list-add");
    replacePlayQueueAction = ActionCollection::get()->createAction("replaceplaylist", i18n("Replace Play Queue"), "media-playback-start");
    addWithPriorityAction = new Action(Icon("favorites"), i18n("Add With Priority"), 0);
    addPrioHighestAction = new Action(i18n("Highest Priority (255)"), 0);
    addPrioHighAction = new Action(i18n("High Priority (200)"), 0);
    addPrioMediumAction = new Action(i18n("Medium Priority (125)"), 0);
    addPrioLowAction = new Action(i18n("Low Priority (50)"), 0);
    addPrioDefaultAction = new Action(i18n("Default Priority (0)"), 0);
    addPrioCustomAction = new Action(i18n("Custom Priority..."), 0);
    addToStoredPlaylistAction = new Action(Icons::self()->playlistIcon, i18n("Add To Playlist"), 0);
    #ifdef TAGLIB_FOUND
    organiseFilesAction = new Action(HIDE_MENU_ICON(Icon("inode-directory")), i18n("Organize Files"), 0);
    editTagsAction = new Action(i18n("Edit Track Information"), 0);
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction = new Action(HIDE_MENU_ICON(Icons::self()->audioFileIcon), i18n("ReplayGain"), 0);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = new Action(HIDE_MENU_ICON(Icon("multimedia-player")), i18n("Copy Songs To Device"), 0);
    copyToDeviceAction->setMenu(DevicesModel::self()->menu());
    deleteSongsAction = new Action(Icon("edit-delete"), i18n("Delete Songs"), 0);
    #endif
    setCoverAction = new Action(i18n("Set Image"), 0);
    removeAction = new Action(Icon("list-remove"), i18n("Remove"), 0);
    searchAction = ActionCollection::get()->createAction("search", i18n("Find"), HIDE_MENU_ICON_NAME("edit-find"));
    searchAction->setShortcut(Qt::ControlModifier+Qt::Key_F);

    addToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());
    addPrioHighestAction->setData(255);
    addPrioHighAction->setData(200);
    addPrioMediumAction->setData(125);
    addPrioLowAction->setData(50);
    addPrioDefaultAction->setData(0);
    addPrioCustomAction->setData(-1);

    QMenu *prioMenu=new QMenu();
    prioMenu->addAction(addPrioHighestAction);
    prioMenu->addAction(addPrioHighAction);
    prioMenu->addAction(addPrioMediumAction);
    prioMenu->addAction(addPrioLowAction);
    prioMenu->addAction(addPrioDefaultAction);
    prioMenu->addAction(addPrioCustomAction);
    addWithPriorityAction->setMenu(prioMenu);
}
