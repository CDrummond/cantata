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
#include "mainwindow.h"
#include "onlineservicesmodel.h"
#include "jamendoservice.h"
#include "magnatuneservice.h"
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

OnlineServicesPage::OnlineServicesPage(MainWindow *p)
    : QWidget(p)
    , mw(p)
{
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    setupUi(this);
    addToPlayQueue->setDefaultAction(p->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(p->replacePlayQueueAction);
    view->addAction(p->addToPlayQueueAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addWithPriorityAction);
    view->addAction(p->addToStoredPlaylistAction);
    configureAction = ActionCollection::get()->createAction("configureonlineservice", i18n("Configure Online Service"), Icons::configureIcon);
    refreshAction = ActionCollection::get()->createAction("refreshonlineservice", i18n("Refresh Online Service"), "view-refresh");
    toggleAction = ActionCollection::get()->createAction("toggleonlineservice", i18n("Toggle Online Service"));
    addAction = ActionCollection::get()->createAction("addonlineservice", i18n("Add Online Service"), "view-add");
    removeAction = ActionCollection::get()->createAction("removeonlineservice", i18n("Remove Online Service"), "view-add");
    QMenu *addMenu=new QMenu(this);
    jamendoAction=addMenu->addAction(Icons::jamendoIcon, JamendoService::constName);
    magnatuneAction=addMenu->addAction(Icons::magnatuneIcon, MagnatuneService::constName);
    addAction->setMenu(addMenu);

    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(OnlineServicesModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    connect(configureAction, SIGNAL(triggered()), this, SLOT(configureService()));
    connect(removeAction, SIGNAL(triggered()), this, SLOT(removeService()));
    connect(refreshAction, SIGNAL(triggered()), this, SLOT(refreshService()));
    connect(toggleAction, SIGNAL(triggered()), this, SLOT(toggleService()));
    connect(jamendoAction, SIGNAL(triggered()), this, SLOT(addJamendo()));
    connect(magnatuneAction, SIGNAL(triggered()), this, SLOT(addMagnatune()));

    Icon::init(addToPlayQueue);
    Icon::init(replacePlayQueue);
    Icon::init(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(removeAction);
    menu->addAction(sep);
    menu->addAction(configureAction);
    menu->addAction(refreshAction);
    view->addAction(sep);
    view->addAction(removeAction);
    menuButton->setMenu(menu);
    menuButton->setIcon(Icons::menuIcon);
    menuButton->setToolTip(i18n("Other Actions"));
    proxy.setSourceModel(OnlineServicesModel::self());
    view->setTopText(i18n("Online Music"));
    view->setModel(&proxy);
    view->init(configureAction, refreshAction, toggleAction, 0);
    view->setRootIsDecorated(false);
}

OnlineServicesPage::~OnlineServicesPage()
{
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
//     const QModelIndexList selected = view->selectedIndexes();
//     if (1!=selected.size()) {
//         return; //doubleclick should only have one selected item
//     }
//     MusicOnlineServicesItem *item = static_cast<MusicOnlineServicesItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
//     if (MusicOnlineServicesItem::Type_Song==item->itemType()) {
//         addSelectionToPlaylist();
//     }
}

void OnlineServicesPage::searchItems()
{
    proxy.update(view->searchText().trimmed(), genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled()) {
        view->expandAll();
    }
}

void OnlineServicesPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool srvSelected=false;
    bool busySrv=false;
    QString name;

    foreach (const QModelIndex &idx, selected) {
        MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer());

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            srvSelected=true;
        }

        if (item && MusicLibraryItem::Type_Root!=item->itemType()) {
            while(item->parentItem()) {
                item=item->parentItem();
            }
        }

        if (item && MusicLibraryItem::Type_Root==item->itemType()) {
            OnlineService *srv=static_cast<OnlineService *>(item);
            if (name.isEmpty()) {
                name=srv->name();
            }
        }
    }

    removeAction->setEnabled(!busySrv && srvSelected && 1==selected.count());
    configureAction->setEnabled(!busySrv && srvSelected && 1==selected.count());
    refreshAction->setEnabled(!busySrv && srvSelected && 1==selected.count());
    mw->addToPlayQueueAction->setEnabled(!srvSelected && selected.count());
    mw->addWithPriorityAction->setEnabled(!srvSelected && selected.count());
    mw->replacePlayQueueAction->setEnabled(!srvSelected && selected.count());
    mw->addToStoredPlaylistAction->setEnabled(!srvSelected && selected.count());
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

        if (srv) {
            srv->reload();
        }
    }
}

void OnlineServicesPage::removeService()
{
}

void OnlineServicesPage::toggleService()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    MusicLibraryItem *item=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.first()).internalPointer());

    if (MusicLibraryItem::Type_Root==item->itemType()) {
        if (static_cast<OnlineService *>(item)->isLoaded() &&
            MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to unload '%1'?").arg(static_cast<OnlineService *>(item)->data()))) {
            return;
        }
        static_cast<OnlineService *>(item)->toggle();
    }
}

void OnlineServicesPage::updateGenres(const QModelIndex &idx)
{
    if (idx.isValid()) {
        QModelIndex m=proxy.mapToSource(idx);
        if (m.isValid()) {
            MusicLibraryItem::Type itemType=static_cast<MusicLibraryItem *>(m.internalPointer())->itemType();
            if (itemType==MusicLibraryItem::Type_Root) {
                genreCombo->update(static_cast<MusicLibraryItemRoot *>(m.internalPointer())->genres());
                return;
            } else if (itemType==MusicLibraryItem::Type_Artist) {
                genreCombo->update(static_cast<MusicLibraryItemArtist *>(m.internalPointer())->genres());
                return;
            } else if (itemType==MusicLibraryItem::Type_Album) {
                genreCombo->update(static_cast<MusicLibraryItemAlbum *>(m.internalPointer())->genres());
                return;
            }
        }
    }
    genreCombo->update(OnlineServicesModel::self()->genres());
}

void OnlineServicesPage::addJamendo()
{
    addService(JamendoService::constName);
}

void OnlineServicesPage::addMagnatune()
{
    addService(MagnatuneService::constName);
}

void OnlineServicesPage::addService(const QString &name)
{
    if (0!=OnlineServicesModel::self()->service(name)) {
        MessageBox::error(this, i18n("%1 service already created!").arg(name));
        return;
    }

    OnlineServicesModel::self()->createService(name);
}
