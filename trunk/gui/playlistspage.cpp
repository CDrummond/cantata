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

#include "playlistspage.h"
#include "mpdconnection.h"
#include "playlistsmodel.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QInputDialog>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#include <KMessageBox>
#else
#include <QtGui/QAction>
#include <QtGui/QMessageBox>
#endif

PlaylistsPage::PlaylistsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    removePlaylistAction = p->actionCollection()->addAction("removeplaylist");
    removePlaylistAction->setText(i18n("Remove Playlist"));
    renamePlaylistAction = p->actionCollection()->addAction("renameplaylist");
    renamePlaylistAction->setText(i18n("Rename Playlist"));
    #else
    removePlaylistAction = new QAction(tr("Remove Playlist"), this);
    renamePlaylistAction = new QAction(tr("Rename PlayList"), this);
    #endif
    removePlaylistAction->setIcon(QIcon::fromTheme("list-remove"));
    renamePlaylistAction->setIcon(QIcon::fromTheme("edit-rename"));

    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    delPlaylist->setDefaultAction(removePlaylistAction);
    renPlaylist->setDefaultAction(renamePlaylistAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), delPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), renPlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    delPlaylist->setAutoRaise(true);
    renPlaylist->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);
    delPlaylist->setEnabled(false);
    renPlaylist->setEnabled(false);

    view->setHeaderHidden(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(removePlaylistAction);
    view->addAction(renamePlaylistAction);

    proxy.setSourceModel(PlaylistsModel::self());
    view->setModel(&proxy);
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(this, SIGNAL(loadPlaylist(const QString &)), MPDConnection::self(), SLOT(load(const QString &)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(rm(const QString &)));
    connect(this, SIGNAL(savePlaylist(const QString &)), MPDConnection::self(), SLOT(save(const QString &)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(rename(const QString &, const QString &)));
    connect(removePlaylistAction, SIGNAL(activated()), this, SLOT(removePlaylist()));
    connect(p->savePlaylistAction, SIGNAL(activated()), this, SLOT(savePlaylist()));
    connect(renamePlaylistAction, SIGNAL(triggered()), this, SLOT(renamePlaylist()));
}

PlaylistsPage::~PlaylistsPage()
{
}

void PlaylistsPage::refresh()
{
    PlaylistsModel::self()->getPlaylists();
}

void PlaylistsPage::clear()
{
    PlaylistsModel::self()->clear();
}

void PlaylistsPage::addSelectionToPlaylist()
{
    const QModelIndexList items = view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        emit loadPlaylist(PlaylistsModel::self()->data(proxy.mapToSource(items.first()), Qt::DisplayRole).toString());
    }
}

void PlaylistsPage::removePlaylist()
{
    const QModelIndexList items = view->selectionModel()->selectedRows();

    if (items.size() != 1) {
        return;
    }

    QModelIndex sourceIndex = proxy.mapToSource(items.first());
    QString name = PlaylistsModel::self()->data(sourceIndex, Qt::DisplayRole).toString();

    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to remove <i>%1</i>?").arg(name), i18n("Delete Playlist?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::warning(this, tr("Delete Playlist?"), tr("Are you sure you wish to remove <i>%1</i>?").arg(name),
                                              QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
        return;
    }
    #endif

    emit removePlaylist(name);
}

void PlaylistsPage::savePlaylist()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString name = QInputDialog::getText(this, i18n("Playlist Name"), i18n("Enter a name for the playlist:"));
    #else
    QString name = QInputDialog::getText(this, tr("Playlist Name"), tr("Enter a name for the playlist:"));
    #endif

    if (!name.isEmpty())
        emit savePlaylist(name);
}

void PlaylistsPage::renamePlaylist()
{
    const QModelIndexList items = view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = proxy.mapToSource(items.first());
        QString name = PlaylistsModel::self()->data(sourceIndex, Qt::DisplayRole).toString();
        #ifdef ENABLE_KDE_SUPPORT
        QString newName = QInputDialog::getText(this, i18n("Rename Playlist"), i18n("Enter new name for playlist: %1").arg(name));
        #else
        QString newName = QInputDialog::getText(this, tr("Rename Playlist"), tr("Enter new name for playlist: %1").arg(name));
        #endif

        if (!newName.isEmpty()) {
            emit renamePlaylist(name, newName);
        }
    }
}

void PlaylistsPage::itemDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = proxy.mapToSource(index);
    QString name = PlaylistsModel::self()->data(sourceIndex, Qt::DisplayRole).toString();
    emit loadPlaylist(name);
}
