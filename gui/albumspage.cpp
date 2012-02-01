/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "musiclibraryitemsong.h"
#include "albumsmodel.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#ifdef ENABLE_DEVICES_SUPPORT
#include <KDE/KMessageBox>
#endif
#else
#include <QtGui/QAction>
#endif

AlbumsPage::AlbumsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->refreshAction);

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Albums"));
    #else
    view->setTopText(tr("Albums"));
    #endif
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(p->addToStoredPlaylistAction);
    #ifdef TAGLIB_FOUND
    view->addAction(p->editTagsAction);
    #endif
    view->addAction(p->burnAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->copyToDeviceAction);
    view->addAction(p->deleteSongsAction);
    #endif
    proxy.setSourceModel(AlbumsModel::self());
    view->setModel(&proxy);
    view->init(p->replacePlaylistAction, p->addToPlaylistAction);

    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary()), view, SLOT(showSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), view, SLOT(hideSpinner()));
}

AlbumsPage::~AlbumsPage()
{
}

void AlbumsPage::setView(int v)
{
    view->setMode((ItemView::Mode)v);
    setItemSize();
}

void AlbumsPage::clear()
{
    AlbumsModel::self()->clear();
    view->update();
}

void AlbumsPage::setItemSize()
{
    if (ItemView::Mode_IconTop!=view->viewMode()) {
        AlbumsModel::setItemSize(QSize(0, 0));
    } else {
        QFontMetrics fm(font());

        int size=AlbumsModel::iconSize();
        QSize grid(size+8, size+(fm.height()*2.5));
        view->setGridSize(grid);
        AlbumsModel::setItemSize(grid-QSize(4, 4));
    }
}

QStringList AlbumsPage::selectedFiles() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return AlbumsModel::self()->filenames(mapped);
}

QList<Song> AlbumsPage::selectedSongs() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return AlbumsModel::self()->songs(mapped);
}

void AlbumsPage::addSelectionToPlaylist(const QString &name)
{
    QStringList files=selectedFiles();

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
void AlbumsPage::addSelectionToDevice(const QString &udi)
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    QList<Song> songs=AlbumsModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void AlbumsPage::deleteSongs()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    QList<Song> songs=AlbumsModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        if (KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
            emit deleteSongs(QString(), songs);
        }
        view->clearSelection();
    }
}
#endif

void AlbumsPage::itemActivated(const QModelIndex &)
{
    if (1==view->selectedIndexes().size()) {//doubleclick should only have one selected item
        addSelectionToPlaylist();
    }
}

void AlbumsPage::searchItems()
{
    QString genre=0==genreCombo->currentIndex() ? QString() : genreCombo->currentText();
    QString filter=view->searchText().trimmed();

    if (filter.isEmpty() && genre.isEmpty()) {
        proxy.setFilterEnabled(false);
        proxy.setFilterGenre(genre);
        if (!proxy.filterRegExp().isEmpty()) {
             proxy.setFilterRegExp(QString());
        }
    } else {
        proxy.setFilterEnabled(true);
        proxy.setFilterGenre(genre);
        if (filter!=proxy.filterRegExp().pattern()) {
            proxy.setFilterRegExp(filter);
        }
    }
}

void AlbumsPage::updateGenres(const QStringList &genres)
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
