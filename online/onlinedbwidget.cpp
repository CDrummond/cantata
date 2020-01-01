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

#include "onlinedbwidget.h"
#include "gui/stdactions.h"
#include "widgets/itemview.h"
#include "widgets/menubutton.h"
#include "widgets/icons.h"
#include "support/action.h"
#include "support/messagebox.h"
#include "support/configuration.h"
#include <QTimer>

OnlineDbWidget::OnlineDbWidget(OnlineDbService *s, QWidget *p)
    : SinglePageWidget(p)
    , configGroup(s->name())
    , srv(s)
{
    srv->setParent(this);
    view->setModel(s);
    view->alwaysShowHeader();
    Configuration config(configGroup);
    view->setMode(ItemView::Mode_DetailedTree);
    view->load(config);
    srv->load(config);
    MenuButton *menu=new MenuButton(this);
    menu->addAction(createViewMenu(QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                           << ItemView::Mode_DetailedTree << ItemView::Mode_List));
    menu->addAction(createMenuGroup(tr("Group By"), QList<MenuItem>() << MenuItem(tr("Genre"), SqlLibraryModel::T_Genre)
                                                                        << MenuItem(tr("Artist"), SqlLibraryModel::T_Artist),
                                    srv->topLevel(), this, SLOT(groupByChanged())));
    Action *configureAction=new Action(Icons::self()->configureIcon, tr("Configure"), this);
    connect(configureAction, SIGNAL(triggered()), SLOT(configure()));
    menu->addAction(configureAction);
    init(ReplacePlayQueue|AppendToPlayQueue|Refresh, QList<QWidget *>() << menu);
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
    connect(view, SIGNAL(updateToPlayQueue(QModelIndex,bool)), this, SLOT(updateToPlayQueue(QModelIndex,bool)));
    view->setOpenAfterSearch(SqlLibraryModel::T_Album!=srv->topLevel());
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    connect(StdActions::self()->addRandomAlbumToPlayQueueAction, SIGNAL(triggered()), SLOT(addRandomAlbum()));
    connect(srv, SIGNAL(modelReset()), this, SLOT(modelReset()));
}

OnlineDbWidget::~OnlineDbWidget()
{
    Configuration config(configGroup);
    view->save(config);
    srv->save(config);
}

void OnlineDbWidget::groupByChanged()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (!act) {
        return;
    }
    int mode=act->property(constValProp).toInt();
    srv->setTopLevel((SqlLibraryModel::Type)mode);
    view->setOpenAfterSearch(SqlLibraryModel::T_Album!=srv->topLevel());
}

QStringList OnlineDbWidget::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return srv->filenames(selected, allowPlaylists);
}

QList<Song> OnlineDbWidget::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return srv->songs(selected, allowPlaylists);
}

void OnlineDbWidget::showEvent(QShowEvent *e)
{
    SinglePageWidget::showEvent(e);
    if (srv->isDownloading() || srv->rowCount(QModelIndex())) {
        return;
    }
    if (srv->previouslyDownloaded()) {
        srv->open();
    } else {
        QTimer::singleShot(0, this, SLOT(firstTimePrompt()));
    }
}

void OnlineDbWidget::firstTimePrompt()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, srv->averageSize()
                                                        ? tr("The music listing needs to be downloaded, this can consume over %1Mb of disk space").arg(srv->averageSize())
                                                        : tr("Dowload music listing?"),
                                                   QString(), GuiItem(tr("Download")), StdGuiItem::cancel())) {
        emit close();
    } else {
        srv->download(false);
    }
}

void OnlineDbWidget::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void OnlineDbWidget::addRandomAlbum()
{
    if (!isVisible()) {
        return;
    }
    QStringList genres;
    QStringList artists;
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    for (const QModelIndex &idx: selected) {
        SqlLibraryModel::Item *item=static_cast<SqlLibraryModel::Item *>(idx.internalPointer());
        switch (item->getType()) {
        case SqlLibraryModel::T_Genre:
            genres.append(item->getId());
            break;
        case SqlLibraryModel::T_Artist:
            artists.append(item->getId());
        default:
            break;
        }
    }

    // If all items selected, then just choose random of all albums
    if (SqlLibraryModel::T_Genre==srv->topLevel() && genres.size()==srv->rowCount(QModelIndex())) {
        genres=QStringList();
    }
    if (SqlLibraryModel::T_Artist==srv->topLevel() && artists.size()==srv->rowCount(QModelIndex())) {
        artists=QStringList();
    }

    LibraryDb::Album album=srv->getRandomAlbum(genres, artists);
    if (album.artist.isEmpty() || album.id.isEmpty()) {
        return;
    }
    QList<Song> songs=srv->getAlbumTracks(album.artist, album.id);
    if (!songs.isEmpty()) {
        QStringList files;
        for (const Song &s: songs) {
            files.append(s.file);
        }
        emit add(files, /*replace ? MPDConnection::ReplaceAndplay : */MPDConnection::Append, 0, false);
    }
}

void OnlineDbWidget::modelReset()
{
    int count = srv->trackCount();
    view->setMinSearchDebounce(count <= 12500 ? 1000u : count <= 18000 ? 1500u : 2000u);
}

void OnlineDbWidget::doSearch()
{
    srv->search(view->searchText());
}

// TODO: Cancel download?
void OnlineDbWidget::refresh()
{
    if (!srv->isDownloading() && MessageBox::Yes==MessageBox::questionYesNo(this, tr("Re-download music listing?"), QString(), GuiItem(tr("Download")), StdGuiItem::cancel())) {
        srv->download(true);
    }
}

void OnlineDbWidget::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;

    StdActions::self()->enableAddToPlayQueue(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);

    bool allowRandomAlbum=isVisible() && !selected.isEmpty();
    if (allowRandomAlbum) {
        for (const QModelIndex &idx: selected) {
            if (SqlLibraryModel::T_Track==static_cast<SqlLibraryModel::Item *>(idx.internalPointer())->getType() ||
                SqlLibraryModel::T_Album==static_cast<SqlLibraryModel::Item *>(idx.internalPointer())->getType()) {
                allowRandomAlbum=false;
                break;
            }
        }
    }
    StdActions::self()->addRandomAlbumToPlayQueueAction->setVisible(allowRandomAlbum);
}

void OnlineDbWidget::configure()
{
    srv->configure(this);
}

void OnlineDbWidget::updateToPlayQueue(const QModelIndex &idx, bool replace)
{
    QStringList files=srv->filenames(QModelIndexList() << idx, true);
    if (!files.isEmpty()) {
        emit add(files, replace ? MPDConnection::ReplaceAndplay : MPDConnection::Append, 0, false);
    }
}

#include "moc_onlinedbwidget.cpp"
