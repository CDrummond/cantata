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

#include "devicespage.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "mainwindow.h"
#include "devicesmodel.h"
#include "settings.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "mainwindow.h"
#include "action.h"
#include "actioncollection.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#endif
#ifdef ENABLE_REMOTE_DEVICES
#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#endif
#include "syncdialog.h"
#ifdef ENABLE_REPLAYGAIN_SUPPORT
#include "rgdialog.h"
#endif
#include "tageditor.h"
#include "actiondialog.h"
#include "trackorganiser.h"

DevicesPage::DevicesPage(MainWindow *p)
    : QWidget(p)
    , mw(p)
{
    setupUi(this);

    configureAction = ActionCollection::get()->createAction("configuredevice", i18n("Configure Device"), Icons::configureIcon);
    refreshAction = ActionCollection::get()->createAction("refreshdevice", i18n("Refresh Device"), "view-refresh");
    copyAction = ActionCollection::get()->createAction("copytolibrary", i18n("Copy To Library"), "document-import");
    copyToLibraryButton->setDefaultAction(copyAction);
    syncAction = ActionCollection::get()->createAction("syncdevice", i18n("Sync"), "folder-sync");
    connect(syncAction, SIGNAL(triggered()), this, SLOT(sync()));
    #ifdef ENABLE_REMOTE_DEVICES
    forgetDeviceAction=ActionCollection::get()->createAction("forgetdevice", i18n("Forget Device"), "list-remove");
    toggleDeviceAction=ActionCollection::get()->createAction("toggledevice", i18n("Toggle Device"), "network-connect");
    connect(forgetDeviceAction, SIGNAL(triggered()), this, SLOT(forgetRemoteDevice()));
    connect(toggleDeviceAction, SIGNAL(triggered()), this, SLOT(toggleDevice()));
    #endif
    Icon::init(copyToLibraryButton);
    copyToLibraryButton->setEnabled(false);
    syncAction->setEnabled(false);
    view->addAction(copyAction);
    view->addAction(syncAction);
//     view->addAction(p->burnAction);
    view->addAction(p->organiseFilesAction);
    view->addAction(p->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(p->replaygainAction);
    #endif
    #ifdef ENABLE_REMOTE_DEVICES
    QAction *sepA=new QAction(this);
    sepA->setSeparator(true);
    view->addAction(sepA);
    view->addAction(forgetDeviceAction);
    #endif
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    view->addAction(sep);
    view->addAction(p->deleteSongsAction);
    connect(DevicesModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), this, SLOT(updateGenres(const QSet<QString> &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copyToLibrary()));
    connect(configureAction, SIGNAL(triggered()), this, SLOT(configureDevice()));
    connect(refreshAction, SIGNAL(triggered()), this, SLOT(refreshDevice()));
    Icon::init(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(configureAction);
    menu->addAction(refreshAction);
    menu->addSeparator();
    #ifdef ENABLE_REMOTE_DEVICES
    Action *addRemote=ActionCollection::get()->createAction("adddevice", i18n("Add Device"), "network-server");
    connect(addRemote, SIGNAL(triggered()), this, SLOT(addRemoteDevice()));
    menu->addAction(addRemote);
    menu->addAction(forgetDeviceAction);
    menu->addSeparator();
    #endif
//     menu->addAction(copyAction);
    menu->addAction(p->organiseFilesAction);
    menu->addAction(p->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    menu->addAction(p->replaygainAction);
    #endif
    //menu->addAction(sep);
    //menu->addAction(p->deleteSongsAction);
    menuButton->setMenu(menu);
    menuButton->setIcon(Icons::menuIcon);
    menuButton->setToolTip(i18n("Other Actions"));
    proxy.setSourceModel(DevicesModel::self());
    view->setTopText(i18n("Devices"));
    view->setModel(&proxy);
    #ifdef ENABLE_REMOTE_DEVICES
    view->init(configureAction, refreshAction, toggleDeviceAction, 0);
    #else
    view->init(configureAction, refreshAction, 0);
    #endif
    view->setRootIsDecorated(false);
    updateGenres(QSet<QString>());
}

DevicesPage::~DevicesPage()
{
}

void DevicesPage::clear()
{
    DevicesModel::self()->clear();
    view->setLevel(0);
}

QString DevicesPage::activeFsDeviceUdi() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QString();
    }

    QString udi;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (Device::Ums!=dev->devType() && Device::RemoteFs!=dev->devType()) {
                return QString();
            }
            if (!udi.isEmpty()) {
                return QString();
            }
            udi=dev->udi();
        }
    }

    return udi;
}

