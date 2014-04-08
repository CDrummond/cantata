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

#include "onlineservicespage.h"
#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitempodcast.h"
#include "onlineservicesmodel.h"
#include "jamendoservice.h"
#include "magnatuneservice.h"
#include "soundcloudservice.h"
#include "podcastservice.h"
#include "settings.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "mainwindow.h"
#include "stdactions.h"
#include "actioncollection.h"
#include "inputdialog.h"
#include "podcastsearchdialog.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#endif

OnlineServicesPage::OnlineServicesPage(QWidget *p)
    : QWidget(p)
    , onlineSearchRequest(false)
    , searchable(true)
    , expanded(false)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    downloadAction = ActionCollection::get()->createAction("downloadtolibrary", i18n("Download To Library"), "go-down");
    downloadPodcastAction = ActionCollection::get()->createAction("downloadpodcast", i18n("Download Podcast Episodes"), "go-down");
    deleteDownloadedPodcastAction = ActionCollection::get()->createAction("deletedownloadedpodcast", i18n("Delete Downloaded Podcast Episodes"), "edit-delete");
    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(OnlineServicesModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(OnlineServicesModel::self(), SIGNAL(updated(QModelIndex)), this, SLOT(updated(QModelIndex)));
//    connect(OnlineServicesModel::self(), SIGNAL(needToSort()), this, SLOT(sortList()));
    connect(OnlineServicesModel::self(), SIGNAL(busy(bool)), view, SLOT(showSpinner(bool)));
    connect(OnlineServicesModel::self(), SIGNAL(providersChanged()), this, SLOT(providersChanged()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(searchIsActive(bool)), this, SLOT(controlSearch(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(setSearchable(QModelIndex)));
    connect(OnlineServicesModel::self()->configureAct(), SIGNAL(triggered()), this, SLOT(configureService()));
    connect(OnlineServicesModel::self()->refreshAct(), SIGNAL(triggered()), this, SLOT(refreshService()));
    connect(OnlineServicesModel::self()->subscribeAct(), SIGNAL(triggered()), this, SLOT(subscribe()));
    connect(OnlineServicesModel::self()->unSubscribeAct(), SIGNAL(triggered()), this, SLOT(unSubscribe()));
    connect(OnlineServicesModel::self()->refreshSubscriptionAct(), SIGNAL(triggered()), this, SLOT(refreshSubscription()));
    connect(downloadAction, SIGNAL(triggered()), this, SLOT(download()));
    connect(downloadPodcastAction, SIGNAL(triggered()), this, SLOT(downloadPodcast()));
    connect(deleteDownloadedPodcastAction, SIGNAL(triggered()), this, SLOT(deleteDownloadedPodcast()));

    QMenu *menu=new QMenu(this);
    menu->addAction(OnlineServicesModel::self()->configureAct());
    menu->addAction(OnlineServicesModel::self()->refreshAct());
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    menu->addAction(sep);
    menu->addAction(OnlineServicesModel::self()->subscribeAct());
    menu->addAction(OnlineServicesModel::self()->unSubscribeAct());
    menu->addAction(OnlineServicesModel::self()->refreshSubscriptionAct());
    menu->addAction(downloadPodcastAction);
    menu->addAction(deleteDownloadedPodcastAction);
    menu->addSeparator();
    QAction *configAction=new QAction(Icons::self()->configureIcon, i18n("Configure..."), this);
    menu->addAction(configAction);
    connect(configAction, SIGNAL(triggered(bool)), this, SLOT(showPreferencesPage()));

    view->addAction(downloadAction);
    view->addAction(sep);
    view->addAction(OnlineServicesModel::self()->subscribeAct());
    view->addAction(OnlineServicesModel::self()->unSubscribeAct());
    view->addAction(OnlineServicesModel::self()->refreshSubscriptionAct());
    view->addAction(downloadPodcastAction);
    view->addAction(deleteDownloadedPodcastAction);
    menuButton->setMenu(menu);
    proxy.setSourceModel(OnlineServicesModel::self());
//    proxy.setDynamicSortFilter(false);
    view->setModel(&proxy);
    view->setRootIsDecorated(true);
    view->setSearchResetLevel(1);
}

OnlineServicesPage::~OnlineServicesPage()
{
}

void OnlineServicesPage::setEnabled(bool e)
{
    OnlineServicesModel::self()->setEnabled(e);
    controlActions();
//    if (e) {
//        proxy.sort();
//    }
}

void OnlineServicesPage::showEvent(QShowEvent *e)
{
    view->focusView();
    if (!expanded && OnlineServicesModel::self()->isEnabled()) {
        expanded=true;
        expandPodcasts();
    }
    QWidget::showEvent(e);
}

void OnlineServicesPage::clear()
{
    OnlineServicesModel::self()->clear();
    view->goToTop();
}

OnlineService * OnlineServicesPage::activeSrv() const
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (selected.isEmpty()) {
        return 0;
    }

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
    if (selected.isEmpty()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return OnlineServicesModel::self()->filenames(mapped);
}

QList<Song> OnlineServicesPage::selectedSongs(bool allowPlaylists) const
{
    Q_UNUSED(allowPlaylists)
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }

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

void OnlineServicesPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty, bool randomAlbums)
{
    Q_UNUSED(randomAlbums)
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

void OnlineServicesPage::refresh()
{
    OnlineServicesModel::self()->resetModel();
    view->goToTop();
    expandPodcasts();
}

void OnlineServicesPage::itemDoubleClicked(const QModelIndex &)
{
     const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
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
    // Can only search when we are at top level...
    if (on && !searchable) {
        view->setSearchVisible(false);
        return;
    }

    QString prevSearchService=searchService;

    searchService=QString();
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1==selected.count()) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
        if (MusicLibraryItem::Type_Root==item->itemType()) {
            OnlineService *srv=static_cast<OnlineService *>(item);
            if (srv->isSearchBased()) {
                onlineSearchRequest=true;
                searchService=srv->id();
            } else if (srv->isLoaded()) {
                onlineSearchRequest=false;
                searchService=srv->id();
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
                searchService=srv->id();
                break;
            } else if (srv->isLoaded()) {
                onlineSearchRequest=false;
                searchService=srv->id();
                break;
            }
        }
    }

    if (on && searchService.isEmpty()) {
        // No items? Or no searchable service - so default to soundcloud...
        onlineSearchRequest=true;
        searchService=SoundCloudService::constName;
    }

//    // Handle the case where search was visible, and user selected other search.
//    // In this case we recieve on=false (which is for the previous), so we fake this
//    // into an on=true for the new service...
//    if (!on && !prevSearchService.isEmpty() && searchService!=prevSearchService) {
//        on=true;
//        view->focusSearch();
//    }

    if (on) {
        genreCombo->setEnabled(true);
        OnlineService *srv=OnlineServicesModel::self()->service(searchService);
        if (searchService.isEmpty()) {
            view->setSearchVisible(false);
            view->setBackgroundImage(QIcon());
        } else {
            if (onlineSearchRequest) {
                genreCombo->setCurrentIndex(0);
                genreCombo->setEnabled(false);
            }
            view->setSearchLabelText(i18nc("Search ServiceName:", "Search %1:", searchService));
            view->setBackgroundImage(srv->icon());
        }
        QModelIndex filterIndex=srv ? OnlineServicesModel::self()->serviceIndex(srv) : QModelIndex();
        proxy.setFilterItem(srv);
        proxy.update(QString(), QString());
        view->setSearchIndex(filterIndex.isValid() ? proxy.mapFromSource(filterIndex) : QModelIndex());
        view->setSearchResetLevel(filterIndex.isValid() ? 0 : 1);
        if (filterIndex.isValid()) {
            view->expand(proxy.mapFromSource(filterIndex), true);
        }
    } else {
        OnlineService *srv=OnlineServicesModel::self()->service(prevSearchService);
        if (srv) {
            srv->cancelSearch();
        }
        genreCombo->setEnabled(true);
        searchService=QString();
        proxy.setFilterItem(0);
        proxy.update(QString(), QString());
        view->setBackgroundImage(QIcon());
        view->setSearchIndex(QModelIndex());
        view->setSearchResetLevel(1);
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
        if (!view->isSearchActive()) {
            proxy.setFilterItem(0);
        }
        proxy.update(view->isSearchActive() ? text : QString(), genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
        if (proxy.enabled() && !proxy.filterText().isEmpty()) {
            view->expandAll(proxy.filterItem() && view->isSearchActive()
                                ? proxy.mapFromSource(OnlineServicesModel::self()->serviceIndex(static_cast<const OnlineService *>(proxy.filterItem())))
                                : QModelIndex());
        }
    }
}

void OnlineServicesPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool srvSelected=false;
    bool canDownload=false;
    bool canConfigure=false;
    bool canSubscribe=false;
    bool canUnSubscribe=false;
    bool canRefresh=false;
    QSet<QString> services;

    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());

        if (item) {
            if (MusicLibraryItem::Type_Root==item->itemType()) {
                srvSelected=true;
                services.insert(item->data());
                canConfigure=static_cast<OnlineService *>(item)->canConfigure();
                canRefresh=static_cast<OnlineService *>(item)->canLoad();
                canSubscribe=static_cast<OnlineService *>(item)->canSubscribe();
            } else {
                while (item->parentItem()) {
                    item=item->parentItem();
                }
                if (item && MusicLibraryItem::Type_Root==item->itemType()) {
                    services.insert(item->data());
                    if (static_cast<OnlineService *>(item)->canDownload()) {
                        canDownload=true;
                    }
                    if (static_cast<OnlineService *>(item)->canSubscribe()) {
                        canUnSubscribe=true;
                    }
                }
            }

            if (srvSelected && canDownload && canUnSubscribe && services.count()>1) {
                break;
            }
        }
    }

    OnlineServicesModel::self()->configureAct()->setEnabled(canConfigure && 1==selected.count());
    OnlineServicesModel::self()->subscribeAct()->setVisible(canSubscribe || canUnSubscribe);
    OnlineServicesModel::self()->unSubscribeAct()->setVisible(canSubscribe || canUnSubscribe);
    OnlineServicesModel::self()->refreshSubscriptionAct()->setVisible(canSubscribe || canUnSubscribe);
    downloadPodcastAction->setEnabled(canUnSubscribe);
    downloadPodcastAction->setVisible(canUnSubscribe);
    deleteDownloadedPodcastAction->setEnabled(canUnSubscribe);
    deleteDownloadedPodcastAction->setVisible(canUnSubscribe);
    OnlineServicesModel::self()->subscribeAct()->setEnabled(canSubscribe && 1==selected.count());
    OnlineServicesModel::self()->unSubscribeAct()->setEnabled(canUnSubscribe && 1==selected.count());
    OnlineServicesModel::self()->refreshSubscriptionAct()->setEnabled((canUnSubscribe || canSubscribe) && 1==selected.count());
    OnlineServicesModel::self()->refreshAct()->setEnabled(canRefresh && 1==selected.count());
    downloadAction->setVisible(!srvSelected && canDownload && !selected.isEmpty() && 1==services.count());
    downloadAction->setEnabled(!srvSelected && canDownload && !selected.isEmpty() && 1==services.count());
    StdActions::self()->addToPlayQueueAction->setEnabled(!srvSelected && !selected.isEmpty());
    StdActions::self()->addWithPriorityAction->setEnabled(!srvSelected && !selected.isEmpty());
    StdActions::self()->replacePlayQueueAction->setEnabled(!srvSelected && !selected.isEmpty());
    StdActions::self()->addToStoredPlaylistAction->setEnabled(!srvSelected && !selected.isEmpty());
    menuButton->controlState();
}

