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

#ifndef PLAYLISTSPAGE_H
#define PLAYLISTSPAGE_H

#include "ui_playlistspage.h"
#include "mainwindow.h"
#include "playlistsproxymodel.h"

class PlaylistsPage : public QWidget, public Ui::PlaylistsPage
{
    Q_OBJECT
public:
    PlaylistsPage(MainWindow *p);
    virtual ~PlaylistsPage();

    void refresh();
    void clear();
    void addSelectionToPlaylist();
    void setView(bool tree) { view->setMode(tree ? ItemView::Mode_Tree : ItemView::Mode_List); }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void loadPlaylist(QString name);
    void removePlaylist(const QString &name);
    void savePlaylist(const QString &name);
    void renamePlaylist(const QString &oldname, const QString &newname);
    void removeFromPlaylist(const QString &name, const QList<int> &positions);

    void add(const QStringList &files);

private:
    void addItemsToPlayQueue(const QModelIndexList &indexes);

public Q_SLOTS:
    void removeItems();

private Q_SLOTS:
    void savePlaylist();
    void renamePlaylist();
    void itemDoubleClicked(const QModelIndex &index);
    void selectionChanged();
    void searchItems();

private:
    Action *renamePlaylistAction;
    PlaylistsProxyModel proxy;
};

#endif
