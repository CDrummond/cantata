/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "playlistspage.h"
#include "playlistsmodel.h"
#include "mpdconnection.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QInputDialog>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#include <KMessageBox>
#else
#include <QtGui/QAction>
#include <QtGui/QMessageBox>
#endif

// Play Queue - for some reason, expects the list in reverse order...
static QStringList reverseList(const QStringList &orig)
{
    QStringList rev;
    foreach (const QString &s, orig) {
        rev.prepend(s);
    }
    return rev;
}

PlaylistsPage::PlaylistsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    removeAction = p->actionCollection()->addAction("removeplaylist");
    removeAction->setText(i18n("Remove"));
    renamePlaylistAction = p->actionCollection()->addAction("renameplaylist");
    renamePlaylistAction->setText(i18n("Rename"));
    #else
    removeAction = new QAction(tr("Remove"), this);
    renamePlaylistAction = new QAction(tr("Rename"), this);
    #endif
    removeAction->setIcon(QIcon::fromTheme("list-remove"));
    renamePlaylistAction->setIcon(QIcon::fromTheme("edit-rename"));

    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    rem->setDefaultAction(removeAction);
    renPlaylist->setDefaultAction(renamePlaylistAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), rem, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), renPlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    rem->setAutoRaise(true);
    renPlaylist->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);
    rem->setEnabled(false);
    renPlaylist->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Playlists"));
#else
    view->setTopText(tr("Playlists"));
#endif
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(removeAction);
    view->addAction(renamePlaylistAction);
    view->setUniformRowHeights(true);
    view->setAcceptDrops(true);
    view->setDragDropOverwriteMode(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);

    proxy.setSourceModel(PlaylistsModel::self());
    view->setModel(&proxy);
    view->init(p->replacePlaylistAction, p->addToPlaylistAction);
    view->setDeleteAction(removeAction);

    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(selectionChanged()));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    //connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(this, SIGNAL(loadPlaylist(const QString &)), MPDConnection::self(), SLOT(loadPlaylist(const QString &)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(removePlaylist(const QString &)));
    connect(this, SIGNAL(savePlaylist(const QString &)), MPDConnection::self(), SLOT(savePlaylist(const QString &)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(renamePlaylist(const QString &, const QString &)));
    connect(this, SIGNAL(removeFromPlaylist(const QString &, const QList<int> &)), MPDConnection::self(), SLOT(removeFromPlaylist(const QString &, const QList<int> &)));
    connect(removeAction, SIGNAL(activated()), this, SLOT(removeItems()));
    connect(p->savePlaylistAction, SIGNAL(activated()), this, SLOT(savePlaylist()));
    connect(renamePlaylistAction, SIGNAL(triggered()), this, SLOT(renamePlaylist()));
}

PlaylistsPage::~PlaylistsPage()
{
}

void PlaylistsPage::refresh()
{
    PlaylistsModel::self()->getPlaylists();
}

void PlaylistsPage::clear()
{
    PlaylistsModel::self()->clear();
}

void PlaylistsPage::addSelectionToPlaylist()
{
    addItemsToPlayQueue(view->selectionModel()->selectedRows());
}

void PlaylistsPage::removeItems()
{
    QSet<QString> remPlaylists;
    QMap<QString, QList<int> > remSongs;

    const QModelIndexList selected = view->selectionModel()->selectedRows();

    foreach(const QModelIndex &index, selected) {
        if(index.isValid()) {
            QModelIndex realIndex(proxy.mapToSource(index));

            if(realIndex.isValid()) {
                PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(realIndex.internalPointer());
                if(item->isPlaylist()) {
                    PlaylistsModel::PlaylistItem *pl=static_cast<PlaylistsModel::PlaylistItem *>(item);
                    remPlaylists.insert(pl->name);
                    if (remSongs.contains(pl->name)) {
                        remSongs.remove(pl->name);
                    }
                } else {
                    PlaylistsModel::SongItem *song=static_cast<PlaylistsModel::SongItem *>(item);
                    remSongs[song->parent->name].append(song->parent->songs.indexOf(song));
                }
            }
        }
    }

    // Should not happen!
    if (remPlaylists.isEmpty() && remSongs.isEmpty()) {
        return;
    }

    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected items?"), i18n("Remove?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::warning(this, tr("Remove?"), tr("Are you sure you wish to remove the selected items?"),
                                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
        return;
    }
    #endif

    foreach (const QString &pl, remPlaylists) {
        emit removePlaylist(pl);
    }
    QMap<QString, QList<int> >::ConstIterator it=remSongs.constBegin();
    QMap<QString, QList<int> >::ConstIterator end=remSongs.constEnd();
    for (; it!=end; ++it) {
        emit removeFromPlaylist(it.key(), it.value());
    }
}