void OnlineServicesPage::configureService()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
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
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        OnlineService *srv=static_cast<OnlineService *>(item);

        if (srv) {
            if (srv->isLoaded() && srv->childCount()>0 &&
                    MessageBox::No==MessageBox::questionYesNo(this, i18n("Re-download music listing for %1?", srv->id()), i18n("Re-download"),
                                                              GuiItem(i18n("Re-download")), StdGuiItem::cancel())) {
                return;
            }
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

void OnlineServicesPage::setSearchable(const QModelIndex &idx)
{
    searchable=!idx.isValid();
    if (!searchable && view->isSearchActive()) {
        view->setSearchVisible(false);
    }
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

void OnlineServicesPage::subscribe()
{
    if (0==PodcastSearchDialog::instanceCount()) {
        PodcastSearchDialog *dlg=new PodcastSearchDialog(this);
        dlg->show();
    }
}

void OnlineServicesPage::unSubscribe()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());
    if (MusicLibraryItem::Type_Podcast!=item->itemType()) {
        return;
    }

    MusicLibraryItem *p=item->parentItem();
    if (!p || MusicLibraryItem::Type_Root!=p->itemType()) {
        return;
    }

    PodcastService *srv=static_cast<PodcastService *>(p);
    if (!srv->canSubscribe()) {
        return;
    }

    if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Unsubscribe from <b>%1</b>?", item->data()))) {
        return;
    }

    srv->unSubscribe(item);
}

