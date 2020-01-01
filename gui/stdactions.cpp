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

#include "stdactions.h"
#include "widgets/icons.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "support/monoicon.h"
#include "models/playlistsmodel.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#endif
#include "support/icon.h"
#include "widgets/icons.h"
#include "support/utils.h"
#include "widgets/mirrormenu.h"
#include "support/globalstatic.h"
#include <QCoreApplication>

GLOBAL_STATIC(StdActions, instance)

static QMenu * priorityMenu(bool isSet, Action *parent)
{
    QString prefix(isSet ? "set" : "addwith");
    Action *prioHighestAction = ActionCollection::get()->createAction(prefix+"priohighest", QObject::tr("Highest Priority (255)"));
    Action *prioHighAction = ActionCollection::get()->createAction(prefix+"priohigh", QObject::tr("High Priority (200)"));
    Action *prioMediumAction = ActionCollection::get()->createAction(prefix+"priomedium", QObject::tr("Medium Priority (125)"));
    Action *prioLowAction = ActionCollection::get()->createAction(prefix+"priolow", QObject::tr("Low Priority (50)"));
    Action *prioDefaultAction = ActionCollection::get()->createAction(prefix+"priodefault", QObject::tr("Default Priority (0)"));
    Action *prioCustomAction = ActionCollection::get()->createAction(prefix+"priocustom", QObject::tr("Custom Priority..."));

    prioHighestAction->setSettingsText(parent);
    prioHighAction->setSettingsText(parent);
    prioMediumAction->setSettingsText(parent);
    prioLowAction->setSettingsText(parent);
    prioDefaultAction->setSettingsText(parent);
    prioCustomAction->setSettingsText(parent);

    prioHighAction->setData(200);
    prioMediumAction->setData(125);
    prioLowAction->setData(50);
    prioDefaultAction->setData(0);
    prioCustomAction->setData(-1);

    prioHighestAction->setData(255);
    prioHighAction->setData(200);
    prioMediumAction->setData(125);
    prioLowAction->setData(50);
    prioDefaultAction->setData(0);
    prioCustomAction->setData(-1);

    QMenu *prioMenu=new QMenu();
    prioMenu->addAction(prioHighestAction);
    prioMenu->addAction(prioHighAction);
    prioMenu->addAction(prioMediumAction);
    prioMenu->addAction(prioLowAction);
    prioMenu->addAction(prioDefaultAction);
    prioMenu->addAction(prioCustomAction);
    return prioMenu;
}

