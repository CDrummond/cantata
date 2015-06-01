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

#include "playlistsapi.h"
#include "mpd-interface/playlist.h"
#include "mpd-interface/mpdconnection.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "httpserver.h"
#include <QDebug>

#define DBUG if (ApiHandler::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

static QVariant toVariant(const QList<Playlist> &playlists)
{
    QVariantList list;
    foreach (const Playlist &playlist, playlists) {
        list.append(playlist.name);
    }
    QVariantMap map;
    map["playlists"]=list;
    map["count"]=list.size();
    return map;
}

QVariant toVariant(const QList<Song> &songs)
{
    QVariantList list;
    int duration=0;
    foreach (const Song &song, songs) {
        QVariantMap map;
        map["url"]=song.file;
        map["name"]=song.artistSong();
        map["duration"]=song.time;
        duration+=song.time;
        list.append(map);
    }
    QVariantMap map;
    map["tracks"]=list;
    map["count"]=list.size();
    map["duration"]=duration;
    return map;
}

PlaylistsApi::PlaylistsApi(QObject *p)
    : ApiHandler(p)
{
    connect(this, SIGNAL(listPlaylists()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(this, SIGNAL(playlistInfo(QString)), MPDConnection::self(), SLOT(playlistInfo(QString)));
    connect(this, SIGNAL(removePlaylist(QString)), MPDConnection::self(), SLOT(removePlaylist(QString)));
    connect(this, SIGNAL(removeFromPlaylist(QString,QList<quint32>)), MPDConnection::self(), SLOT(removeFromPlaylist(QString,QList<quint32>)));
    connect(MPDConnection::self(), SIGNAL(playlistsRetrieved(QList<Playlist>)), this, SLOT(storedPlaylists(QList<Playlist>)));
    connect(MPDConnection::self(), SIGNAL(playlistInfoRetrieved(QString,QList<Song>)), this, SLOT(playlistInfoRetrieved(QString,QList<Song>)));
}

HttpRequestHandler::HandleStatus PlaylistsApi::handle(HttpRequest *request, HttpResponse *response)
{
    const QByteArray &path=request->path();
    if (HttpRequest::Method_Get==request->method()) {
        if (path=="/api/v1/playlists") {
            emit listPlaylists();
            awaitResponse(response);
            return Status_Handling;
        } else if (path=="/api/v1/playlists/tracks") {
            plName=request->parameter("name");
            emit playlistInfo(plName);
            awaitResponse(response);
            return Status_Handling;
        } else {
            return Status_BadRequest;
        }
    } else if (HttpRequest::Method_Post==request->method() && path=="/api/v1/playlists") {
//        DBUG << request->parameter("index");
    } else if (HttpRequest::Method_Delete==request->method()) {
        if (path=="/api/v1/playlists") {
            QString name=request->parameter("name");
            DBUG << "params" << name;
            emit removePlaylist(name);
        } else if (path.startsWith("/api/v1/playlists/")) {
            QString name=path.mid(18);
            int index=request->parameter("index").toInt();
            DBUG << "params" << name << index;
            emit removeFromPlaylist(name, QList<quint32>() << index);
        } else {
            return Status_BadRequest;
        }
    } else {
        return Status_BadRequest;
    }
    return Status_Handled;
}

void PlaylistsApi::storedPlaylists(const QList<Playlist> &playlists)
{
    if (awaitingResponse()) {
        responseReceived(toVariant(playlists));
    }
}

void PlaylistsApi::playlistInfoRetrieved(const QString &name, const QList<Song> &songs)
{
    if (awaitingResponse() && name==plName) {
        responseReceived(toVariant(songs));
    }
}
