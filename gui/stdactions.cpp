/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "widgets/mirrormenu.h"
#include "support/globalstatic.h"
#include <QCoreApplication>

GLOBAL_STATIC(StdActions, instance)

static void setToolTip(Action *act, const QString &tt)
{
    act->setToolTip(tt);
    act->setProperty(Action::constTtForSettings, true);
}

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
    savePlayQueueAction = ActionCollection::get()->createAction("saveplayqueue", i18n("Save As"), HIDE_MENU_ICON(Icons::self()->savePlayQueueIcon));
    appendToPlayQueueAction = ActionCollection::get()->createAction("appendtoplayqueue", i18n("Append"), Icons::self()->appendToPlayQueueIcon);
    setToolTip(appendToPlayQueueAction, i18n("Append To Play Queue"));
    appendToPlayQueueAndPlayAction = ActionCollection::get()->createAction("appendtoplayqueueandplay", i18n("Append And Play"));
    addToPlayQueueAndPlayAction = ActionCollection::get()->createAction("addtoplayqueueandplay", i18n("Add And Play"));
    setToolTip(appendToPlayQueueAndPlayAction, i18n("Append To Play Queue And Play"));
    insertAfterCurrentAction = ActionCollection::get()->createAction("insertintoplayqueue", i18n("Insert After Current"));
    addRandomAlbumToPlayQueueAction = ActionCollection::get()->createAction("addrandomalbumtoplayqueue", i18n("Append Random Album"));
    replacePlayQueueAction = ActionCollection::get()->createAction("replaceplayqueue", i18n("Play Now (And Replace Play Queue)"), Icons::self()->replacePlayQueueIcon);
    savePlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_S);
    appendToPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_P);
    replacePlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_R);

    addWithPriorityAction = new Action(Icon("favorites"), i18n("Add With Priority"), 0);
    addPrioHighestAction = new Action(i18n("Highest Priority (255)"), 0);
    addPrioHighAction = new Action(i18n("High Priority (200)"), 0);
    addPrioMediumAction = new Action(i18n("Medium Priority (125)"), 0);
    addPrioLowAction = new Action(i18n("Low Priority (50)"), 0);
    addPrioDefaultAction = new Action(i18n("Default Priority (0)"), 0);
    addPrioCustomAction = new Action(i18n("Custom Priority..."), 0);
    addToStoredPlaylistAction = new Action(Icons::self()->playlistFileIcon, i18n("Add To Playlist"), 0);
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
    searchAction = ActionCollection::get()->createAction("search", i18n("Find"), HIDE_MENU_ICON(Icons::self()->searchIcon));
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

    QMenu *addMenu=new QMenu();
    addToPlayQueueMenuAction = new Action(i18n("Add To Play Queue"), 0);
    addMenu->addAction(appendToPlayQueueAction);
    addMenu->addAction(appendToPlayQueueAndPlayAction);
    addMenu->addAction(addToPlayQueueAndPlayAction);
    addMenu->addAction(addWithPriorityAction);
    addMenu->addAction(insertAfterCurrentAction);
    addMenu->addAction(addRandomAlbumToPlayQueueAction);
    addToPlayQueueMenuAction->setMenu(addMenu);
    addRandomAlbumToPlayQueueAction->setVisible(false);
}

void StdActions::enableAddToPlayQueue(bool en)
{
    appendToPlayQueueAction->setEnabled(en);
    appendToPlayQueueAndPlayAction->setEnabled(en);
    addToPlayQueueAndPlayAction->setEnabled(en);
    insertAfterCurrentAction->setEnabled(en);
    replacePlayQueueAction->setEnabled(en);
    addWithPriorityAction->setEnabled(en);
    addToStoredPlaylistAction->setEnabled(en);
    addPrioHighestAction->setEnabled(en);
    addPrioHighAction->setEnabled(en);
    addPrioMediumAction->setEnabled(en);
    addPrioLowAction->setEnabled(en);
    addPrioDefaultAction->setEnabled(en);
    addPrioCustomAction->setEnabled(en);
    addToPlayQueueMenuAction->setEnabled(en);
}
