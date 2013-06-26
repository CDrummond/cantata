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

#include "stdactions.h"
#include "localize.h"
#include "action.h"
#include "actioncollection.h"
#include "playlistsmodel.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#endif
#include "icon.h"
#include "icons.h"
#include <QMenu>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(StdActions, instance)
#endif

StdActions * StdActions::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static StdActions *instance=0;
    if(!instance) {
        instance=new StdActions;
    }
    return instance;
    #endif
}

StdActions::StdActions()
{
    savePlayQueueAction = ActionCollection::get()->createAction("saveplaylist", i18n("Save As"), "document-save-as");
    addToPlayQueueAction = ActionCollection::get()->createAction("addtoplaylist", i18n("Add To Play Queue"), "list-add");
    replacePlayQueueAction = ActionCollection::get()->createAction("replaceplaylist", i18n("Replace Play Queue"), "media-playback-start");
    addWithPriorityAction = ActionCollection::get()->createAction("addwithprio", i18n("Add With Priority"), Icon("favorites"));
    addPrioHighestAction = new Action(i18n("Highest Priority (255)"), 0);
    addPrioHighAction = new Action(i18n("High Priority (200)"), 0);
    addPrioMediumAction = new Action(i18n("Medium Priority (125)"), 0);
    addPrioLowAction = new Action(i18n("Low Priority (50)"), 0);
    addPrioDefaultAction = new Action(i18n("Default Priority (0)"), 0);
    addPrioCustomAction = new Action(i18n("Custom Priority..."), 0);
    addToStoredPlaylistAction = ActionCollection::get()->createAction("addtostoredplaylist", i18n("Add To Playlist"), Icons::self()->playlistIcon);
    #ifdef TAGLIB_FOUND
    organiseFilesAction = ActionCollection::get()->createAction("organizefiles", i18n("Organize Files"), "inode-directory");
    editTagsAction = ActionCollection::get()->createAction("edittags", i18n("Edit Tags"), "document-edit");
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction = ActionCollection::get()->createAction("replaygain", i18n("ReplayGain"), "audio-x-generic");
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = ActionCollection::get()->createAction("copytodevice", i18n("Copy Songs To Device"), "multimedia-player");
    copyToDeviceAction->setMenu(DevicesModel::self()->menu());
    deleteSongsAction = ActionCollection::get()->createAction("deletesongs", i18n("Delete Songs"), "edit-delete");
    #endif
    setCoverAction = ActionCollection::get()->createAction("setcover", i18n("Set Image"));
    refreshAction = ActionCollection::get()->createAction("refresh", i18n("Refresh Database"), "view-refresh");
    backAction = ActionCollection::get()->createAction("back", i18n("Back"), "go-previous");
    backAction->setShortcut(QKeySequence::Back);
    removeAction = ActionCollection::get()->createAction("removeitems", i18n("Remove"), "list-remove");
    searchAction = ActionCollection::get()->createAction("search", i18n("Search"), "edit-find");
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
