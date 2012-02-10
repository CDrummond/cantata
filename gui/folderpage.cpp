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

#include "folderpage.h"
#include "mpdconnection.h"
#include "musiclibrarymodel.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KMessageBox>
#else
#include <QtGui/QAction>
#endif

FolderPage::FolderPage(MainWindow *p)
    : QWidget(p)
    , enabled(false)
    , mw(p)
{
    setupUi(this);
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->refreshAction);

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);

    #ifdef ENABLE_KDE_SUPPORT
    view->setTopText(i18n("Folders"));
    #else
    view->setTopText(tr("Folders"));
    #endif
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

    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->init(p->replacePlaylistAction, p->addToPlaylistAction);
    connect(this, SIGNAL(listAll()), MPDConnection::self(), SLOT(listAll()));
    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
}

FolderPage::~FolderPage()
{
}

void FolderPage::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;

    if (enabled) {
        connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *)), &model, SLOT(updateDirView(DirViewItemRoot *)));
        connect(MPDConnection::self(), SIGNAL(updatingFileList()), view, SLOT(showSpinner()));
        connect(MPDConnection::self(), SIGNAL(updatedFileList()), view, SLOT(hideSpinner()));
        refresh();
    } else {
        disconnect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *)), &model, SLOT(updateDirView(DirViewItemRoot *)));
        disconnect(MPDConnection::self(), SIGNAL(updatingFileList()), view, SLOT(showSpinner()));
        disconnect(MPDConnection::self(), SIGNAL(updatedFileList()), view, SLOT(hideSpinner()));
    }
}

void FolderPage::refresh()
{
    if (enabled) {
        view->showSpinner();
        emit listAll();
    }
}

void FolderPage::clear()
{
    model.clear();
}

void FolderPage::searchItems()
{
    QString filter=view->searchText().trimmed();

    if (filter.isEmpty() ) {
        proxy.setFilterEnabled(false);
        if (!proxy.filterRegExp().isEmpty()) {
             proxy.setFilterRegExp(QString());
        }
    } else if (filter!=proxy.filterRegExp().pattern()) {
        proxy.setFilterEnabled(true);
        proxy.setFilterRegExp(filter);
    }
}

void FolderPage::controlActions()
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

void FolderPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view->selectedIndexes();
    if (1!=selected.size()) {
        return; //doubleclick should only have one selected item
    }

    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(selected.at(0)).internalPointer());
    if (DirViewItem::Type_File==item->type()) {
        addSelectionToPlaylist();
    }
}

QList<Song> FolderPage::selectedSongs() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QList<Song>();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return MusicLibraryModel::self()->songs(model.filenames(mapped));
}

QStringList FolderPage::selectedFiles() const
{
    const QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return QStringList();
    }

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, selected) {
        mapped.append(proxy.mapToSource(idx));
    }

    return model.filenames(mapped);
}

void FolderPage::addSelectionToPlaylist(const QString &name)
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
void FolderPage::addSelectionToDevice(const QString &udi)
{
    QList<Song> songs=selectedSongs();

    if (!songs.isEmpty()) {
        emit addToDevice(QString(), udi, songs);
        view->clearSelection();
    }
}

void FolderPage::deleteSongs()
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

QStringList FolderPage::walk(QModelIndex rootItem)
{
    QStringList files;
    DirViewItem *item = static_cast<DirViewItem *>(proxy.mapToSource(rootItem).internalPointer());

    if (DirViewItem::Type_File==item->type()) {
        return QStringList(item->fullName());
    }

    for (int i = 0; ; i++) {
        QModelIndex current = rootItem.child(i, 0);
        if (!current.isValid())
            return files;

        QStringList tmp = walk(current);
        for (int j = 0; j < tmp.size(); j++) {
            if (!files.contains(tmp.at(j)))
                files << tmp.at(j);
        }
    }
    return files;
}
