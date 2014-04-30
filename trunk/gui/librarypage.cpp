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

#include "librarypage.h"
#include "mpdconnection.h"
#include "mpdstats.h"
#include "covers.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "localize.h"
#include "messagebox.h"
#include "settings.h"
#include "stdactions.h"
#include "utils.h"

LibraryPage::LibraryPage(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(StdActions::self()->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);

    view->addAction(StdActions::self()->addToPlayQueueAction);
    view->addAction(StdActions::self()->addRandomToPlayQueueAction);
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

    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary()), this, SLOT(showSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), this, SLOT(hideSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatingDatabase()), this, SLOT(showSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatedDatabase()), this, SLOT(hideSpinner()));
    connect(MusicLibraryModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(this, SIGNAL(loadLibrary()), MPDConnection::self(), SLOT(loadLibrary()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(rootIndexSet(QModelIndex)), this, SLOT(updateGenres(QModelIndex)));
    proxy.setSourceModel(MusicLibraryModel::self());
    view->setModel(&proxy);
}

LibraryPage::~LibraryPage()
{
}

void LibraryPage::setView(int v)
{
    setItemSize(v);
    view->setMode((ItemView::Mode)v);
    MusicLibraryModel::self()->setLargeImages(ItemView::Mode_IconTop==v);
}

void LibraryPage::showEvent(QShowEvent *e)
{
    view->focusView();
    QWidget::showEvent(e);
}

void LibraryPage::setItemSize(int v)
{
    if (ItemView::Mode_IconTop!=v) {
        MusicLibraryItemAlbum::setItemSize(QSize(0, 0));
    } else {
        QFontMetrics fm(font());

        int size=MusicLibraryItemAlbum::iconSize(true);
        QSize grid(size+8, size+(fm.height()*2.5));
        view->setGridSize(grid);
        MusicLibraryItemAlbum::setItemSize(grid-QSize(4, 4));
    }
}

void LibraryPage::refresh()
{
    view->goToTop();

    if (!MusicLibraryModel::self()->fromXML()) {
        emit loadLibrary();
    }
}

void LibraryPage::clear()
{
    MusicLibraryModel::self()->clear();
    view->goToTop();
}

static inline QString nameKey(const QString &artist, const QString &album)
{
    return '{'+artist+"}{"+album+'}';
}

QStringList LibraryPage::selectedFiles(bool allowPlaylists, bool randomAlbums) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }

    QModelIndexList mapped=proxy.mapToSource(selected, Settings::self()->filteredOnly());
    if (randomAlbums) {
        if (Settings::self()->filteredOnly()) {
            QMap<quint32, QModelIndexList> albums;
            foreach (const QModelIndex &idx, mapped) {
                if (idx.parent().isValid() && idx.parent().parent().isValid()) {
                    albums[(idx.parent().parent().row()<<16)+idx.parent().row()].append(idx);
                }
            }
            QList<quint32> keys=albums.keys();
            if (keys.isEmpty()) {
                return QStringList();
            } else if (1==keys.count()) {
                mapped=albums.begin().value();
            } else {
                mapped.clear();
                while (!keys.isEmpty()) {
                    mapped.append(albums[keys.takeAt(Utils::random(keys.count()))]);
                }
            }
        } else {
            QModelIndexList albumIndexes;
            QSet<QString> albumNames;
            foreach (const QModelIndex &idx, mapped) {
                MusicLibraryItem *item=static_cast<MusicLibraryItem *>(idx.internalPointer());
                if (MusicLibraryItem::Type_Album==item->itemType()) {
                    QString name=nameKey(item->parentItem()->data(), item->data());
                    if (!albumNames.contains(name)) {
                        albumNames.insert(name);
                        albumIndexes.append(idx);
                    }
                } else if (MusicLibraryItem::Type_Artist==item->itemType()) {
                    for (int row=0; row<static_cast<MusicLibraryItemContainer *>(item)->childCount(); ++row) {
                        MusicLibraryItem *album=static_cast<MusicLibraryItemContainer *>(item)->childItem(row);
                        QString name=nameKey(item->data(), album->data());
                        if (!albumNames.contains(name)) {
                            albumNames.insert(name);
                            albumIndexes.append(MusicLibraryModel::self()->index(row, 0, idx));
                        }
                    }
                }
            }

            if (albumIndexes.isEmpty()) {
                return QStringList();
            }

            if (1==albumIndexes.count()) {
                mapped=albumIndexes;
            } else {
                mapped.clear();
                while (!albumIndexes.isEmpty()) {
                    mapped.append(albumIndexes.takeAt(Utils::random(albumIndexes.count())));
                }
            }
        }
    }

    return MusicLibraryModel::self()->filenames(mapped, allowPlaylists);
}

QList<Song> LibraryPage::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return MusicLibraryModel::self()->songs(proxy.mapToSource(selected, Settings::self()->filteredOnly()), allowPlaylists);
}

