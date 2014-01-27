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

#include "playlistspage.h"
#include "playlistsmodel.h"
#include "mpdconnection.h"
#include "messagebox.h"
#include "inputdialog.h"
#include "localize.h"
#include "icons.h"
#include "stdactions.h"
#include "actioncollection.h"
#include "mpdconnection.h"
#include "settings.h"
#include <QMenu>
#ifndef ENABLE_KDE_SUPPORT
#include <QStyle>
#endif

static inline void setResizeMode(QHeaderView *hdr, int idx, QHeaderView::ResizeMode mode)
{
    #if QT_VERSION < 0x050000
    hdr->setResizeMode(idx, mode);
    #else
    hdr->setSectionResizeMode(idx, mode);
    #endif
}

static inline void setResizeMode(QHeaderView *hdr, QHeaderView::ResizeMode mode)
{
    #if QT_VERSION < 0x050000
    hdr->setResizeMode(mode);
    #else
    hdr->setSectionResizeMode(mode);
    #endif
}

PlaylistTableView::PlaylistTableView(QWidget *p)
    : TableView(p)
    , menu(0)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    setDragDropOverwriteMode(false);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDropIndicatorShown(true);
    setUniformRowHeights(true);
    setUseSimpleDelegate();
}

void PlaylistTableView::initHeader()
{
    if (!model()) {
        return;
    }

    QHeaderView *hdr=header();
    if (!menu) {
        QFont f(font());
        f.setBold(true);
        QFontMetrics fm(f);
        setResizeMode(hdr, QHeaderView::Interactive);
        hdr->setContextMenuPolicy(Qt::CustomContextMenu);
        hdr->resizeSection(PlaylistsModel::COL_YEAR, fm.width("99999"));
        setResizeMode(hdr, PlaylistsModel::COL_TITLE, QHeaderView::Stretch);
        setResizeMode(hdr, PlaylistsModel::COL_ARTIST, QHeaderView::Interactive);
        setResizeMode(hdr, PlaylistsModel::COL_ALBUM, QHeaderView::Interactive);
        setResizeMode(hdr, PlaylistsModel::COL_GENRE, QHeaderView::Interactive);
        setResizeMode(hdr, PlaylistsModel::COL_LENGTH, QHeaderView::ResizeToContents);
        setResizeMode(hdr, PlaylistsModel::COL_YEAR, QHeaderView::Fixed);
        hdr->setStretchLastSection(false);
        connect(hdr, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu()));
    }

    //Restore state
    QByteArray state=Settings::self()->playlistHeaderState();
    QList<int> hideAble;
    hideAble << PlaylistsModel::COL_YEAR << PlaylistsModel::COL_GENRE;

    //Restore
    if (state.isEmpty()) {
        hdr->setSectionHidden(PlaylistsModel::COL_YEAR, true);
        hdr->setSectionHidden(PlaylistsModel::COL_GENRE, true);
    } else {
        hdr->restoreState(state);

        foreach (int col, hideAble) {
            if (hdr->isSectionHidden(col) || 0==hdr->sectionSize(col)) {
                hdr->setSectionHidden(col, true);
            }
        }
    }

    if (!menu) {
        menu = new QMenu(this);

        foreach (int col, hideAble) {
            QAction *act=new QAction(PlaylistsModel::headerText(col), menu);
            act->setCheckable(true);
            act->setChecked(!hdr->isSectionHidden(col));
            menu->addAction(act);
            act->setData(col);
            connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleHeaderItem(bool)));
        }
    }
}

void PlaylistTableView::saveHeader()
{
    if (menu && model()) {
        Settings::self()->savePlaylistHeaderState(header()->saveState());
    }
}

void PlaylistTableView::showMenu()
{
    menu->exec(QCursor::pos());
}

void PlaylistTableView::toggleHeaderItem(bool visible)
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        int index=act->data().toInt();
        if (-1!=index) {
            header()->setSectionHidden(index, !visible);
        }
    }
}

