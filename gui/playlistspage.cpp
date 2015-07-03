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
#include "models/playlistsmodel.h"
#include "mpd-interface/mpdconnection.h"
#include "support/messagebox.h"
#include "support/inputdialog.h"
#include "support/localize.h"
#include "widgets/icons.h"
#include "stdactions.h"
#include "support/actioncollection.h"
#include "widgets/tableview.h"
#include "widgets/spacerwidget.h"
#include "widgets/menubutton.h"
#include "dynamic/dynamic.h"
#include "dynamic/dynamicpage.h"
#include "settings.h"
#include <QMenu>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#else
#include <QStyle>
#endif

class PlaylistTableView : public TableView
{
public:
    PlaylistTableView(QWidget *p)
        : TableView(QLatin1String("playlist"), p)
    {
        setUseSimpleDelegate();
        setIndentation(fontMetrics().width(QLatin1String("XX")));
    }

    virtual ~PlaylistTableView() { }
};

StoredPlaylistsPage::StoredPlaylistsPage(QWidget *p)
    : SinglePageWidget(p)
{
    renamePlaylistAction = new Action(Icon("edit-rename"), i18n("Rename"), this);
    removeDuplicatesAction=new Action(i18n("Remove Duplicates"), this);
    removeDuplicatesAction->setEnabled(false);

    view->allowGroupedView();
    view->allowTableView(new PlaylistTableView(view));
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
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(this, SIGNAL(loadPlaylist(const QString &, bool)), MPDConnection::self(), SLOT(loadPlaylist(const QString &, bool)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(removePlaylist(const QString &)));
    connect(this, SIGNAL(savePlaylist(const QString &)), MPDConnection::self(), SLOT(savePlaylist(const QString &)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(renamePlaylist(const QString &, const QString &)));
    connect(this, SIGNAL(removeFromPlaylist(const QString &, const QList<quint32> &)), MPDConnection::self(), SLOT(removeFromPlaylist(const QString &, const QList<quint32> &)));
    connect(StdActions::self()->savePlayQueueAction, SIGNAL(triggered()), this, SLOT(savePlaylist()));
    connect(renamePlaylistAction, SIGNAL(triggered()), this, SLOT(renamePlaylist()));
    connect(removeDuplicatesAction, SIGNAL(triggered()), this, SLOT(removeDuplicates()));
    connect(PlaylistsModel::self(), SIGNAL(updated(const QModelIndex &)), this, SLOT(updated(const QModelIndex &)));
    connect(PlaylistsModel::self(), SIGNAL(playlistRemoved(quint32)), view, SLOT(collectionRemoved(quint32)));
    intitiallyCollapseAction=new Action(i18n("Initially Collapse Albums"), this);
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
    init(ReplacePlayQueue|AddToPlayQueue, QList<QWidget *>() << menu);
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
//    foreach (const QModelIndex &idx, indexes) {
//        mapped.append(proxy.mapToSource(idx));
//    }
//
//    return PlaylistsModel::self()->filenames(mapped, true);
//}

void StoredPlaylistsPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    addItemsToPlayList(view->selectedIndexes(), name, replace, priorty);
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
        MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected playlists?\n\nThis cannot be undone."),
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

void StoredPlaylistsPage::savePlaylist()
{
    QString name = InputDialog::getText(i18n("Playlist Name"), i18n("Enter a name for the playlist:"), QString(), 0, this);

    if (!name.isEmpty()) {
        if (PlaylistsModel::self()->exists(name)) {
            if (MessageBox::No==MessageBox::warningYesNo(this, i18n("A playlist named '%1' already exists!\n\nOverwrite?", name),
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
        QString newName = InputDialog::getText(i18n("Rename Playlist"), i18n("Enter new name for playlist:"), name, 0, this);

        if (!newName.isEmpty() && name!=newName) {
            if (PlaylistsModel::self()->exists(newName)) {
                if (MessageBox::No==MessageBox::warningYesNo(this, i18n("A playlist named '%1' already exists!\n\nOverwrite?", newName),
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
            map[song.albumKey()+"-"+song.artistSong()].append(song);
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

void StoredPlaylistsPage::itemDoubleClicked(const QModelIndex &index)
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

void StoredPlaylistsPage::addItemsToPlayList(const QModelIndexList &indexes, const QString &name, bool replace, quint8 priorty)
{
    if (indexes.isEmpty()) {
        return;
    }

    bool filteredOnly=proxy.enabled() && Settings::self()->filteredOnly();
    // If we only have 1 item selected, see if it is a playlist. If so, we might be able to
    // just ask MPD to load it...
    if (name.isEmpty() && 1==indexes.count() && 0==priorty && !filteredOnly) {
        QModelIndex idx=proxy.mapToSource(*(indexes.begin()));
        PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(idx.internalPointer());

        if (item->isPlaylist()) {
            emit loadPlaylist(static_cast<PlaylistsModel::PlaylistItem*>(item)->name, replace);
            return;
        }
    }

    if (!name.isEmpty()) {
        foreach (const QModelIndex &idx, indexes) {
            QModelIndex m=proxy.mapToSource(idx);
            PlaylistsModel::Item *item=static_cast<PlaylistsModel::Item *>(m.internalPointer());
            if ( (item->isPlaylist() && static_cast<PlaylistsModel::PlaylistItem *>(item)->name==name) ||
                 (!item->isPlaylist() && static_cast<PlaylistsModel::SongItem *>(item)->parent->name==name) ) {
                MessageBox::error(this, i18n("Cannot add songs from '%1' to '%2'", name, name));
                return;
            }
        }
    }

    QStringList files=PlaylistsModel::self()->filenames(proxy.mapToSource(indexes, filteredOnly));
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
QList<Song> StoredPlaylistsPage::selectedSongs(bool allowPlaylists) const
{
    Q_UNUSED(allowPlaylists)
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return PlaylistsModel::self()->songs(proxy.mapToSource(selected, proxy.enabled() && Settings::self()->filteredOnly()));
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
        foreach (const QModelIndex &index, selected) {
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
    StdActions::self()->removeAction->setEnabled(enableActions && !allSmartPlaylists);
    StdActions::self()->replacePlayQueueAction->setEnabled(enableActions);
    StdActions::self()->addToPlayQueueAction->setEnabled(enableActions);
    StdActions::self()->addWithPriorityAction->setEnabled(enableActions);
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

PlaylistsPage::PlaylistsPage(QWidget *p)
    : MultiPageWidget(p)
{
    stored=new StoredPlaylistsPage(this);
    addPage(PlaylistsModel::self()->name(), PlaylistsModel::self()->icon(), PlaylistsModel::self()->title(), PlaylistsModel::self()->descr(), stored);
    dynamic=new DynamicPage(this);
    addPage(Dynamic::self()->name(), Dynamic::self()->icon(), Dynamic::self()->title(), Dynamic::self()->descr(), dynamic);

    connect(stored, SIGNAL(addToDevice(QString,QString,QList<Song>)), SIGNAL(addToDevice(QString,QString,QList<Song>)));
}

PlaylistsPage::~PlaylistsPage()
{
}

#ifdef ENABLE_DEVICES_SUPPORT
void PlaylistsPage::addSelectionToDevice(const QString &udi)
{
    if (stored==currentWidget()) {
        stored->addSelectionToDevice(udi);
    }
}
#endif
