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

#include "onlineservicespage.h"
#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "onlineservicesmodel.h"
#include "jamendoservice.h"
#include "magnatuneservice.h"
#include "soundcloudservice.h"
#include "settings.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "mainwindow.h"
#include "stdactions.h"
#include "actioncollection.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#endif

OnlineServicesPage::OnlineServicesPage(QWidget *p)
    : QWidget(p)
    , onlineSearchRequest(false)
{
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    downloadAction = ActionCollection::get()->createAction("downloadtolibrary", i18n("Download To Library"), "go-down");

    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(OnlineServicesModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(OnlineServicesModel::self(), SIGNAL(updated(QModelIndex)), this, SLOT(updated(QModelIndex)));
    connect(OnlineServicesModel::self(), SIGNAL(busy(bool)), view, SLOT(showSpinner(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(searchIsActive(bool)), this, SLOT(controlSearch(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    connect(OnlineServicesModel::self()->configureAct(), SIGNAL(triggered()), this, SLOT(configureService()));
    connect(OnlineServicesModel::self()->refreshAct(), SIGNAL(triggered()), this, SLOT(refreshService()));
    connect(downloadAction, SIGNAL(triggered()), this, SLOT(download()));

    QMenu *menu=new QMenu(this);
    menu->addAction(OnlineServicesModel::self()->configureAct());
    menu->addAction(OnlineServicesModel::self()->refreshAct());
    view->addAction(downloadAction);
    menuButton->setMenu(menu);
    proxy.setSourceModel(OnlineServicesModel::self());
    view->setModel(&proxy);
    view->setRootIsDecorated(true);
}

OnlineServicesPage::~OnlineServicesPage()
{
}

void OnlineServicesPage::setEnabled(bool e)
{
    OnlineServicesModel::self()->setEnabled(e);
    controlActions();
}

void OnlineServicesPage::clear()
{
    OnlineServicesModel::self()->clear();
    view->setLevel(0);
}

QString OnlineServicesPage::activeService() const
{
    OnlineService *srv=activeSrv();
    return srv ? srv->name() : QString();
}

OnlineService * OnlineServicesPage::activeSrv() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return 0;
    }

    QString udi;
    OnlineService *activeSrv=0;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex index = proxy.mapToSource(idx);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(index.internalPointer());

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            OnlineService *srv=static_cast<OnlineService *>(item);
            if (activeSrv) {
                return 0;
            }
            activeSrv=srv;
        }
    }

    return activeSrv;
}

QStringList OnlineServicesPage::selectedFiles() const
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

    return OnlineServicesModel::self()->filenames(mapped);
}

QList<Song> OnlineServicesPage::selectedSongs() const
{
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }
    qSort(selected);

    QString name;
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
    }

    return OnlineServicesModel::self()->songs(mapped);
}

void OnlineServicesPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    QStringList files=selectedFiles();

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace, priorty);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

void OnlineServicesPage::itemDoubleClicked(const QModelIndex &)
{
     const QModelIndexList selected = view->selectedIndexes();
     if (1!=selected.size()) {
         return; //doubleclick should only have one selected item
     }
     MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
     if (MusicLibraryItem::Type_Song==item->itemType()) {
         addSelectionToPlaylist();
     }
}

void OnlineServicesPage::controlSearch(bool on)
{
    if (on) {
        searchService=QString();
        const QModelIndexList selected = view->selectedIndexes();
        if (1==selected.count()) {
            MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
            if (MusicLibraryItem::Type_Root==item->itemType()) {
                OnlineService *srv=static_cast<OnlineService *>(item);
                if (srv->isSearchBased()) {
                    onlineSearchRequest=true;
                    searchService=srv->name();
                } else if (srv->isLoaded()) {
                    onlineSearchRequest=false;
                    searchService=srv->name();
                }
            }
        }

        if (searchService.isEmpty() && !selected.isEmpty()) {
            // Get service of first selected item...
            foreach (const QModelIndex &idx, selected) {
                const MusicLibraryItem *item = static_cast<const MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());
                while (item->parentItem()) {
                    item=item->parentItem();
                }
                const OnlineService *srv=static_cast<const OnlineService *>(item);
                if (srv->isSearchBased()) {
                    onlineSearchRequest=true;
                    searchService=srv->name();
                    break;
                } else if (srv->isLoaded()) {
                    onlineSearchRequest=false;
                    searchService=srv->name();
                    break;
                }
            }
        }

        if (searchService.isEmpty()) {
            // No items? Or no searchable service - so default to soundcloud...
            onlineSearchRequest=true;
            searchService=SoundCloudService::constName;
        }

        genreCombo->setEnabled(true);
        proxy.update(QString(), genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText(), searchService);
        if (searchService.isEmpty()) {
            view->setSearchVisible(false);
            view->setBackgroundImage(QIcon());
        } else {
            if (onlineSearchRequest) {
                genreCombo->setCurrentIndex(0);
                genreCombo->setEnabled(false);
                proxy.update(QString(), QString());
            }
            view->setSearchLabelText(i18nc("Search ServiceName:", "Search %1:").arg(searchService));
            view->setBackgroundImage(OnlineServicesModel::self()->service(searchService)->serviceIcon());
        }
    } else {
        OnlineService *srv=OnlineServicesModel::self()->service(searchService);
        if (srv && srv->isSearchBased()) {
            srv->setSearch(QString());
        }
        genreCombo->setEnabled(true);
        searchService=QString();
        proxy.update(QString(), genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText(), QString());
        view->setBackgroundImage(QIcon());
    }
}

