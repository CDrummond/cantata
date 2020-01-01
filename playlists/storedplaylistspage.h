/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef STORED_PAGE_H
#define STORED_PAGE_H

#include "widgets/singlepagewidget.h"
#include "widgets/multipagewidget.h"
#include "models/playlistsproxymodel.h"

class Action;

class StoredPlaylistsPage : public SinglePageWidget
{
    Q_OBJECT
public:
    StoredPlaylistsPage(QWidget *p);
    ~StoredPlaylistsPage() override;

    void updateRows();
    void clear();
    //QStringList selectedFiles() const;
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    void setView(int mode) override;
    #ifdef ENABLE_DEVICES_SUPPORT
    QList<Song> selectedSongs(bool allowPlaylists=false) const override;
    void addSelectionToDevice(const QString &udi) override;
    #endif

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void loadPlaylist(const QString &name, bool replace);
    void removePlaylist(const QString &name);
    void savePlaylist(const QString &name, bool overwrite);
    void renamePlaylist(const QString &oldname, const QString &newname);
    void removeFromPlaylist(const QString &name, const QList<quint32> &positions);

    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);

private:
    void addItemsToPlayList(const QModelIndexList &indexes, const QString &name, int action, quint8 priority=0, bool decreasePriority=false);

public Q_SLOTS:
    void removeItems() override;

private Q_SLOTS:
    void savePlaylist();
    void renamePlaylist();
    void removeDuplicates();
    void removeInvalid();
    void itemDoubleClicked(const QModelIndex &index);
    void updated(const QModelIndex &index);
    void headerClicked(int level);
    void setStartClosed(bool sc);
    void updateToPlayQueue(const QModelIndex &idx, bool replace);

private:
    void doSearch() override;
    void controlActions() override;

private:
    QString lastPlaylist;
    Action *renamePlaylistAction;
    Action *removeDuplicatesAction;
    Action *removeInvalidAction;
    Action *intitiallyCollapseAction;
    PlaylistsProxyModel proxy;
};

#endif
