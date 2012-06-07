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
#include "localize.h"
#include "messagebox.h"
#include "settings.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KGlobalSettings>
#else
#include <QtGui/QAction>
#endif

LibraryPage::LibraryPage(MainWindow *p)
    : QWidget(p)
    , mw(p)
{
    setupUi(this);
    addToPlayQueue->setDefaultAction(p->addToPlayQueueAction);
    replacePlayQueue->setDefaultAction(p->replacePlayQueueAction);
    libraryUpdate->setDefaultAction(p->refreshAction);

    MainWindow::initButton(addToPlayQueue);
    MainWindow::initButton(replacePlayQueue);
    MainWindow::initButton(libraryUpdate);

    view->addAction(p->addToPlayQueueAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addToStoredPlaylistAction);
//     view->addAction(p->burnAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->copyToDeviceAction);
    #endif
    view->addAction(p->organiseFilesAction);
    view->addAction(p->editTagsAction);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    view->addAction(p->replaygainAction);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    view->addAction(sep);
    view->addAction(p->deleteSongsAction);
    #endif

    connect(this, SIGNAL(add(const QStringList &, bool)), MPDConnection::self(), SLOT(add(const QStringList &, bool)));
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
    view->setTopText(i18n("Library"));
    view->setModel(&proxy);
    view->init(p->replacePlayQueueAction, p->addToPlayQueueAction);
    updateGenres(QSet<QString>());
}

LibraryPage::~LibraryPage()
{
}

void LibraryPage::refresh()
{
    view->setLevel(0);

    if (!MusicLibraryModel::self()->fromXML(mw->getDbUpdate())) {
        view->showSpinner();
        emit listAllInfo(mw->getDbUpdate());
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
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QStringList();
    }
    qSort(selected);

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return MusicLibraryModel::self()->filenames(mapped);
}

QList<Song> LibraryPage::selectedSongs() const
{
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }
    qSort(selected);

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return MusicLibraryModel::self()->songs(mapped);
}

void LibraryPage::addSelectionToPlaylist(const QString &name, bool replace)
{
    QStringList files=selectedFiles();

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, replace);
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
        if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove the selected songs?\nThis cannot be undone."))) {
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
                if (ItemView::Mode_Tree==view->viewMode() || first) {
                    view->showIndex(p, first);
                }
                if (first) {
                    first=false;
                }
                if (ItemView::Mode_Tree!=view->viewMode()) {
                    return;
                }
            }
        }
    }
}

void LibraryPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes();
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

void LibraryPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    bool enable=selected.count()>0;

    mw->addToPlayQueueAction->setEnabled(enable);
    mw->replacePlayQueueAction->setEnabled(enable);
    mw->addToStoredPlaylistAction->setEnabled(enable);
    mw->organiseFilesAction->setEnabled(enable && Settings::self()->canReadMpdDir());
    mw->editTagsAction->setEnabled(mw->organiseFilesAction->isEnabled());
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(mw->organiseFilesAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->deleteSongsAction->setEnabled(mw->organiseFilesAction->isEnabled());
    mw->copyToDeviceAction->setEnabled(mw->organiseFilesAction->isEnabled());
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
    entries.prepend(i18n("All Genres"));

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