void OnlineServicesPage::searchItems()
{
    QString text=view->searchText().trimmed();

    if (onlineSearchRequest) {
        if (view->isSearchActive()) {
            OnlineServicesModel::self()->setSearch(searchService, text);
        }
    } else {
        proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText(), view->isSearchActive() ? searchService : QString());
        if (proxy.enabled() && !text.isEmpty()) {
            view->expandAll();
        }
    }
}

void OnlineServicesPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool srvSelected=false;
    bool canDownload=false;
    QSet<QString> services;

    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());

        if (item) {
            if (MusicLibraryItem::Type_Root==item->itemType()) {
                srvSelected=true;
                services.insert(item->data());
            } else {
                while (item->parentItem()) {
                    item=item->parentItem();
                }
                if (item && MusicLibraryItem::Type_Root==item->itemType()) {
                    services.insert(item->data());
                    if (static_cast<OnlineService *>(item)->canDownload()) {
                        canDownload=true;
                    }
                }
            }

            if (srvSelected && canDownload && services.count()>1) {
                break;
            }
        }
    }

    OnlineServicesModel::self()->configureAct()->setEnabled(srvSelected && 1==selected.count());
    OnlineServicesModel::self()->refreshAct()->setEnabled(srvSelected && 1==selected.count());
    downloadAction->setEnabled(!srvSelected && canDownload && !selected.isEmpty() && 1==services.count());
    StdActions::self()->addToPlayQueueAction->setEnabled(!srvSelected && !selected.isEmpty());
    StdActions::self()->addWithPriorityAction->setEnabled(!srvSelected && !selected.isEmpty());
    StdActions::self()->replacePlayQueueAction->setEnabled(!srvSelected && !selected.isEmpty());
    StdActions::self()->addToStoredPlaylistAction->setEnabled(!srvSelected && !selected.isEmpty());
    menuButton->controlState();
}

void OnlineServicesPage::configureService()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        static_cast<OnlineService *>(item)->configure(this);
    }
}

void OnlineServicesPage::refreshService()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        OnlineService *srv=static_cast<OnlineService *>(item);

        if (srv->isLoaded() && srv->childCount()>0 &&
                MessageBox::No==MessageBox::questionYesNo(this, i18n("Re-download music listing for %1?").arg(srv->name()), i18n("Re-download"),
                                                          GuiItem(i18n("Re-download")), StdGuiItem::cancel())) {
            return;
        }
        if (srv) {
            srv->reload(0==srv->childCount());
        }
    }
}

void OnlineServicesPage::updateGenres(const QModelIndex &idx)
{
    if (idx.isValid()) {
        QModelIndex m=proxy.mapToSource(idx);
        if (m.isValid()) {
            MusicLibraryItem *item=static_cast<MusicLibraryItem *>(m.internalPointer());
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
    genreCombo->update(OnlineServicesModel::self()->genres());
}

void OnlineServicesPage::download()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        QModelIndex idx = view->selectedIndexes().at(0);
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());

        while (item && item->parentItem()) {
            item=item->parentItem();
        }
        if (item) {
            emit addToDevice(OnlineServicesModel::constUdiPrefix+item->data(), QString(), songs);
        }
    }
}

void OnlineServicesPage::updated(const QModelIndex &idx)
{
    view->setExpanded(proxy.mapFromSource(idx));
}