void OnlineServicesPage::refreshSubscription()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        PodcastService *srv=static_cast<PodcastService *>(item);
        if (!srv->canSubscribe()) {
            return;
        }
        if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Refresh all podcast listings?"))) {
            return;
        }
        srv->refreshSubscription(0);
        return;
    }

    if (MusicLibraryItem::Type_Podcast!=item->itemType()) {
        return;
    }

    MusicLibraryItem *p=item->parentItem();
    if (!p || MusicLibraryItem::Type_Root!=p->itemType()) {
        return;
    }

    PodcastService *srv=static_cast<PodcastService *>(p);
    if (!srv->canSubscribe()) {
        return;
    }

    if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Refresh episode listing from <b>%1</b>?", item->data()))) {
        return;
    }

    srv->refreshSubscription(item);
}

static QString format(const QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> > &urls)
{
    QString rv;
    QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> >::ConstIterator it(urls.constBegin());
    QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> >::ConstIterator end(urls.constEnd());

    if (1==urls.keys().count()) {
        for (; it!=end; ++it) {
            foreach (MusicLibraryItemPodcastEpisode *ep, it.value()) {
                rv+=QLatin1String("<li>")+ep->data()+QLatin1String("</li>");
            }
        }
    } else {
        for (; it!=end; ++it) {
            rv+=QLatin1String("<li>")+it.key()->data()+QLatin1String("<ul>");
            foreach (MusicLibraryItemPodcastEpisode *ep, it.value()) {
                rv+=QLatin1String("<li>")+ep->data()+QLatin1String("</li>");
            }
            rv+="</ul></li>";
        }
    }
    return rv;
}

