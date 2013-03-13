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

#include "devicespage.h"
#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "mainwindow.h"
#include "devicesmodel.h"
#include "settings.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "action.h"
#include "actioncollection.h"
#include "stdactions.h"
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
#include "preferencesdialog.h"
#include "coverdialog.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocddevice.h"
#include "albumdetailsdialog.h"
#include "cddbselectiondialog.h"
#endif

DevicesPage::DevicesPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    copyAction = ActionCollection::get()->createAction("copytolibrary", i18n("Copy To Library"), Icons::importIcon);
    copyToLibraryButton->setDefaultAction(copyAction);
    syncAction = ActionCollection::get()->createAction("syncdevice", i18n("Sync"), "folder-sync");
    connect(syncAction, SIGNAL(triggered()), this, SLOT(sync()));
    #ifdef ENABLE_REMOTE_DEVICES
    forgetDeviceAction=ActionCollection::get()->createAction("forgetdevice", i18n("Forget Device"), "list-remove");
    connect(forgetDeviceAction, SIGNAL(triggered()), this, SLOT(forgetRemoteDevice()));
    connect(DevicesModel::self()->connectAct(), SIGNAL(triggered()), this, SLOT(toggleDevice()));
    connect(DevicesModel::self()->disconnectAct(), SIGNAL(triggered()), this, SLOT(toggleDevice()));
    #endif
    copyToLibraryButton->setEnabled(false);
    syncAction->setEnabled(false);
    view->addAction(copyAction);
    view->addAction(syncAction);
    view->addAction(StdActions::self()->organiseFilesAction);
    view->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(StdActions::self()->replaygainAction);
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
    view->addAction(StdActions::self()->deleteSongsAction);
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(DevicesModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(DevicesModel::self(), SIGNAL(updated(QModelIndex)), this, SLOT(updated(QModelIndex)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copyToLibrary()));
    connect(DevicesModel::self()->configureAct(), SIGNAL(triggered()), this, SLOT(configureDevice()));
    connect(DevicesModel::self()->refreshAct(), SIGNAL(triggered()), this, SLOT(refreshDevice()));
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    connect(DevicesModel::self()->editAct(), SIGNAL(triggered()), this, SLOT(editDetails()));
    connect(DevicesModel::self(), SIGNAL(matches(const QString &, const QList<CdAlbum> &)),
            SLOT(cdMatches(const QString &, const QList<CdAlbum> &)));
    #endif
    QMenu *menu=new QMenu(this);
    #ifdef ENABLE_REMOTE_DEVICES
    Action *addRemote=ActionCollection::get()->createAction("adddevice", i18n("Add Device"), "network-server");
    connect(addRemote, SIGNAL(triggered()), this, SLOT(addRemoteDevice()));
    menu->addAction(addRemote);
    menu->addAction(forgetDeviceAction);
    menu->addSeparator();
    #endif
    menu->addAction(DevicesModel::self()->configureAct());
    menu->addAction(DevicesModel::self()->refreshAct());
    menu->addSeparator();
    menu->addAction(StdActions::self()->organiseFilesAction);
    menu->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    menu->addAction(StdActions::self()->replaygainAction);
    #endif
    menuButton->setMenu(menu);
    proxy.setSourceModel(DevicesModel::self());
    view->setTopText(i18n("Devices"));
    view->setModel(&proxy);
    view->setRootIsDecorated(false);
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
    Device *dev=activeFsDevice();
    return dev ? dev->udi() : QString();
}

Device * DevicesPage::activeFsDevice() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return 0;
    }

    QString udi;
    Device *activeDev=0;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        MusicLibraryItem *item=DevicesModel::self()->toItem(index);

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (Device::Ums!=dev->devType() && Device::RemoteFs!=dev->devType()) {
                return 0;
            }
            if (activeDev) {
                return 0;
            }
            activeDev=dev;
        }
    }

    return activeDev;
}