PlaylistsPage::PlaylistsPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    renamePlaylistAction = ActionCollection::get()->createAction("renameplaylist", i18n("Rename"), "edit-rename");
    removeDuplicatesAction=new Action(i18n("Remove Duplicates"), this);
    removeDuplicatesAction->setEnabled(false);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(PlaylistsModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));

    view->allowGroupedView();
    view->allowTableView(new PlaylistTableView(this));
    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    view->addAction(renamePlaylistAction);
    view->addAction(StdActions::self()->removeAction);
    view->addAction(removeDuplicatesAction);
    view->setUniformRowHeights(true);
    view->setAcceptDrops(true);
    view->setDragDropOverwriteMode(false);
    view->setDragDropMode(QAbstractItemView::DragDrop);

    proxy.setSourceModel(PlaylistsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(StdActions::self()->removeAction);

    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    //connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(this, SIGNAL(loadPlaylist(const QString &, bool)), MPDConnection::self(), SLOT(loadPlaylist(const QString &, bool)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(removePlaylist(const QString &)));
    connect(this, SIGNAL(savePlaylist(const QString &)), MPDConnection::self(), SLOT(savePlaylist(const QString &)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(renamePlaylist(const QString &, const QString &)));
    connect(this, SIGNAL(removeFromPlaylist(const QString &, const QList<quint32> &)), MPDConnection::self(), SLOT(removeFromPlaylist(const QString &, const QList<quint32> &)));
    connect(StdActions::self()->savePlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(savePlaylist()));
    connect(renamePlaylistAction, SIGNAL(triggered(bool)), this, SLOT(renamePlaylist()));
    connect(removeDuplicatesAction, SIGNAL(triggered(bool)), this, SLOT(removeDuplicates()));
    connect(PlaylistsModel::self(), SIGNAL(updated(const QModelIndex &)), this, SLOT(updated(const QModelIndex &)));
    connect(PlaylistsModel::self(), SIGNAL(playlistRemoved(quint32)), view, SLOT(collectionRemoved(quint32)));
    QMenu *menu=new QMenu(this);
    menu->addAction(StdActions::self()->addToPlayQueueAction);
    menu->addAction(StdActions::self()->addWithPriorityAction);
    menu->addAction(StdActions::self()->removeAction);
    menu->addAction(renamePlaylistAction);
    menuButton->setMenu(menu);
}

PlaylistsPage::~PlaylistsPage()
{
}

void PlaylistsPage::showEvent(QShowEvent *e)
{
    view->focusView();
    QWidget::showEvent(e);
}

void PlaylistsPage::setStartClosed(bool sc)
{
    view->setStartClosed(sc);
}

bool PlaylistsPage::isStartClosed()
{
    return view->isStartClosed();
}

void PlaylistsPage::updateRows()
{
    view->updateRows();
}

void PlaylistsPage::refresh()
{
    view->setLevel(0);
    PlaylistsModel::self()->getPlaylists();
}

void PlaylistsPage::clear()
{
    PlaylistsModel::self()->clear();
}

//QStringList PlaylistsPage::selectedFiles() const
//{
//    QModelIndexList indexes=view->selectedIndexes();
//    if (indexes.isEmpty()) {
//        return QStringList();
//    }
//
//    QModelIndexList mapped;
//    foreach (const QModelIndex &idx, indexes) {
//        mapped.append(proxy.mapToSource(idx));
//    }
//
//    return PlaylistsModel::self()->filenames(mapped, true);
//}

void PlaylistsPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    addItemsToPlayList(view->selectedIndexes(), name, replace, priorty);
}

void PlaylistsPage::setView(int mode)
{
    //bool diff=view->viewMode()!=mode;
    PlaylistsModel::self()->setMultiColumn(ItemView::Mode_Table==mode);
    view->setMode((ItemView::Mode)mode);
    //if (diff) {
    //    clear();
    //    refresh();
    //}
}

