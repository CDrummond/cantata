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

#include "albumspage.h"
#include "mpdconnection.h"
#include "covers.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "albumsmodel.h"
#include "localize.h"
#include "messagebox.h"
#include "settings.h"
#include "stdactions.h"

AlbumsPage::AlbumsPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
    libraryUpdate->setDefaultAction(StdActions::self()->refreshAction);
    searchButton->setDefaultAction(StdActions::self()->searchAction);

    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->replacePlayQueueAction);
    view->addAction(StdActions::self()->addWithPriorityAction);
    view->addAction(StdActions::self()->addToStoredPlaylistAction);
    #ifdef TAGLIB_FOUND
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(StdActions::self()->copyToDeviceAction);
    #endif
    view->addAction(StdActions::self()->organiseFilesAction);
    view->addAction(StdActions::self()->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(StdActions::self()->replaygainAction);
    #endif
    view->addAction(StdActions::self()->setCoverAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    view->addAction(sep);
    view->addAction(StdActions::self()->deleteSongsAction);
    #endif
    #endif // TAGLIB_FOUND

    proxy.setSourceModel(AlbumsModel::self());
    view->setModel(&proxy);

    connect(MusicLibraryModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary()), view, SLOT(showSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), view, SLOT(hideSpinner()));
}

AlbumsPage::~AlbumsPage()
{
}

void AlbumsPage::setView(int v)
{
    setItemSize(v);
    view->setMode((ItemView::Mode)v);
}

void AlbumsPage::clear()
{
    AlbumsModel::self()->clear();
    view->update();
}

void AlbumsPage::setItemSize(int v)
{
    if (ItemView::Mode_IconTop!=v) {
        AlbumsModel::setItemSize(QSize(0, 0));
    } else {
        QFontMetrics fm(font());

        int size=AlbumsModel::iconSize();
        QSize grid(size+8, size+(fm.height()*2.5));
        view->setGridSize(grid);
        AlbumsModel::setItemSize(grid-QSize(4, 4));
    }
}

QStringList AlbumsPage::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return AlbumsModel::self()->filenames(mapped, allowPlaylists);
}

QList<Song> AlbumsPage::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return AlbumsModel::self()->songs(mapped, allowPlaylists);
}

Song AlbumsPage::coverRequest() const
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==selected.count()) {
        QList<Song> songs=AlbumsModel::self()->songs(QModelIndexList() << proxy.mapToSource(selected.at(0)), false);
        if (!songs.isEmpty()) {
            return songs.at(0);
        }
    }
    return Song();
}

void AlbumsPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty)
{
    QStringList files=selectedFiles(true);

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
void AlbumsPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void AlbumsPage::deleteSongs()
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to delete the selected songs?\nThis cannot be undone."),
                                                      i18n("Delete Songs"), StdGuiItem::del(), StdGuiItem::cancel())) {
            emit deleteSongs(QString(), songs);
        }
        view->clearSelection();
    }
}
#endif

void AlbumsPage::itemActivated(const QModelIndex &)
{
    if (1==view->selectedIndexes(false).size()) {//doubleclick should only have one selected item
        addSelectionToPlaylist();
    }
}

void AlbumsPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

void AlbumsPage::updateGenres(const QModelIndex &idx)
{
    if (idx.isValid()) {
        QModelIndex m=proxy.mapToSource(idx);
        if (m.isValid() && static_cast<AlbumsModel::Item *>(m.internalPointer())->isAlbum()) {
            genreCombo->update(static_cast<AlbumsModel::AlbumItem *>(m.internalPointer())->genres);
            return;
        }
    }
    genreCombo->update(MusicLibraryModel::self()->genres());
}

void AlbumsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false); // Dont need sorted selection here...
    bool enable=selected.count()>0;

    StdActions::self()->addToPlayQueueAction->setEnabled(enable);
    StdActions::self()->addWithPriorityAction->setEnabled(enable);
    StdActions::self()->replacePlayQueueAction->setEnabled(enable);
    StdActions::self()->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef TAGLIB_FOUND
    StdActions::self()->organiseFilesAction->setEnabled(enable && MPDConnection::self()->getDetails().dirReadable);
    StdActions::self()->editTagsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    StdActions::self()->replaygainAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    StdActions::self()->copyToDeviceAction->setEnabled(StdActions::self()->organiseFilesAction->isEnabled());
    #endif
    #endif // TAGLIB_FOUND
    StdActions::self()->setCoverAction->setEnabled(1==selected.count() && static_cast<AlbumsModel::Item *>(proxy.mapToSource(selected.at(0)).internalPointer())->isAlbum());
}