void OnlineServicesPage::downloadPodcast()
{
    const QModelIndexList selected = view->selectedIndexes(true);
    if (selected.isEmpty()) {
        return;
    }

    QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> > urls;
    int count=0;
    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Podcast:
            foreach (MusicLibraryItem *c, static_cast<MusicLibraryItemPodcast *>(item)->childItems()) {
                MusicLibraryItemPodcastEpisode *episode=static_cast<MusicLibraryItemPodcastEpisode *>(c);
                if (episode->localPath().isEmpty() && !urls[static_cast<MusicLibraryItemPodcast *>(item)].contains(episode)) {
                    urls[static_cast<MusicLibraryItemPodcast *>(item)].append(episode);
                    count++;
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (MusicLibraryItem::Type_Podcast==item->parentItem()->itemType()) {
                MusicLibraryItemPodcastEpisode *episode=static_cast<MusicLibraryItemPodcastEpisode *>(item);
                if (episode->localPath().isEmpty() && !urls[static_cast<MusicLibraryItemPodcast *>(item->parentItem())].contains(episode)) {
                    urls[static_cast<MusicLibraryItemPodcast *>(item->parentItem())].append(episode);
                    count++;
                }
            }
        default:
            break;
        }
    }

    if (urls.isEmpty()) {
        MessageBox::information(this, i18n("All selected podcasts have already been downloaded!"));
    } else {
        QString question;
        if (1==count) {
            question=QLatin1String("<p>")+i18n("Do you wish to download the following podcast episode?")+
                     QLatin1String("<ul>")+format(urls)+QLatin1String("</ul></p>");
        } else if (count<15) {
            question=QLatin1String("<p>")+i18n("Do you wish to download the following podcast episodes?")+
                     QLatin1String("<ul>")+format(urls)+QLatin1String("</ul></p>");
        } else {
            question=i18n("Do you wish to download the selected podcast episodes?");
        }
        if (MessageBox::No==MessageBox::questionYesNo(this, question)) {
            return;
        }
        QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> >::ConstIterator it(urls.constBegin());
        QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            OnlineServicesModel::self()->downloadPodcasts(it.key(), it.value());
        }
    }
}

