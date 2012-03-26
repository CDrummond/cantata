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
    , mw(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->refreshAction);

    MainWindow::initButton(addToPlaylist);
    MainWindow::initButton(replacePlaylist);
    MainWindow::initButton(libraryUpdate);

    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Albums"));
    #else
    view->setTopText(tr("Albums"));
    #endif
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(p->addToStoredPlaylistAction);
//     view->addAction(p->burnAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    view->addAction(p->copyToDeviceAction);
    view->addAction(p->organiseFilesAction);
    #endif
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

    proxy.setSourceModel(AlbumsModel::self());
    view->setModel(&proxy);
    view->init(p->replacePlaylistAction, p->addToPlaylistAction);

    connect(this, SIGNAL(add(const QStringList &, bool)), MPDConnection::self(), SLOT(add(const QStringList &, bool)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(MPDConnection::self(), SIGNAL(updatingLibrary()), view, SLOT(showSpinner()));
    connect(MPDConnection::self(), SIGNAL(updatedLibrary()), view, SLOT(hideSpinner()));
    updateGenres(QSet<QString>());
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

void AlbumsPage::addSelectionToPlaylist(const QString &name, bool replace)
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
    proxy.update(view->searchText().trimmed(), genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
}

void AlbumsPage::controlActions()
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
    mw->editTagsAction->setEnabled(enable);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    mw->replaygainAction->setEnabled(enable);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    mw->deleteSongsAction->setEnabled(enable);
    #endif
}

void AlbumsPage::updateGenres(const QSet<QString> &g)
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
