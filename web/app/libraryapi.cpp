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

#include "libraryapi.h"
#include "db/mpdlibrarydb.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "httpserver.h"
#include <QJsonDocument>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QDebug>

#define DBUG if (ApiHandler::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

static QVariant toVariant(const QList<LibraryDb::Genre> &genres)
{
    QVariantList list;
    foreach (const LibraryDb::Genre &genre, genres) {
        QVariantMap map;
        map["name"]=genre.name;
        map["artists"]=genre.artistCount;
        list.append(map);
    }
    QVariantMap map;
    map["genres"]=list;
    map["count"]=list.size();
    return map;
}

static QVariant toVariant(const QList<LibraryDb::Artist> &artists)
{
    QVariantList list;
    foreach (const LibraryDb::Artist &artist, artists) {
        QVariantMap map;
        map["id"]=artist.name;
        map["sort"]=artist.sort;
        map["albums"]=artist.albumCount;
        list.append(map);
    }
    QVariantMap map;
    map["artists"]=list;
    map["count"]=list.size();
    return map;
}

static QVariant toVariant(const QList<LibraryDb::Album> &albums)
{
    QVariantList list;
    foreach (const LibraryDb::Album &album, albums) {
        QVariantMap map;
        map["id"]=album.id;
        map["name"]=album.name;
//        map["sort"]=album.sort;
        map["year"]=album.year;
        map["tracks"]=album.trackCount;
        map["duration"]=album.duration;
        if (!album.artist.isEmpty()) {
            map["artist"]=album.artist;
        }
//        if (!album.artistSort.isEmpty()) {
//            map["artistSort"]=album.artistSort;
//        }
        list.append(map);
    }
    QVariantMap map;
    map["albums"]=list;
    map["count"]=list.size();
    return map;
}

static QVariant toVariant(const QList<Song> &songs)
{
    QVariantList list;
    foreach (const Song &song, songs) {
        QVariantMap map;
        map["url"]=song.file;
        map["name"]=song.displayTitle();
        map["track"]=song.track;
        map["disc"]=song.disc;
        map["duration"]=song.time;
        list.append(map);
    }
    QVariantMap map;
    map["tracks"]=list;
    map["count"]=list.size();
    return map;
}

LibraryApi::LibraryApi(QObject *p)
    : ApiHandler(p)
{
    DBUG;
}

LibraryApi::~LibraryApi()
{
    DBUG;
}

HttpRequestHandler::HandleStatus LibraryApi::handle(HttpRequest *request, HttpResponse *response)
{
    const QByteArray &path=request->path();

    if (HttpRequest::Method_Get==request->method()) {
        if (path=="/api/v1/library/genres") {
            jsonHeaders(response);
            response->write(QJsonDocument::fromVariant(toVariant(MpdLibraryDb::self()->getGenres())).toJson());
        } else if (path=="/api/v1/library/artists") {
            jsonHeaders(response);
            QString genre=request->parameter("genre");
            DBUG << "params" << genre;
            response->write(QJsonDocument::fromVariant(toVariant(MpdLibraryDb::self()->getArtists(genre))).toJson());
        } else if (path=="/api/v1/library/albums") {
            jsonHeaders(response);
            QString genre=request->parameter("genre");
            QString artistId=request->parameter("artistId");
            QString sort=request->parameter("sort");
            DBUG << "params" << artistId << genre << sort;
            response->write(QJsonDocument::fromVariant(toVariant(MpdLibraryDb::self()->getAlbums(artistId, genre, LibraryDb::toAlbumSort(sort)))).toJson());
        } else if (path=="/api/v1/library/tracks") {
            jsonHeaders(response);
            QString genre=request->parameter("genre");
            QString artistId=request->parameter("artistId");
            QString albumId=request->parameter("albumId");
            QString sort=request->parameter("sort");
            DBUG << "params" << artistId << albumId << genre << sort;
            response->write(QJsonDocument::fromVariant(toVariant(MpdLibraryDb::self()->getTracks(artistId, albumId, genre, LibraryDb::toAlbumSort(sort)))).toJson());
        } else {
            return Status_BadRequest;
        }
    } else {
        return Status_BadRequest;
    }
    return Status_Handled;
}