Song LibraryPage::coverRequest() const
{
    QModelIndexList selected = view->selectedIndexes(false); // Dont need sorted selection here...

    if (1==selected.count()) {
        QModelIndex idx=proxy.mapToSource(selected.at(0));
        QList<Song> songs=MusicLibraryModel::self()->songs(QModelIndexList() << idx, false);
        if (!songs.isEmpty()) {
            Song s=songs.at(0);

            if (MusicLibraryItem::Type_Artist==static_cast<MusicLibraryItem *>(idx.internalPointer())->itemType()) {
                s.setArtistImageRequest();
            }
            return s;
        }
    }
    return Song();
}

void LibraryPage::addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty, bool randomAlbums)
{
    QStringList files=selectedFiles(name.isEmpty(), randomAlbums);

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
void LibraryPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void LibraryPage::deleteSongs()
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

void LibraryPage::showSongs(const QList<Song> &songs)
{
    // Filter out non-mpd file songs...
    QList<Song> sngs;
    foreach (const Song &s, songs) {
        if (!s.file.isEmpty() && !s.file.contains(":/") && !s.file.startsWith('/')) {
            sngs.append(s);
        }
    }

    if (sngs.isEmpty()) {
        return;
    }

    genreCombo->setCurrentIndex(0);
    view->clearSearchText();

    bool first=true;
    foreach (const Song &s, sngs) {
        QModelIndex idx=MusicLibraryModel::self()->findSongIndex(s);
        if (idx.isValid()) {
            QModelIndex p=proxy.mapFromSource(idx);
            if (p.isValid()) {
                if (ItemView::Mode_SimpleTree==view->viewMode() || ItemView::Mode_DetailedTree==view->viewMode() || first) {
                    view->showIndex(p, first);
                }
                if (first) {
                    first=false;
                }
                if (ItemView::Mode_SimpleTree!=view->viewMode() && ItemView::Mode_DetailedTree!=view->viewMode()) {
                    return;
                }
            }
        }
    }
}

void LibraryPage::showArtist(const QString &artist)
{
    QModelIndex idx=MusicLibraryModel::self()->findArtistIndex(artist);
    if (idx.isValid()) {
        idx=proxy.mapFromSource(idx);
        genreCombo->setCurrentIndex(0);
        view->clearSearchText();
        view->showIndex(idx, true);
        if (ItemView::Mode_SimpleTree==view->viewMode() || ItemView::Mode_DetailedTree==view->viewMode()) {
            view->setExpanded(idx);
        }
    }
}

void LibraryPage::showAlbum(const QString &artist, const QString &album)
{
    QModelIndex idx=MusicLibraryModel::self()->findAlbumIndex(artist, album);
    if (idx.isValid()) {
        idx=proxy.mapFromSource(idx);
        genreCombo->setCurrentIndex(0);
        view->clearSearchText();
        view->showIndex(idx, true);
        if (ItemView::Mode_SimpleTree==view->viewMode() || ItemView::Mode_DetailedTree==view->viewMode()) {
            view->setExpanded(idx.parent());
            view->setExpanded(idx);
        }
    }
}


void LibraryPage::itemDoubleClicked(const QModelIndex &)
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

void LibraryPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

void LibraryPage::updateGenres(const QModelIndex &idx)
{
    if (idx.isValid()) {
        QModelIndex m=proxy.mapToSource(idx);
        if (m.isValid()) {
            MusicLibraryItem::Type itemType=static_cast<MusicLibraryItem *>(m.internalPointer())->itemType();
            if (itemType==MusicLibraryItem::Type_Artist) {
                genreCombo->update(static_cast<MusicLibraryItemArtist *>(m.internalPointer())->genres());
                return;
            } else if (itemType==MusicLibraryItem::Type_Album) {
                genreCombo->update(static_cast<MusicLibraryItemAlbum *>(m.internalPointer())->genres());
                return;
            }
        }
    }
    genreCombo->update(MusicLibraryModel::self()->genres());
}

void LibraryPage::controlActions()
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

    if (1==selected.count()) {
        MusicLibraryItem::Type type=static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.at(0)).internalPointer())->itemType();
        StdActions::self()->setCoverAction->setEnabled(MusicLibraryItem::Type_Artist==type || MusicLibraryItem::Type_Album==type);
    } else {
        StdActions::self()->setCoverAction->setEnabled(false);
    }

    bool allowRandomAlbum=!selected.isEmpty();
    if (allowRandomAlbum) {
        foreach (const QModelIndex &idx, selected) {
            if (MusicLibraryItem::Type_Song==static_cast<MusicLibraryItem *>(proxy.mapToSource(idx).internalPointer())->itemType()) {
                allowRandomAlbum=false;
                break;
            }
        }
    }
    StdActions::self()->addRandomToPlayQueueAction->setEnabled(allowRandomAlbum);
}

void LibraryPage::showSpinner()
{
    view->showSpinner();
    view->showMessage(i18n("Updating..."), -1);
}

void LibraryPage::hideSpinner()
{
    view->hideSpinner();
    view->showMessage(QString(), 0);
}