void OnlineServicesPage::deleteDownloadedPodcast()
{
    const QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...
    if (selected.isEmpty()) {
        return;
    }

    QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> > urls;
    int count=0;
    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Podcast:
            foreach (MusicLibraryItem *c, static_cast<MusicLibraryItemPodcast *>(item)->childItems()) {
                MusicLibraryItemPodcastEpisode *episode=static_cast<MusicLibraryItemPodcastEpisode *>(c);
                if (!episode->localPath().isEmpty() && !urls[static_cast<MusicLibraryItemPodcast *>(item)].contains(episode)) {
                    urls[static_cast<MusicLibraryItemPodcast *>(item)].append(episode);
                    count++;
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (MusicLibraryItem::Type_Podcast==item->parentItem()->itemType()) {
                MusicLibraryItemPodcastEpisode *episode=static_cast<MusicLibraryItemPodcastEpisode *>(item);
                if (!episode->localPath().isEmpty() && !urls[static_cast<MusicLibraryItemPodcast *>(item->parentItem())].contains(episode)) {
                    urls[static_cast<MusicLibraryItemPodcast *>(item->parentItem())].append(episode);
                    count++;
                }
            }
        default:
            break;
        }
    }

    if (urls.isEmpty()) {
        MessageBox::information(this, i18n("All selected downloaded podcast episodes have already been deleted!"));
    } else {
        QString question;
        if (1==count) {
            question=QLatin1String("<p>")+i18n("Do you wish to delete the downloaded file of the following podcast episode?")+
                     QLatin1String("<ul>")+format(urls)+QLatin1String("</ul></p>");
        } else if (count<15) {
            question=QLatin1String("<p>")+i18n("Do you wish to the delete downloaded files of the following podcast episodes?")+
                     QLatin1String("<ul>")+format(urls)+QLatin1String("</ul></p>");
        } else {
            question=i18n("Do you wish to the delete downloaded files of the selected podcast episodes?");
        }
        if (MessageBox::No==MessageBox::questionYesNo(this, question)) {
            return;
        }
        QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> >::ConstIterator it(urls.constBegin());
        QMap<MusicLibraryItemPodcast *, QList<MusicLibraryItemPodcastEpisode *> >::ConstIterator end(urls.constEnd());
        for (; it!=end; ++it) {
            OnlineServicesModel::self()->deleteDownloadedPodcasts(it.key(), it.value());
        }
    }
}

void OnlineServicesPage::showPreferencesPage()
{
    emit showPreferencesPage(QLatin1String("online"));
}

void OnlineServicesPage::providersChanged()
{
    view->setSearchIndex(QModelIndex());
    view->setSearchResetLevel(1);
    view->closeSearch();
}

void OnlineServicesPage::updated(const QModelIndex &idx)
{
//    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(idx.internalPointer());
//    if (MusicLibraryItem::Type_Root==item->itemType() && !static_cast<OnlineService *>(item)->isSearchBased()) {
//        proxy.sort();
//    }

    if (isVisible() || !idx.isValid() || PodcastService::constName!=static_cast<OnlineService *>(idx.internalPointer())->id()) {
        view->setExpanded(proxy.mapFromSource(idx));
    }
}

//void OnlineServicesPage::sortList()
//{
//    proxy.sort();
//}

void OnlineServicesPage::expandPodcasts()
{
    OnlineService *pod=OnlineServicesModel::self()->service(PodcastService::constName);
    if (!OnlineServicesModel::self()->isHidden(pod)) {
        bool wasAnimated=view->isAnimated();
        if (wasAnimated) {
            view->setAnimated(false);
        }
        view->expand(proxy.mapFromSource(pod->index()), true);
        if (wasAnimated) {
            view->setAnimated(true);
        }
    }
}