void PlaylistsPage::removeItems()
{
    QSet<QString> remPlaylists;
    QMap<QString, QList<quint32> > remSongs;
    QModelIndexList selected = view->selectedIndexes();

    foreach(const QModelIndex &index, selected) {
        if(index.isValid()) {
            QModelIndex realIndex(proxy.mapToSource(index));

            if(realIndex.isValid()) {
                PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(realIndex.internalPointer());
                if (item->isPlaylist()) {
                    PlaylistsModel::PlaylistItem *pl=static_cast<PlaylistsModel::PlaylistItem *>(item);
                    if (!pl->isSmartPlaylist) {
                        remPlaylists.insert(pl->name);
                    }
                    if (remSongs.contains(pl->name)) {
                        remSongs.remove(pl->name);
                    }
                } else {
                    PlaylistsModel::SongItem *song=static_cast<PlaylistsModel::SongItem *>(item);
                    if (!song->parent->isSmartPlaylist && !remPlaylists.contains(song->parent->name)) {
                        remSongs[song->parent->name].append(song->parent->songs.indexOf(song));
                    }
                }
            }
        }
    }

    // Should not happen!
    if (remPlaylists.isEmpty() && remSongs.isEmpty()) {
        return;
    }

    if (remPlaylists.count() &&
        MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected playlists?\nThis cannot be undone."),
                                                 i18n("Remove Playlists"), StdGuiItem::remove(), StdGuiItem::cancel())) {
            return;
    }

    foreach (const QString &pl, remPlaylists) {
        emit removePlaylist(pl);
    }
    QMap<QString, QList<quint32> >::ConstIterator it=remSongs.constBegin();
    QMap<QString, QList<quint32> >::ConstIterator end=remSongs.constEnd();
    for (; it!=end; ++it) {
        emit removeFromPlaylist(it.key(), it.value());
    }
}

