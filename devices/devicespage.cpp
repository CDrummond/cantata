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
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KGlobalSettings>
#include <KDE/KMessageBox>
#else
#include <QtGui/QAction>
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

    configureAction = p->actionCollection()->addAction("configuredevice");
    configureAction->setText(i18n("Configure Device"));
    configureAction->setIcon(QIcon::fromTheme("configure"));
    refreshAction = p->actionCollection()->addAction("refreshdevice");
    refreshAction->setText(i18n("Refresh Device"));
    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    copyAction = p->actionCollection()->addAction("copytolibrary");
    copyAction->setText(i18n("Copy To Library"));
    copyAction->setIcon(QIcon::fromTheme("document-import"));
    copyToLibraryButton->setDefaultAction(copyAction);
    syncAction = p->actionCollection()->addAction("syncdevice");
    syncAction->setText(i18n("Sync"));
    syncAction->setIcon(QIcon::fromTheme("folder-sync"));
    connect(syncAction, SIGNAL(triggered()), this, SLOT(sync()));
    #ifdef ENABLE_REMOTE_DEVICES
    forgetDeviceAction=p->actionCollection()->addAction("forgetdevice");
    forgetDeviceAction->setText(i18n("Forget Device"));
    forgetDeviceAction->setIcon(QIcon::fromTheme("list-remove"));
    toggleDeviceAction=p->actionCollection()->addAction("toggledevice");
    toggleDeviceAction->setText(i18n("Toggle Device"));
    toggleDeviceAction->setIcon(QIcon::fromTheme("network-connect"));
    connect(forgetDeviceAction, SIGNAL(triggered()), this, SLOT(forgetRemoteDevice()));
    connect(toggleDeviceAction, SIGNAL(triggered()), this, SLOT(toggleDevice()));
    #endif
    MainWindow::initButton(copyToLibraryButton);
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
    view->addAction(forgetDeviceAction);;
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
    MainWindow::initButton(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(configureAction);
    menu->addAction(refreshAction);
    menu->addSeparator();
    #ifdef ENABLE_REMOTE_DEVICES
    KAction *addRemote=p->actionCollection()->addAction("adddevice");
    addRemote->setText(i18n("Add Device"));
    addRemote->setIcon(QIcon::fromTheme("network-server"));
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
    menuButton->setIcon(QIcon::fromTheme("system-run"));
    proxy.setSourceModel(DevicesModel::self());
    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Devices"));
    #else
    view->setTopText(tr("Devices"));
    #endif
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
            if (Device::Ums!=dev->devType() && Device::Remote!=dev->devType()) {
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
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }

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
            if (Device::Ums!=dev->devType() && Device::Remote!=dev->devType()) {
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
    bool enable=false;
    bool onlyFs=true;
    bool singleUdi=true;
    bool connected=false;
    #ifdef ENABLE_REMOTE_DEVICES
    bool remoteDev=false;
    #endif
    QString udi;

    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            Device *dev=static_cast<Device *>(item);
            if (Device::Ums!=dev->devType() && Device::Remote!=dev->devType()) {
                onlyFs=false;
            }
            #ifdef ENABLE_REMOTE_DEVICES
            if (Device::Remote==dev->devType()) {
                remoteDev=true;
            }
            #endif
            if (udi.isEmpty()) {
                udi=dev->udi();
            } else if (udi!=dev->udi()) {
                singleUdi=false;
            }
            if (!enable) {
                enable=dev->childCount()>0;
            }
            connected=dev->isConnected();
        }
    }

    configureAction->setEnabled(!enable && 1==selected.count());
    refreshAction->setEnabled(!enable && 1==selected.count());
    copyAction->setEnabled(enable);
    syncAction->setEnabled(connected && 1==selected.count() && singleUdi);
    mw->deleteSongsAction->setEnabled(enable);
    mw->editTagsAction->setEnabled(enable && onlyFs && singleUdi);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(enable && onlyFs && singleUdi);
    #endif
    //mw->burnAction->setEnabled(enable && onlyFs);
    mw->organiseFilesAction->setEnabled(enable && onlyFs && singleUdi);
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
            switch (KMessageBox::questionYesNoCancel(this, i18n("<p>Which type of refresh do you wish to perform?<ul>"
                                                                "<li>Partial - Only new songs are scanned <i>(quick)</i></li>"
                                                                "<li>Full - All songs are rescanned <i>(slow)</i></li></ul></p>"),
                                                     i18n("Refresh"), KGuiItem(i18n("Partial")), KGuiItem(i18n("Full")))) {
                case KMessageBox::Yes:
                    full=false;
                case KMessageBox::No:
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
        if (KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
            emit deleteSongs(udi, songs);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_REMOTE_DEVICES
void DevicesPage::addRemoteDevice()
{
    RemoteDevicePropertiesDialog *dlg=new RemoteDevicePropertiesDialog(this);
    dlg->show("cover.jpg", Device::Options(), RemoteDevice::Details(), DevicePropertiesWidget::Prop_All-DevicePropertiesWidget::Prop_Folder, true);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const Device::Options &, const RemoteDevice::Details &)),
            DevicesModel::self(), SLOT(addRemoteDevice(const QString &, const Device::Options &, const RemoteDevice::Details &)));
}

void DevicesPage::forgetRemoteDevice()
{
    QString udi=activeFsDeviceUdi();
    if (!udi.isEmpty() && KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to forget the selected device?"))) {
        DevicesModel::self()->removeRemoteDevice(udi);
    }
}

void DevicesPage::toggleDevice()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType() && Device::Remote==static_cast<Device *>(item)->devType()) {
        static_cast<RemoteDevice *>(item)->toggle();
    }
}
#endif

#ifdef ENABLE_KDE_SUPPORT
#define DIALOG_ERROR KMessageBox::error(this, i18n("Action is not currently possible, due to other open dialogs.")); return
#else
#define DIALOG_ERROR QMessageBox::information(this, tr("Action is not currently possible, due to other open dialogs."), QMessageBox::Ok); return
#endif

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
    #ifdef ENABLE_KDE_SUPPORT
    entries.prepend(i18n("All Genres"));
    #else
    entries.prepend(tr("All Genres"));
    #endif

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