QList<Song> DevicesPage::selectedSongs() const
{
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }
    qSort(selected);

    // Ensure all songs are from UMS/Remote devices...
    QString udi;
    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        mapped.append(index);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (Device::Ums!=dev->devType() && Device::RemoteFs!=dev->devType()) {
                return QList<Song>();
            }
        }
    }

    return DevicesModel::self()->songs(mapped);
}

void DevicesPage::itemDoubleClicked(const QModelIndex &)
{
//     const QModelIndexList selected = view->selectedIndexes();
//     if (1!=selected.size()) {
//         return; //doubleclick should only have one selected item
//     }
//     MusicDevicesItem *item = static_cast<MusicDevicesItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
//     if (MusicDevicesItem::Type_Song==item->itemType()) {
//         addSelectionToPlaylist();
//     }
}

void DevicesPage::searchItems()
{
    proxy.update(view->searchText().trimmed(), genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled()) {
        view->expandAll();
    }
}

void DevicesPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool haveTracks=false;
    bool onlyFs=true;
    bool singleUdi=true;
    bool connected=false;
    #ifdef ENABLE_REMOTE_DEVICES
    bool remoteDev=false;
    #endif
    bool deviceSelected=false;
    bool busyDevice=false;
    QString udi;

    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            deviceSelected=true;
        }

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (!dev->isStdFs()) {
                onlyFs=false;
            }
            if (!dev->isIdle()) {
                busyDevice=true;
            }
            #ifdef ENABLE_REMOTE_DEVICES
            if (Device::RemoteFs==dev->devType()) {
                remoteDev=true;
            }
            #endif
            if (udi.isEmpty()) {
                udi=dev->udi();
            } else if (udi!=dev->udi()) {
                singleUdi=false;
            }
            if (!haveTracks) {
                haveTracks=dev->childCount()>0;
            }
            connected=dev->isConnected();
        }
    }

    configureAction->setEnabled(!busyDevice && 1==selected.count());
    refreshAction->setEnabled(!busyDevice && 1==selected.count());
    copyAction->setEnabled(!busyDevice && haveTracks && !deviceSelected);
    syncAction->setEnabled(!busyDevice && deviceSelected && connected && 1==selected.count() && singleUdi);
    mw->deleteSongsAction->setEnabled(!busyDevice && haveTracks && !deviceSelected);
    mw->editTagsAction->setEnabled(!busyDevice && haveTracks && onlyFs && singleUdi && !deviceSelected);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(!busyDevice && haveTracks && onlyFs && singleUdi && !deviceSelected);
    #endif
    //mw->burnAction->setEnabled(enable && onlyFs);
    mw->organiseFilesAction->setEnabled(!busyDevice && haveTracks && onlyFs && singleUdi && !deviceSelected);
    #ifdef ENABLE_REMOTE_DEVICES
    forgetDeviceAction->setEnabled(singleUdi && remoteDev);
    #endif
}

void DevicesPage::copyToLibrary()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(mapped.first().internalPointer());
    while (item->parentItem()) {
        item=item->parentItem();
    }
    QString udi;
    if (MusicLibraryItem::Type_Root==item->itemType()) {
        udi=static_cast<Device *>(item)->udi();
    }

    if (udi.isEmpty()) {
        return;
    }

    QList<Song> songs=DevicesModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        emit addToDevice(udi, QString(), songs);
        view->clearSelection();
    }
}