void PlaylistsPage::savePlaylist()
{
    QString name = InputDialog::getText(i18n("Playlist Name"), i18n("Enter a name for the playlist:"), QString(), 0, this);

    if (!name.isEmpty()) {
        if (PlaylistsModel::self()->exists(name)) {
            if (MessageBox::No==MessageBox::warningYesNo(this, i18n("A playlist named <b>%1</b> already exists!<br/>Overwrite?", name),
                                                         i18n("Overwrite Playlist"), StdGuiItem::overwrite(), StdGuiItem::cancel())) {
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
    const QModelIndexList items = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==items.size()) {
        QModelIndex index = proxy.mapToSource(items.first());
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(index.internalPointer());
        if (!item->isPlaylist()) {
            return;
        }
        QString name = static_cast<PlaylistsModel::PlaylistItem *>(item)->name;
        QString newName = InputDialog::getText(i18n("Rename Playlist"), i18n("Enter new name for playlist:"), name, 0, this);

        if (!newName.isEmpty() && name!=newName) {
            if (PlaylistsModel::self()->exists(newName)) {
                if (MessageBox::No==MessageBox::warningYesNo(this, i18n("A playlist named <b>%1</b> already exists!<br/>Overwrite?", newName),
                                                             i18n("Overwrite Playlist"), StdGuiItem::overwrite(), StdGuiItem::cancel())) {
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

void PlaylistsPage::removeDuplicates()
{
    const QModelIndexList items = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==items.size()) {
        QModelIndex index = proxy.mapToSource(items.first());
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(index.internalPointer());
        if (!item->isPlaylist()) {
            return;
        }

        PlaylistsModel::PlaylistItem *pl=static_cast<PlaylistsModel::PlaylistItem *>(item);
        QMap<QString, QList<Song> > map;
        for (int i=0; i<pl->songs.count(); ++i) {
            Song song=*(pl->songs.at(i));
            song.id=i;
            map[song.artistSong()].append(song);
        }

        QList<quint32> toRemove;
        foreach (const QString &key, map.keys()) {
            QList<Song> values=map.value(key);
            if (values.size()>1) {
                Song::sortViaType(values);
                for (int i=1; i<values.count(); ++i) {
                    toRemove.append(values.at(i).id);
                }
            }
        }
        if (!toRemove.isEmpty()) {
            emit removeFromPlaylist(pl->name, toRemove);
        }
    }
}

void PlaylistsPage::itemDoubleClicked(const QModelIndex &index)
{
    if (
        #ifdef ENABLE_KDE_SUPPORT
        KGlobalSettings::singleClick()
        #else
        style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, this)
        #endif
        || !static_cast<PlaylistsModel::Item *>(proxy.mapToSource(index).internalPointer())->isPlaylist()) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayList(indexes, QString(), false);
    }
}

void PlaylistsPage::addItemsToPlayList(const QModelIndexList &indexes, const QString &name, bool replace, quint8 priorty)
{
    if (indexes.isEmpty()) {
        return;
    }

    // If we only have 1 item selected, see if it is a playlist. If so, we might be able to
    // just ask MPD to load it...
    if (name.isEmpty() && 1==indexes.count() && 0==priorty) {
        QModelIndex idx=proxy.mapToSource(*(indexes.begin()));
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(idx.internalPointer());

        if (item->isPlaylist()) {
            emit loadPlaylist(static_cast<PlaylistsModel::PlaylistItem*>(item)->name, replace);
            return;
        }
    }

    QModelIndexList mapped;
    bool checkNames=!name.isEmpty();
    foreach (const QModelIndex &idx, indexes) {
        QModelIndex m=proxy.mapToSource(idx);
        if (checkNames) {
            PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(m.internalPointer());
            if ( (item->isPlaylist() && static_cast<PlaylistsModel::PlaylistItem *>(item)->name==name) ||
                 (!item->isPlaylist() && static_cast<PlaylistsModel::SongItem *>(item)->parent->name==name) ) {
                MessageBox::error(this, i18n("Cannot add songs from '%1' to '%2'", name, name));
                return;
            }
        }
        mapped.append(m);
    }

    QStringList files=PlaylistsModel::self()->filenames(mapped);
    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace, priorty);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
QList<Song> PlaylistsPage::selectedSongs() const
{
    QModelIndexList selected = view->selectedIndexes();

    if (selected.isEmpty()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return PlaylistsModel::self()->songs(mapped);
}

void PlaylistsPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();
    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}
#endif

void PlaylistsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enableActions=selected.count()>0;
    bool allSmartPlaylists=false;
    bool canRename=false;

    if (1==selected.count()) {
        QModelIndex index = proxy.mapToSource(selected.first());
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(index.internalPointer());
        if (item) {
            if (item->isPlaylist()) {
                if (static_cast<PlaylistsModel::PlaylistItem *>(item)->isSmartPlaylist) {
                    allSmartPlaylists=true;
                } else {
                    canRename=true;
                }
            } else if (static_cast<PlaylistsModel::SongItem *>(item)->parent->isSmartPlaylist) {
                allSmartPlaylists=true;
            }
        }
    } else if (selected.count()<=200) {
        allSmartPlaylists=true;
        foreach (const QModelIndex &index, selected) {
            PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(proxy.mapToSource(index).internalPointer());
            if (item && (item->isPlaylist() ? !static_cast<PlaylistsModel::PlaylistItem *>(item)->isSmartPlaylist
                                            : static_cast<PlaylistsModel::SongItem *>(item)->parent->isSmartPlaylist)) {
                allSmartPlaylists=false;
                break;
            }
        }
    }

    renamePlaylistAction->setEnabled(canRename);
    removeDuplicatesAction->setEnabled(enableActions && !allSmartPlaylists);
    StdActions::self()->removeAction->setEnabled(enableActions && !allSmartPlaylists);
    StdActions::self()->replacePlayQueueAction->setEnabled(enableActions);
    StdActions::self()->addToPlayQueueAction->setEnabled(enableActions);
    StdActions::self()->addWithPriorityAction->setEnabled(enableActions);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enableActions);
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->copyToDeviceAction->setEnabled(enableActions && !allSmartPlaylists);
    #endif
    menuButton->controlState();
}

void PlaylistsPage::searchItems()
{
    QString text=view->searchText().trimmed();
    bool updated=proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled() && !proxy.filterText().isEmpty()) {
        view->expandAll();
    }
    if(updated) {
        view->updateRows();
    }
}

void PlaylistsPage::updated(const QModelIndex &index)
{
    view->updateRows(proxy.mapFromSource(index));
}

void PlaylistsPage::updateGenres(const QModelIndex &idx)
{
    if (idx.isValid()) {
        QModelIndex m=proxy.mapToSource(idx);
        if (m.isValid() && static_cast<PlaylistsModel::Item *>(m.internalPointer())->isPlaylist()) {
            genreCombo->update(static_cast<PlaylistsModel::PlaylistItem *>(m.internalPointer())->genres);
            return;
        }
    }
    genreCombo->update(PlaylistsModel::self()->genres());
}