StdActions::StdActions()
{
    QColor col=Utils::monoIconColor();
    prevTrackAction = ActionCollection::get()->createAction("prevtrack", QObject::tr("Previous Track"), Icons::self()->toolbarPrevIcon);
    nextTrackAction = ActionCollection::get()->createAction("nexttrack", QObject::tr("Next Track"), Icons::self()->toolbarNextIcon);
    playPauseTrackAction = ActionCollection::get()->createAction("playpausetrack", QObject::tr("Play/Pause"), Icons::self()->toolbarPlayIcon);
    stopPlaybackAction = ActionCollection::get()->createAction("stopplayback", QObject::tr("Stop"), Icons::self()->toolbarStopIcon);
    stopAfterCurrentTrackAction = ActionCollection::get()->createAction("stopaftercurrenttrack", QObject::tr("Stop After Current Track"), Icons::self()->toolbarStopIcon);
    stopAfterTrackAction = ActionCollection::get()->createAction("stopaftertrack", QObject::tr("Stop After Track"), Icons::self()->toolbarStopIcon);
    increaseVolumeAction = ActionCollection::get()->createAction("increasevolume", QObject::tr("Increase Volume"));
    decreaseVolumeAction = ActionCollection::get()->createAction("decreasevolume", QObject::tr("Decrease Volume"));
    savePlayQueueAction = ActionCollection::get()->createAction("saveplayqueue", QObject::tr("Save As"), Icons::self()->savePlayQueueIcon);
    appendToPlayQueueAction = ActionCollection::get()->createAction("appendtoplayqueue", QObject::tr("Append"), Icons::self()->appendToPlayQueueIcon);
    appendToPlayQueueAction->setSettingsText(QObject::tr("Append To Play Queue"));
    appendToPlayQueueAndPlayAction = ActionCollection::get()->createAction("appendtoplayqueueandplay", QObject::tr("Append And Play"));
    addToPlayQueueAndPlayAction = ActionCollection::get()->createAction("addtoplayqueueandplay", QObject::tr("Add And Play"));
    appendToPlayQueueAndPlayAction->setSettingsText(QObject::tr("Append To Play Queue And Play"));
    insertAfterCurrentAction = ActionCollection::get()->createAction("insertintoplayqueue", QObject::tr("Insert After Current"));
    addRandomAlbumToPlayQueueAction = ActionCollection::get()->createAction("addrandomalbumtoplayqueue", QObject::tr("Append Random Album"));
    replacePlayQueueAction = ActionCollection::get()->createAction("replaceplayqueue", QObject::tr("Play Now (And Replace Play Queue)"), Icons::self()->replacePlayQueueIcon);
    savePlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_S);
    appendToPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_P);
    replacePlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_R);

    addWithPriorityAction = new Action(QObject::tr("Add With Priority"), nullptr);
    setPriorityAction = new Action(QObject::tr("Set Priority"), nullptr);
    setPriorityAction->setMenu(priorityMenu(true, setPriorityAction));
    addWithPriorityAction->setMenu(priorityMenu(false, addWithPriorityAction));

    addToStoredPlaylistAction = ActionCollection::get()->createAction("addtoplaylist", QObject::tr("Add To Playlist"), Icons::self()->playlistListIcon);
    #ifdef TAGLIB_FOUND
    organiseFilesAction = ActionCollection::get()->createAction("orgfiles", QObject::tr("Organize Files"), MonoIcon::icon(FontAwesome::folderopeno, col));
    editTagsAction = ActionCollection::get()->createAction("edittags", QObject::tr("Edit Track Information"));
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction = ActionCollection::get()->createAction("replaygain", QObject::tr("ReplayGain"), MonoIcon::icon(FontAwesome::barchart, col));
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = ActionCollection::get()->createAction("copytodevice", QObject::tr("Copy Songs To Device"), MonoIcon::icon(FontAwesome::mobile, col));
    copyToDeviceAction->setMenu(DevicesModel::self()->menu());
    deleteSongsAction = ActionCollection::get()->createAction("deletesongs", QObject::tr("Delete Songs"), MonoIcon::icon(FontAwesome::trash, MonoIcon::constRed));
    #endif
    setCoverAction = ActionCollection::get()->createAction("setimage", QObject::tr("Set Image"));
    removeAction = ActionCollection::get()->createAction("remove", QObject::tr("Remove"), Icons::self()->removeIcon);
    searchAction = ActionCollection::get()->createAction("search", QObject::tr("Find"), Icons::self()->searchIcon);
    searchAction->setShortcut(Qt::ControlModifier+Qt::Key_F);

    addToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());

    QMenu *addMenu=new QMenu();
    addToPlayQueueMenuAction = new Action(QObject::tr("Add To Play Queue"), nullptr);
    addMenu->addAction(appendToPlayQueueAction);
    addMenu->addAction(appendToPlayQueueAndPlayAction);
    addMenu->addAction(addToPlayQueueAndPlayAction);
    addMenu->addAction(addWithPriorityAction);
    addMenu->addAction(insertAfterCurrentAction);
    addMenu->addAction(addRandomAlbumToPlayQueueAction);
    addToPlayQueueMenuAction->setMenu(addMenu);
    addRandomAlbumToPlayQueueAction->setVisible(false);

    zoomInAction = ActionCollection::get()->createAction("zoomIn", QObject::tr("Zoom In"));
    zoomInAction->setShortcut(Qt::ControlModifier+Qt::Key_Plus);
    zoomOutAction = ActionCollection::get()->createAction("zoomOut", QObject::tr("Zoom Out"));
    zoomOutAction->setShortcut(Qt::ControlModifier+Qt::Key_Minus);
}

void StdActions::enableAddToPlayQueue(bool en)
{
    appendToPlayQueueAction->setEnabled(en);
    appendToPlayQueueAndPlayAction->setEnabled(en);
    addToPlayQueueAndPlayAction->setEnabled(en);
    insertAfterCurrentAction->setEnabled(en);
    replacePlayQueueAction->setEnabled(en);
    addToStoredPlaylistAction->setEnabled(en);
    addToPlayQueueMenuAction->setEnabled(en);
}