void PlaylistsPage::savePlaylist()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString name = QInputDialog::getText(this, i18n("Playlist Name"), i18n("Enter a name for the playlist:"));
    #else
    QString name = QInputDialog::getText(this, tr("Playlist Name"), tr("Enter a name for the playlist:"));
    #endif

    if (!name.isEmpty()) {
        if (PlaylistsModel::self()->exists(name)) {
            #ifdef ENABLE_KDE_SUPPORT
            if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("A playlist named <b>%1</b> already exists!<br/>Overwrite?").arg(name), i18n("Overwrite Playlist?"))) {
            #else
            if (QMessageBox::No==QMessageBox::warning(this, tr("Overwrite Playlist?"), tr("A playlist named <b>%1</b> already exists!<br/>Overwrite?").arg(name),
                                                      QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
            #endif
                return;
            }
            else {
                emit removePlaylist(name);
            }
        }
        emit savePlaylist(name);
    }
}

void PlaylistsPage::renamePlaylist()
{
    const QModelIndexList items = view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = proxy.mapToSource(items.first());
        QString name = PlaylistsModel::self()->data(sourceIndex, Qt::DisplayRole).toString();
        #ifdef ENABLE_KDE_SUPPORT
        QString newName = QInputDialog::getText(this, i18n("Rename Playlist"), i18n("Enter new name for playlist: %1").arg(name));
        #else
        QString newName = QInputDialog::getText(this, tr("Rename Playlist"), tr("Enter new name for playlist: %1").arg(name));
        #endif

        if (!newName.isEmpty()) {
            if (PlaylistsModel::self()->exists(newName)) {
                #ifdef ENABLE_KDE_SUPPORT
                if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("A playlist named <b>%1</b> already exists!<br/>Overwrite?").arg(newName), i18n("Overwrite Playlist?"))) {
                #else
                if (QMessageBox::No==QMessageBox::warning(this, tr("Overwrite Playlist?"), tr("A playlist named <b>%1</b> already exists!<br/>Overwrite?").arg(newName),
                                                          QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
                #endif
                    return;
                }
                else {
                    emit removePlaylist(newName);
                }
            }
            emit renamePlaylist(name, newName);
        }
    }
}

void PlaylistsPage::itemDoubleClicked(const QModelIndex &index)
{
    QModelIndexList indexes;
    indexes.append(index);
    addItemsToPlayQueue(indexes);
}

void PlaylistsPage::addItemsToPlayQueue(const QModelIndexList &indexes)
{
    QStringList filenames;

    // If we only have 1 item selected, see if it is a playlist. If so, we might be able to
    // jsut ask MPD to load it...
    if (1==indexes.count()) {
        QModelIndex idx=proxy.mapToSource(*(indexes.begin()));
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(idx.internalPointer());

        if (item->isPlaylist()) {
            bool loadable=true;
            foreach (const PlaylistsModel::SongItem *s, static_cast<PlaylistsModel::PlaylistItem*>(item)->songs) {
                if (s->file.startsWith("http:/")) {
                    loadable=false; // Can't just load playlist, might need to parse HTTP...
                }
                filenames << s->file;
            }

            if (loadable) {
                emit loadPlaylist(static_cast<PlaylistsModel::PlaylistItem*>(item)->name);;
            } else {
                emit add(reverseList(filenames));
            }
            return;
        }
    }

    QSet<PlaylistsModel::Item *> selectedPlaylists;
    foreach(QModelIndex index, indexes) {
        QModelIndex idx=proxy.mapToSource(index);
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(idx.internalPointer());

        if (item->isPlaylist()) {
            selectedPlaylists.insert(item);
            foreach (const PlaylistsModel::SongItem *s, static_cast<PlaylistsModel::PlaylistItem*>(item)->songs) {
                filenames << s->file;
            }
        } else if (!selectedPlaylists.contains(static_cast<PlaylistsModel::SongItem*>(item)->parent)) {
            filenames << static_cast<PlaylistsModel::SongItem*>(item)->file;
        }
    }

    emit add(reverseList(filenames));
}

void PlaylistsPage::selectionChanged()
{
    QModelIndexList selected=view->selectionModel()->selectedIndexes();
    bool enable=false;

    if (1==selected.count()) {
        QModelIndex index = proxy.mapToSource(selected.first());
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(index.internalPointer());
        if (item && item->isPlaylist()) {
            enable=true;
        }
    }
    renamePlaylistAction->setEnabled(enable);
    renPlaylist->setEnabled(enable);
}

void PlaylistsPage::searchItems()
{
    proxy.setFilterRegExp(view->searchText());
}
