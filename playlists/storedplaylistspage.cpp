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

#include "storedplaylistspage.h"
#include "models/playlistsmodel.h"
#include "mpd-interface/mpdconnection.h"
#include "support/messagebox.h"
#include "support/inputdialog.h"
#include "widgets/icons.h"
#include "gui/stdactions.h"
#include "gui/customactions.h"
#include "support/actioncollection.h"
#include "support/configuration.h"
#include "widgets/tableview.h"
#include "widgets/spacerwidget.h"
#include "widgets/menubutton.h"
#include "gui/settings.h"
#include <QMenu>
#include <QStyle>

class PlaylistTableView : public TableView
{
public:
    PlaylistTableView(QWidget *p)
        : TableView(QLatin1String("playlist"), p)
    {
        setUseSimpleDelegate();
        setIndentation(fontMetrics().width(QLatin1String("XX")));
    }

    ~PlaylistTableView() override { }
};

StoredPlaylistsPage::StoredPlaylistsPage(QWidget *p)
    : SinglePageWidget(p)
{
    renamePlaylistAction = new Action(Icons::self()->editIcon, tr("Rename"), this);
    removeDuplicatesAction=new Action(tr("Remove Duplicates"), this);
    removeDuplicatesAction->setEnabled(false);
    removeInvalidAction=new Action(tr("Remove Invalid Tracks"), this);
    removeInvalidAction->setEnabled(false);

    view->allowGroupedView();
    view->allowTableView(new PlaylistTableView(view));
    view->setAcceptDrops(true);
    view->setDragDropOverwriteMode(false);
    view->setDragDropMode(QAbstractItemView::DragDrop);

    proxy.setSourceModel(PlaylistsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(StdActions::self()->removeAction);
    view->alwaysShowHeader();
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(this, SIGNAL(loadPlaylist(const QString &, bool)), MPDConnection::self(), SLOT(loadPlaylist(const QString &, bool)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(removePlaylist(const QString &)));
    connect(this, SIGNAL(savePlaylist(const QString &, bool)), MPDConnection::self(), SLOT(savePlaylist(const QString &, bool)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(renamePlaylist(const QString &, const QString &)));
    connect(this, SIGNAL(removeFromPlaylist(const QString &, const QList<quint32> &)), MPDConnection::self(), SLOT(removeFromPlaylist(const QString &, const QList<quint32> &)));
    connect(StdActions::self()->savePlayQueueAction, SIGNAL(triggered()), this, SLOT(savePlaylist()));
    connect(renamePlaylistAction, SIGNAL(triggered()), this, SLOT(renamePlaylist()));
    connect(removeDuplicatesAction, SIGNAL(triggered()), this, SLOT(removeDuplicates()));
    connect(removeInvalidAction, SIGNAL(triggered()), this, SLOT(removeInvalid()));
    connect(PlaylistsModel::self(), SIGNAL(updated(const QModelIndex &)), this, SLOT(updated(const QModelIndex &)));
    connect(PlaylistsModel::self(), SIGNAL(playlistRemoved(quint32)), view, SLOT(collectionRemoved(quint32)));
    intitiallyCollapseAction=new Action(tr("Initially Collapse Albums"), this);
    intitiallyCollapseAction->setCheckable(true);
    Configuration config(metaObject()->className());
    view->setMode(ItemView::Mode_DetailedTree);
    view->load(config);
    intitiallyCollapseAction->setChecked(view->isStartClosed());
    connect(intitiallyCollapseAction, SIGNAL(toggled(bool)), SLOT(setStartClosed(bool)));
    MenuButton *menu=new MenuButton(this);
    menu->addAction(createViewMenu(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                           << ItemView::Mode_DetailedTree << ItemView::Mode_List
                                                           << ItemView::Mode_GroupedTree << ItemView::Mode_Table));
    menu->addAction(intitiallyCollapseAction);
    init(ReplacePlayQueue|AppendToPlayQueue, QList<QWidget *>() << menu);

    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    view->addAction(CustomActions::self());
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    view->addAction(renamePlaylistAction);
    view->addAction(StdActions::self()->removeAction);
    view->addAction(removeDuplicatesAction);
    view->addAction(removeInvalidAction);
    connect(view, SIGNAL(updateToPlayQueue(QModelIndex,bool)), this, SLOT(updateToPlayQueue(QModelIndex,bool)));
    view->setInfoText(tr("Either save the play queue, or use the context menu in the library, to create new playlists."));
}

StoredPlaylistsPage::~StoredPlaylistsPage()
{
    Configuration config(metaObject()->className());
    view->save(config);
}

void StoredPlaylistsPage::updateRows()
{
    view->updateRows();
}

void StoredPlaylistsPage::clear()
{
    PlaylistsModel::self()->clear();
}

//QStringList StoredPlaylistsPage::selectedFiles() const
//{
//    QModelIndexList indexes=view->selectedIndexes();
//    if (indexes.isEmpty()) {
//        return QStringList();
//    }
//
//    QModelIndexList mapped;
//    for (const QModelIndex &idx: indexes) {
//        mapped.append(proxy.mapToSource(idx));
//    }
//
//    return PlaylistsModel::self()->filenames(mapped, true);
//}

void StoredPlaylistsPage::addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority)
{
    addItemsToPlayList(view->selectedIndexes(), name, action, priority, decreasePriority);
}

void StoredPlaylistsPage::setView(int mode)
{
    PlaylistsModel::self()->setMultiColumn(ItemView::Mode_Table==mode);
    SinglePageWidget::setView(mode);
    intitiallyCollapseAction->setEnabled(ItemView::Mode_GroupedTree==mode);
}

void StoredPlaylistsPage::removeItems()
{
    QSet<QString> remPlaylists;
    QMap<QString, QList<quint32> > remSongs;
    QModelIndexList selected = view->selectedIndexes();

    for (const QModelIndex &index: selected) {
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
        MessageBox::No==MessageBox::warningYesNo(this, tr("Are you sure you wish to remove the selected playlists?\n\nThis cannot be undone."),
                                                 tr("Remove Playlists"), StdGuiItem::remove(), StdGuiItem::cancel())) {
            return;
    }

    for (const QString &pl: remPlaylists) {
        emit removePlaylist(pl);
    }
    QMap<QString, QList<quint32> >::ConstIterator it=remSongs.constBegin();
    QMap<QString, QList<quint32> >::ConstIterator end=remSongs.constEnd();
    for (; it!=end; ++it) {
        emit removeFromPlaylist(it.key(), it.value());
    }
}

void StoredPlaylistsPage::savePlaylist()
{
    QStringList existing;
    for (int i=0; i<proxy.rowCount(); ++i) {
        existing.append(proxy.data(proxy.index(i, 0, QModelIndex())).toString());
    }

    QString name = InputDialog::getText(tr("Playlist Name"), tr("Enter a name for the playlist:"), lastPlaylist, existing, nullptr, this);
    if (!name.isEmpty()) {
        lastPlaylist=name;
        emit savePlaylist(name, true);
    }
}

void StoredPlaylistsPage::renamePlaylist()
{
    const QModelIndexList items = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==items.size()) {
        QModelIndex index = proxy.mapToSource(items.first());
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(index.internalPointer());
        if (!item->isPlaylist()) {
            return;
        }
        QString name = static_cast<PlaylistsModel::PlaylistItem *>(item)->name;
        QString newName = InputDialog::getText(tr("Rename Playlist"), tr("Enter new name for playlist:"), name, nullptr, this);

        if (!newName.isEmpty() && name!=newName) {
            if (PlaylistsModel::self()->exists(newName)) {
                if (MessageBox::No==MessageBox::warningYesNo(this, tr("A playlist named '%1' already exists!\n\nOverwrite?").arg(newName),
                                                             tr("Overwrite Playlist"), StdGuiItem::overwrite(), StdGuiItem::cancel())) {
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

void StoredPlaylistsPage::removeDuplicates()
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
            map[song.albumKey()+"-"+song.artistSong()+"-"+song.track].append(song);
        }

        QList<quint32> toRemove;
        for (const QString &key: map.keys()) {
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

void StoredPlaylistsPage::removeInvalid()
{
    const QModelIndexList items = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==items.size()) {
        QModelIndex index = proxy.mapToSource(items.first());
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(index.internalPointer());
        if (!item->isPlaylist()) {
            return;
        }

        PlaylistsModel::PlaylistItem *pl=static_cast<PlaylistsModel::PlaylistItem *>(item);
        QList<quint32> toRemove;
        for (quint32 i=0; i<pl->songs.count(); ++i) {
            const auto song = pl->songs.at(i);
            if (song->isInvalid()) {
                toRemove.append(i);
            }
        }
        if (!toRemove.isEmpty()) {
            emit removeFromPlaylist(pl->name, toRemove);
        }
    }
}

void StoredPlaylistsPage::itemDoubleClicked(const QModelIndex &index)
{
    if (index.isValid() && (style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, nullptr, this)
        || !static_cast<PlaylistsModel::Item *>(proxy.mapToSource(index).internalPointer())->isPlaylist())) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayList(indexes, QString(), MPDConnection::Append);
    }
}

void StoredPlaylistsPage::addItemsToPlayList(const QModelIndexList &indexes, const QString &name, int action, quint8 priority, bool decreasePriority)
{
    if (indexes.isEmpty()) {
        return;
    }

    // If we only have 1 item selected, see if it is a playlist. If so, we might be able to
    // just ask MPD to load it...
    if (name.isEmpty() && 1==indexes.count() && 0==priority && !proxy.enabled() && MPDConnection::Append==action) {
        QModelIndex idx=proxy.mapToSource(*(indexes.begin()));
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(idx.internalPointer());

        if (item->isPlaylist()) {
            lastPlaylist=static_cast<PlaylistsModel::PlaylistItem*>(item)->name;
            emit loadPlaylist(lastPlaylist, false);
            return;
        }
    }

    if (!name.isEmpty()) {
        lastPlaylist.clear();
        for (const QModelIndex &idx: indexes) {
            QModelIndex m=proxy.mapToSource(idx);
            PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(m.internalPointer());
            if ( (item->isPlaylist() && static_cast<PlaylistsModel::PlaylistItem *>(item)->name==name) ||
                 (!item->isPlaylist() && static_cast<PlaylistsModel::SongItem *>(item)->parent->name==name) ) {
                MessageBox::error(this, tr("Cannot add songs from '%1' to '%2'").arg(name).arg(name));
                return;
            }
        }
    }

    QStringList files=PlaylistsModel::self()->filenames(proxy.mapToSource(indexes));
    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, action, priority, decreasePriority);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
QList<Song> StoredPlaylistsPage::selectedSongs(bool allowPlaylists) const
{
    Q_UNUSED(allowPlaylists)
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return PlaylistsModel::self()->songs(proxy.mapToSource(selected));
}

void StoredPlaylistsPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();
    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}
#endif

void StoredPlaylistsPage::controlActions()
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
        for (const QModelIndex &index: selected) {
            PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(proxy.mapToSource(index).internalPointer());
            if (item && !(item->isPlaylist() ? static_cast<PlaylistsModel::PlaylistItem *>(item)->isSmartPlaylist
                                             : static_cast<PlaylistsModel::SongItem *>(item)->parent->isSmartPlaylist)) {
                allSmartPlaylists=false;
                break;
            }
        }
    }

    renamePlaylistAction->setEnabled(canRename);
    removeDuplicatesAction->setEnabled(enableActions && !allSmartPlaylists);
    removeInvalidAction->setEnabled(enableActions && !allSmartPlaylists);
    StdActions::self()->removeAction->setEnabled(enableActions && !allSmartPlaylists);
    StdActions::self()->enableAddToPlayQueue(enableActions);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enableActions);
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->copyToDeviceAction->setEnabled(enableActions && !allSmartPlaylists);
    #endif
}

void StoredPlaylistsPage::doSearch()
{
    QString text=view->searchText().trimmed();
    bool updated=proxy.update(text);
    if (proxy.enabled() && !proxy.filterText().isEmpty()) {
        view->expandAll();
    }
    if(updated) {
        view->updateRows();
    }
}

void StoredPlaylistsPage::updated(const QModelIndex &index)
{
    view->updateRows(proxy.mapFromSource(index));
}

void StoredPlaylistsPage::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void StoredPlaylistsPage::setStartClosed(bool sc)
{
    view->setStartClosed(sc);
}

void StoredPlaylistsPage::updateToPlayQueue(const QModelIndex &idx, bool replace)
{
    QStringList files=PlaylistsModel::self()->filenames(proxy.mapToSource(QModelIndexList() << idx, false));
    if (!files.isEmpty()) {
        emit add(files, replace ? MPDConnection::ReplaceAndplay : MPDConnection::Append, 0, false);
    }
}

#include "moc_storedplaylistspage.cpp"
