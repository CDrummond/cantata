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
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    delPlaylist->setDefaultAction(p->removePlaylistAction);
//     renamePlaylist->setDefaultAction(p->renamePlaylistAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), delPlaylist, SLOT(setEnabled(bool)));
//     connect(view, SIGNAL(itemsSelected(bool)), renamePlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    delPlaylist->setAutoRaise(true);
//     renamePlaylist->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);
    delPlaylist->setEnabled(false);
//     renamePlaylist->setEnabled(false);

// #ifdef ENABLE_KDE_SUPPORT
//     search->setPlaceholderText(i18n("Search playlists..."));
// #else
//     search->setPlaceholderText(tr("Search playlists..."));
// #endif
    view->setHeaderHidden(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlaylistAction);
    view->addAction(p->removePlaylistAction);
    view->addAction(p->renamePlaylistAction);

    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(this, SIGNAL(loadPlaylist(const QString &)), MPDConnection::self(), SLOT(load(const QString &)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(rm(const QString &)));
    connect(this, SIGNAL(savePlaylist(const QString &)), MPDConnection::self(), SLOT(save(const QString &)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(rename(const QString &, const QString &)));
    connect(p->removePlaylistAction, SIGNAL(activated()), this, SLOT(removePlaylist()));
    connect(p->savePlaylistAction, SIGNAL(activated()), this, SLOT(savePlaylist()));
    connect(p->renamePlaylistAction, SIGNAL(triggered()), this, SLOT(renamePlaylist()));
}

PlaylistsPage::~PlaylistsPage()
{
}

void PlaylistsPage::refresh()
{
    model.getPlaylists();
}

void PlaylistsPage::clear()
{
    model.clear();
}

void PlaylistsPage::addSelectionToPlaylist()
{
    const QModelIndexList items = view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        emit loadPlaylist(model.data(proxy.mapToSource(items.first()), Qt::DisplayRole).toString());
    }
}

void PlaylistsPage::removePlaylist()
{
    const QModelIndexList items = view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = proxy.mapToSource(items.first());
        QString name = model.data(sourceIndex, Qt::DisplayRole).toString();

#ifdef ENABLE_KDE_SUPPORT
        if (KMessageBox::Yes == KMessageBox::warningYesNo(this, i18n("Are you sure you want to delete playlist: %1?", name),
                i18n("Delete Playlist?"))) {
            emit removePlaylist(name);
        }
#else
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Delete Playlist?"));
        msgBox.setText(tr("Are you sure you want to delete playlist: %1?").arg(name));
        msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::No);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Yes) {
            emit removePlaylist(name);
        }
#endif
    }
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
        QString name = model.data(sourceIndex, Qt::DisplayRole).toString();
#ifdef ENABLE_KDE_SUPPORT
        QString new_name = QInputDialog::getText(this, i18n("Rename Playlist"), i18n("Enter new name for playlist: %1").arg(name));
#else
        QString new_name = QInputDialog::getText(this, tr("Rename Playlist"), tr("Enter new name for playlist: %1").arg(name));
#endif

        if (!new_name.isEmpty()) {
            emit renamePlaylist(name, new_name);
        }
    }
}

void PlaylistsPage::itemDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = proxy.mapToSource(index);
    QString name = model.data(sourceIndex, Qt::DisplayRole).toString();
    emit loadPlaylist(name);
}
