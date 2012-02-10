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

#include "librarypage.h"
#include "mpdconnection.h"
#include "covers.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "mainwindow.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KGlobalSettings>
#ifdef ENABLE_DEVICES_SUPPORT
#include <KDE/KMessageBox>
#endif
#else
#include <QtGui/QAction>
#endif

LibraryPage::LibraryPage(MainWindow *p)
    : QWidget(p)
    , mw(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->refreshAction);

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);

    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(p->addToStoredPlaylistAction);
//     view->addAction(p->burnAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->copyToDeviceAction);
    view->addAction(p->organiseFilesAction);
    #endif
    #ifdef TAGLIB_FOUND
    view->addAction(p->editTagsAction);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->deleteSongsAction);
    #endif

    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(MPDConnection::self(), SIGNAL(musicLibraryUpdated(MusicLibraryItemRoot *, QDateTime)),
            MusicLibraryModel::self(), SLOT(updateMusicLibrary(MusicLibraryItemRoot *, QDateTime)));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary()), view, SLOT(showSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), view, SLOT(hideSpinner()));
    connect(MPDConnection::self(), SIGNAL(databaseUpdated()), this, SLOT(databaseUpdated()));
    connect(MusicLibraryModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), this, SLOT(updateGenres(const QSet<QString> &)));
    connect(this, SIGNAL(listAllInfo(const QDateTime &)), MPDConnection::self(), SLOT(listAllInfo(const QDateTime &)));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    proxy.setSourceModel(MusicLibraryModel::self());
    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Library"));
    #else
    view->setTopText(tr("Library"));
    #endif
    view->setModel(&proxy);
    view->init(p->replacePlaylistAction, p->addToPlaylistAction);
    updateGenres(QSet<QString>());
}

LibraryPage::~LibraryPage()
{
}

void LibraryPage::refresh()
{
    view->setLevel(0);

    if (!MusicLibraryModel::self()->fromXML(MPDStats::self()->dbUpdate())) {
        view->showSpinner();
        emit listAllInfo(MPDStats::self()->dbUpdate());
    }
}

void LibraryPage::databaseUpdated()
{
//     refresh(RefreshFromCache);
}

void LibraryPage::clear()
{
    MusicLibraryModel::self()->clear();
    view->setLevel(0);
}

QStringList LibraryPage::selectedFiles() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return MusicLibraryModel::self()->filenames(mapped);
}

QList<Song> LibraryPage::selectedSongs() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return MusicLibraryModel::self()->songs(mapped);
}

void LibraryPage::addSelectionToPlaylist(const QString &name)
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
void LibraryPage::addSelectionToDevice(const QString &udi)
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    QList<Song> songs=MusicLibraryModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void LibraryPage::deleteSongs()
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    QList<Song> songs=MusicLibraryModel::self()->songs(mapped);

    if (!songs.isEmpty()) {
        if (KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
            emit deleteSongs(QString(), songs);
        }
        view->clearSelection();
    }
}
#endif

void LibraryPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes();
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }
    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
    if (MusicLibraryItem::Type_Song==item->type()) {
        addSelectionToPlaylist();
    }
}

void LibraryPage::searchItems()
{
    QString genre=0==genreCombo->currentIndex() ? QString() : genreCombo->currentText();
    QString filter=view->searchText().trimmed();

    if (filter.isEmpty() && genre.isEmpty()) {
        proxy.setFilterEnabled(false);
        proxy.setFilterGenre(genre);
        if (!proxy.filterRegExp().isEmpty()) {
             proxy.setFilterRegExp(QString());
        } else {
            proxy.invalidate();
        }
    } else {
        proxy.setFilterEnabled(true);
        proxy.setFilterGenre(genre);
        if (filter!=proxy.filterRegExp().pattern()) {
            proxy.setFilterRegExp(filter);
        } else {
            proxy.invalidate();
        }
    }
}

void LibraryPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool enable=selected.count()>0;

    mw->addToPlaylistAction->setEnabled(enable);
    mw->replacePlaylistAction->setEnabled(enable);
    mw->addToStoredPlaylistAction->setEnabled(enable);
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->copyToDeviceAction->setEnabled(enable);
    mw->organiseFilesAction->setEnabled(enable);
    #endif
    #ifdef TAGLIB_FOUND
    mw->editTagsAction->setEnabled(enable);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->deleteSongsAction->setEnabled(enable);
    #endif
}

void LibraryPage::updateGenres(const QSet<QString> &g)
{
    if (genreCombo->count() && g==genres) {
        return;
    }

    genres=g;
    QStringList entries=g.toList();
    qSort(entries);
    #ifdef ENABLE_KDE_SUPPORT
    entries.prepend(i18n("All Genres"));
    #else
    entries.prepend(tr("All Genres"));
    #endif

    QString currentFilter = genreCombo->currentIndex() ? genreCombo->currentText() : QString();

    genreCombo->clear();
    genreCombo->addItems(entries);
    if (0==genres.count()) {
        genreCombo->setCurrentIndex(0);
    } else {
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