QStringList DevicesPage::playableUrls() const
{
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QStringList();
    }
    qSort(selected);

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return DevicesModel::self()->playableUrls(mapped);
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
        MusicLibraryItem *item=DevicesModel::self()->toItem(index);

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

void DevicesPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    QStringList files=playableUrls();

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace, priorty);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

void DevicesPage::itemDoubleClicked(const QModelIndex &)
{
//     const QModelIndexList selected = view->selectedIndexes();
//     if (1!=selected.size()) {
//         return; //doubleclick should only have one selected item
//     }
//     MusicDevicesItem *item = DevicesModel::self()->toItem(proxy.mapToSource(selected.at(0)));
//     if (MusicDevicesItem::Type_Song==item->itemType()) {
//         addSelectionToPlaylist();
//     }
}

void DevicesPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled() && !text.isEmpty()) {
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
    bool audioCd=false;
    bool canPlay=false;
    QString udi;

    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=DevicesModel::self()->toItem(proxy.mapToSource(idx));

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
            if (Device::AudioCd==dev->devType()) {
                audioCd=true;
            }
            #ifdef ENABLE_REMOTE_DEVICES
            else if (Device::RemoteFs==dev->devType()) {
                remoteDev=true;
            }
            canPlay=dev->canPlaySongs();
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

    DevicesModel::self()->configureAct()->setEnabled(!busyDevice && 1==selected.count());
    DevicesModel::self()->refreshAct()->setEnabled(!busyDevice && 1==selected.count());
    copyAction->setEnabled(!busyDevice && haveTracks && (!deviceSelected || audioCd));
    syncAction->setEnabled(!audioCd && !busyDevice && deviceSelected && connected && 1==selected.count() && singleUdi);
    StdActions::self()->deleteSongsAction->setEnabled(!audioCd && !busyDevice && haveTracks && !deviceSelected);
    StdActions::self()->editTagsAction->setEnabled(!busyDevice && haveTracks && onlyFs && singleUdi && !deviceSelected);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(!busyDevice && haveTracks && onlyFs && singleUdi && !deviceSelected);
    #endif
    //StdActions::self()->burnAction->setEnabled(enable && onlyFs);
    StdActions::self()->organiseFilesAction->setEnabled(!busyDevice && haveTracks && onlyFs && singleUdi && !deviceSelected);
    StdActions::self()->addToPlayQueueAction->setEnabled(canPlay && !selected.isEmpty() && singleUdi && !busyDevice && haveTracks && (audioCd || !deviceSelected));
    StdActions::self()->addWithPriorityAction->setEnabled(StdActions::self()->addToPlayQueueAction->isEnabled());
    StdActions::self()->replacePlayQueueAction->setEnabled(StdActions::self()->addToPlayQueueAction->isEnabled());
    #ifdef ENABLE_REMOTE_DEVICES
    forgetDeviceAction->setEnabled(singleUdi && remoteDev);
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    DevicesModel::self()->editAct()->setEnabled(!AlbumDetailsDialog::instanceCount() && !busyDevice && 1==selected.count() && audioCd && haveTracks && deviceSelected);
    #endif
    menuButton->controlState();
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

    MusicLibraryItem *item=DevicesModel::self()->toItem(mapped.first());
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

    MusicLibraryItem *item=DevicesModel::self()->toItem(proxy.mapToSource(selected.first()));

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

    MusicLibraryItem *item=DevicesModel::self()->toItem(proxy.mapToSource(selected.first()));

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        Device *dev=static_cast<Device *>(item);
        bool full=true;

        if (Device::AudioCd==dev->devType()) {
            if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Lookup album and track details via %1?")
                                                          #if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
                                                          .arg(Settings::self()->useCddb() ? i18n("CDDB") : i18n("MusicBrainz"))
                                                          #elif defined MUSICBRAINZ5_FOUND
                                                          .arg(i18n("MusicBrainz"))
                                                          #else
                                                          .arg(i18n("CDDB"))
                                                          #endif
                                                          )) {
                return;
            }
        } else {
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

    MusicLibraryItem *item=DevicesModel::self()->toItem(mapped.first());
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
    Device *dev=activeFsDevice();
    if (!dev) {
        return;
    }
    QString udi=dev->udi();
    QString devName=dev->data();
    if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to forget '%1'?").arg(devName))) {
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

    MusicLibraryItem *item=DevicesModel::self()->toItem(proxy.mapToSource(selected.first()));

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        if (static_cast<Device *>(item)->isConnected() &&
            MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to disconnect '%1'?").arg(static_cast<Device *>(item)->data()))) {
            return;
        }
        static_cast<Device *>(item)->toggle();
    }
}

