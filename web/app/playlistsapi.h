/*
 * Cantata Web
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef PLAYLISTS_API_H
#define PLAYLISTS_API_H

#include "apihandler.h"

class HttpRequest;
class HttpResponse;
struct Playlist;
struct Song;

class PlaylistsApi : public ApiHandler
{
    Q_OBJECT

public:
    PlaylistsApi(QObject *p=0);
    HandleStatus handle(HttpRequest *request, HttpResponse *response);

Q_SIGNALS:
    void listPlaylists();
    void playlistInfo(const QString &pl);
    void removePlaylist(const QString &name);
    void removeFromPlaylist(const QString &name, const QList<quint32> &positions);

private Q_SLOTS:
    void storedPlaylists(const QList<Playlist> &playlists);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);

private:
    QString plName;
};

#endif
