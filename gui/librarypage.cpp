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

#include "librarypage.h"
#include "mpdconnection.h"
#include "covers.h"
#include "musiclibraryitemsong.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#else
#include <QAction>
#endif

LibraryPage::LibraryPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    search->setPlaceholderText(i18n("Search library..."));
#else
    search->setPlaceholderText(tr("Search library..."));
#endif
    view->setPageDefaults();
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(search, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(MPDConnection::self(), SIGNAL(musicLibraryUpdated(MusicLibraryItemRoot *, QDateTime)), &model, SLOT(updateMusicLibrary(MusicLibraryItemRoot *, QDateTime)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            &model, SLOT(setCover(const QString &, const QString &, const QImage &)));
    connect(&model, SIGNAL(updateGenres(const QStringList &)), this, SLOT(updateGenres(const QStringList &)));
    connect(this, SIGNAL(listAllInfo(const QDateTime &)), MPDConnection::self(), SLOT(listAllInfo(const QDateTime &)));
}

LibraryPage::~LibraryPage()
{
}

void LibraryPage::refresh(Refresh type)
{
    if (RefreshForce==type) {
        model.clearUpdateTime();
    }

    if (RefreshFromCache!=type || !model.fromXML(MPDStats::self()->dbUpdate())) {
        emit listAllInfo(MPDStats::self()->dbUpdate());
    }
}

void LibraryPage::clear()
{
    model.clear();
}

void LibraryPage::addSelectionToPlaylist()
{
    QStringList files;
    MusicLibraryItem *item;
    MusicLibraryItemSong *songItem;

    // Get selected view indexes
    const QModelIndexList selected = view->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();

    if (selectionSize == 0) {
        return;
    }

    // Loop over the selection. Only add files.
    for (int selectionPos = 0; selectionPos < selectionSize; selectionPos++) {
        const QModelIndex current = selected.at(selectionPos);
        item = static_cast<MusicLibraryItem *>(proxy.mapToSource(current).internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Artist: {
            for (quint32 i = 0; ; i++) {
                const QModelIndex album = current.child(i , 0);
                if (!album.isValid())
                    break;

                for (quint32 j = 0; ; j++) {
                    const QModelIndex track = album.child(j, 0);
                    if (!track.isValid())
                        break;
                    const QModelIndex mappedSongIndex = proxy.mapToSource(track);
                    songItem = static_cast<MusicLibraryItemSong *>(mappedSongIndex.internalPointer());
                    const QString fileName = songItem->file();
                    if (!fileName.isEmpty() && !files.contains(fileName))
                        files.append(fileName);
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album: {
            for (quint32 i = 0; ; i++) {
                QModelIndex track = current.child(i, 0);
                if (!track.isValid())
                    break;
                const QModelIndex mappedSongIndex = proxy.mapToSource(track);
                songItem = static_cast<MusicLibraryItemSong *>(mappedSongIndex.internalPointer());
                const QString fileName = songItem->file();
                if (!fileName.isEmpty() && !files.contains(fileName))
                    files.append(fileName);
            }
            break;
        }
        case MusicLibraryItem::Type_Song: {
            const QString fileName = static_cast<MusicLibraryItemSong *>(item)->file();
            if (!fileName.isEmpty() && !files.contains(fileName))
                files.append(fileName);
            break;
        }
        default:
            break;
        }
    }

    if (!files.isEmpty()) {
        emit add(files);
        view->selectionModel()->clearSelection();
    }
}

void LibraryPage::itemActivated(const QModelIndex &)
{
    MusicLibraryItem *item;
    const QModelIndexList selected = view->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();
    if (selectionSize != 1) return; //doubleclick should only have one selected item

    const QModelIndex current = selected.at(0);
    item = static_cast<MusicLibraryItem *>(proxy.mapToSource(current).internalPointer());
    if (item->type() == MusicLibraryItem::Type_Song) {
        addSelectionToPlaylist();
    }
}

void LibraryPage::searchItems()
{
    proxy.setFilterGenre(0==genreCombo->currentIndex() ? QString() : genreCombo->currentText());

    if (search->text().isEmpty()) {
        proxy.setFilterRegExp(QString());
        return;
    }

    proxy.setFilterRegExp(search->text());
}

void LibraryPage::updateGenres(const QStringList &genres)
{
    QStringList entries;
#ifdef ENABLE_KDE_SUPPORT
    entries << i18n("All Genres");
#else
    entries << tr("All Genres");
#endif
    entries+=genres;

    bool diff=genreCombo->count() != entries.count();
    if (!diff) {
        // Check items...
        for (int i=1; i<genreCombo->count() && !diff; ++i) {
            if (genreCombo->itemText(i) != entries.at(i)) {
                diff=true;
            }
        }
    }

    if (!diff) {
        return;
    }

    QString currentFilter = genreCombo->currentIndex() ? genreCombo->currentText() : QString();

    genreCombo->clear();
    if (genres.count()<2) {
        genreCombo->setCurrentIndex(0);
    } else {
        genreCombo->addItems(entries);
        if (!currentFilter.isEmpty()) {
            bool found=false;
            for (int i=1; i<genreCombo->count() && !found; ++i) {
                if (genreCombo->itemText(i) == currentFilter) {
                    genreCombo->setCurrentIndex(i);
                    found=true;
                }
            }
            if (!found) {
                genreCombo->setCurrentIndex(0);
            }
        }
    }
}