#define DIALOG_ERROR MessageBox::error(this, i18n("Please close other dialogs first.")); return

void DevicesPage::sync()
{
    if (0!=SyncDialog::instanceCount()) {
        return;
    }

    if (0!=PreferencesDialog::instanceCount() || 0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()
        || 0!=ActionDialog::instanceCount() || 0!=CoverDialog::instanceCount()
        #ifdef ENABLE_REPLAYGAIN_SUPPORT
        || 0!=RgDialog::instanceCount()
        #endif
        ) {
        DIALOG_ERROR;
    }

    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=DevicesModel::self()->toItem(selected.first());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        SyncDialog *dlg=new SyncDialog(this);
        dlg->sync(static_cast<Device *>(item)->udi());
    }
}

void DevicesPage::updateGenres(const QModelIndex &idx)
{
    if (idx.isValid()) {
        QModelIndex m=proxy.mapToSource(idx);
        if (m.isValid()) {
            MusicLibraryItem *item=DevicesModel::self()->toItem(m);
            MusicLibraryItem::Type itemType=item->itemType();
            if (itemType==MusicLibraryItem::Type_Root) {
                genreCombo->update(static_cast<MusicLibraryItemRoot *>(item)->genres());
                return;
            } else if (itemType==MusicLibraryItem::Type_Artist) {
                genreCombo->update(static_cast<MusicLibraryItemArtist *>(item)->genres());
                return;
            } else if (itemType==MusicLibraryItem::Type_Album) {
                genreCombo->update(static_cast<MusicLibraryItemAlbum *>(item)->genres());
                return;
            }
        }
    }
    genreCombo->update(DevicesModel::self()->genres());
}

void DevicesPage::updated(const QModelIndex &idx)
{
    view->setExpanded(proxy.mapFromSource(idx));
}

void DevicesPage::cdMatches(const QString &udi, const QList<CdAlbum> &albums)
{
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    int chosen=0;
    Device *dev=DevicesModel::self()->device(udi);
    if (dev && Device::AudioCd==dev->devType()) {
        if (Device::AudioCd==dev->devType()) {
            CddbSelectionDialog *dlg=new CddbSelectionDialog(this);
            chosen=dlg->select(albums);
            if (chosen<0 || chosen>=albums.count()) {
                chosen=0;
            }
        }
    }

    // Need to get device again, as it may have been removed!
    dev=DevicesModel::self()->device(udi);
    if (dev && Device::AudioCd==dev->devType()) {
        if (Device::AudioCd==dev->devType()) {
            static_cast<AudioCdDevice *>(dev)->setDetails(albums.at(chosen));
        }
    }
    #else
    Q_UNUSED(udi)
    Q_UNUSED(albums)
    #endif
}

void DevicesPage::editDetails()
{
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    if (AlbumDetailsDialog::instanceCount()) {
        return;
    }
    const QModelIndexList selected = view->selectedIndexes();
    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=DevicesModel::self()->toItem(proxy.mapToSource(selected.first()));

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        Device *dev=static_cast<Device *>(item);
        if (Device::AudioCd==dev->devType()) {
            AlbumDetailsDialog *dlg=new AlbumDetailsDialog(this);
            dlg->show(static_cast<AudioCdDevice *>(dev));
        }
    }
    #endif
}