void DevicesPage::configureDevice()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        static_cast<Device *>(item)->configure(this);
    }
}

void DevicesPage::refreshDevice()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        Device *dev=static_cast<Device *>(item);
        bool full=true;
        if (dev->childCount() && Device::Mtp!=dev->devType()) {
            QString udi=dev->udi();
            switch (MessageBox::questionYesNoCancel(this, i18n("<p>Which type of refresh do you wish to perform?<ul>"
                                                               "<li>Partial - Only new songs are scanned <i>(quick)</i></li>"
                                                               "<li>Full - All songs are rescanned <i>(slow)</i></li></ul></p>"),
                                                    i18n("Refresh"), GuiItem(i18n("Partial")), GuiItem(i18n("Full")))) {
                case MessageBox::Yes:
                    full=false;
                case MessageBox::No:
                    break;
                default:
                    return;
            }
            // We have to query for device again, as it could have been unplugged whilst the above question dialog was visible...
            dev=DevicesModel::self()->device(udi);
        }

        if (dev) {
            dev->rescan(full);
        }
    }
}

void DevicesPage::deleteSongs()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(mapped.first().internalPointer());
    while (item->parentItem()) {
        item=item->parentItem();
    }
    QString udi;
    if (MusicLibraryItem::Type_Root==item->itemType()) {
        udi=static_cast<Device *>(item)->udi();
    }

    if (udi.isEmpty()) {
        return;
    }

    QList<Song> songs=DevicesModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
            emit deleteSongs(udi, songs);
        }
        view->clearSelection();
    }
}

void DevicesPage::addRemoteDevice()
{
    #ifdef ENABLE_REMOTE_DEVICES
    RemoteDevicePropertiesDialog *dlg=new RemoteDevicePropertiesDialog(this);
    dlg->show(DeviceOptions(QLatin1String("cover.jpg")), RemoteFsDevice::Details(), DevicePropertiesWidget::Prop_All-DevicePropertiesWidget::Prop_Folder, true);
    connect(dlg, SIGNAL(updatedSettings(const DeviceOptions &, RemoteFsDevice::Details)),
            DevicesModel::self(), SLOT(addRemoteDevice(const DeviceOptions &, RemoteFsDevice::Details)));
    #endif
}

void DevicesPage::forgetRemoteDevice()
{
    #ifdef ENABLE_REMOTE_DEVICES
    QString udi=activeFsDeviceUdi();
    if (!udi.isEmpty() && MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to forget the selected device?"))) {
        DevicesModel::self()->removeRemoteDevice(udi);
    }
    #endif
}

void DevicesPage::toggleDevice()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        static_cast<Device *>(item)->toggle();
    }
}

#define DIALOG_ERROR MessageBox::error(this, i18n("Action is not currently possible, due to other open dialogs.")); return

void DevicesPage::sync()
{
    if (0!=SyncDialog::instanceCount()) {
        return;
    }

    if (0!=TagEditor::instanceCount() || 0!=ActionDialog::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (0!=RgDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif

    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        SyncDialog *dlg=new SyncDialog(this);
        dlg->sync(static_cast<Device *>(item)->udi());
    }
}

void DevicesPage::updateGenres(const QSet<QString> &g)
{
    if (genreCombo->count() && g==genres) {
        return;
    }

    genres=g;
    QStringList entries=g.toList();
    qSort(entries);
    entries.prepend(i18n("All Genres"));

    QString currentFilter = genreCombo->currentIndex() ? genreCombo->currentText() : QString();

    genreCombo->clear();
    genreCombo->addItems(entries);
    if (0==genres.count()) {
        genreCombo->setCurrentIndex(0);
    } else {
        if (!currentFilter.isEmpty()) {
            bool found=false;
            for (int i=1; i<genreCombo->count() && !found; ++i) {
                if (genreCombo->itemText(i) == currentFilter) {
                    genreCombo->setCurrentIndex(i);
                    found=true;
                }
            }
            if (!found) {
                genreCombo->setCurrentIndex(0);
            }
        }
    }
}
