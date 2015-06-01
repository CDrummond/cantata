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

#include "playqueueapi.h"
#include "db/mpdlibrarydb.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "httpserver.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"
#include "streams/streamfetcher.h"
#include "support/localize.h"
#include <QDebug>

#define DBUG if (ApiHandler::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

static QString streamText(const Song &song, const QString &trackTitle, bool useName=true)
{
    if (song.album.isEmpty() && song.albumArtist().isEmpty()) {
        QString songName=song.name();
        return song.title.isEmpty() && songName.isEmpty()
                ? song.file
                : !useName || songName.isEmpty()
                  ? song.title
                  : song.title.isEmpty()
                    ? songName
                    : (song.title + " - " + songName);
    } else if (!song.title.isEmpty() && !song.artist.isEmpty()) {
        QString name=song.name();
        return song.artist + " - " + (!useName || name.isEmpty()
                                      ? song.title
                                      : song.title.isEmpty()
                                        ? name
                                        : (song.title + " - " + name));
    } else {
        return trackTitle;
    }
}

static QVariant toVariant(const QList<Song> &songs)
{
    QVariantList list;
    int lastKey=Song::Null_Key;
    int lastAlbumIndex=0;
    int albumDuration=0;
    int totalDuration=0;
    for (int i=0; i<songs.count(); ++i) {
        Song song=songs.at(i);
        QVariantMap map;
        song.setKey(MPDParseUtils::Loc_PlayQueue);

        QString title;
        QString track;
        bool stream=song.isStandardStream();
        bool isEmpty=song.title.isEmpty() && song.artist.isEmpty() && !song.file.isEmpty();
        QString trackTitle=isEmpty
                ? song.file
                : song.diffArtist()
                  ? song.artistSong()
                  : song.title;

        if (0==i || lastKey!=song.key) {
            if (stream) {
                int nextKey=i+1<songs.count() ? Song(songs.at(i+1)).setKey(MPDParseUtils::Loc_PlayQueue) : Song::Null_Key;
                if (nextKey!=song.key && !song.name().isEmpty()) {
                    title=song.name();
                    track=streamText(song, trackTitle, false);
                } else {
                    title=song.isCdda() ? i18n("Audio CD") : i18n("Streams");
                    track=streamText(song, trackTitle);
                }
            } else if (isEmpty) {
                title=Song::unknown();
                track=trackTitle;
            } else if (song.album.isEmpty()) {
                title=song.artistOrComposer();
                track=song.trackAndTitleStr();
            } else {
                if (song.isFromOnlineService()) {
                    title=Song::displayAlbum(song.album, Song::albumYear(song));
                } else {
                    title=song.artistOrComposer()+QLatin1String(" - ")+Song::displayAlbum(song.album, Song::albumYear(song));
                }
                while (title.contains(") (")) {
                    title=title.replace(") (", ", ");
                }
                track=song.trackAndTitleStr();
            }
        } else {
            if (stream) {
                track=streamText(song, trackTitle);
            } else {
                track=song.trackAndTitleStr();
            }
        }
        if (!title.isEmpty()) {
            map["title"]=title;
            map["albumId"]=song.albumId();
            map["artistId"]=song.artistOrComposer();
        }
        map["track"]=track;

        map["duration"]=song.time;
        map["key"]=song.key;
        map["id"]=song.id;
        list.append(map);
        if (0!=i && lastKey!=song.key) {
            QVariantMap album=list[lastAlbumIndex].toMap();
            album["albumDuration"]=albumDuration;
            list[lastAlbumIndex]=album;
            lastAlbumIndex=i;
            albumDuration=0;
        }
        lastKey=song.key;
        totalDuration+=song.time;
        albumDuration+=song.time;
    }
    if (!list.isEmpty() && lastAlbumIndex!=songs.length()-1) {
        QVariantMap album=list[lastAlbumIndex].toMap();
        album["albumDuration"]=albumDuration;
        list[lastAlbumIndex]=album;
    }
    QVariantMap map;
    map["tracks"]=list;
    map["count"]=list.size();
    map["duration"]=totalDuration;
    return map;
}

PlayQueueApi::PlayQueueApi(QObject *p)
    : ApiHandler(p)
    , fetcher(0)
{
    connect(this, SIGNAL(add(QStringList,bool,quint8)), MPDConnection::self(), SLOT(add(QStringList,bool,quint8)));
    connect(this, SIGNAL(loadPlaylist(QString,bool)), MPDConnection::self(), SLOT(loadPlaylist(QString,bool)));
    connect(this, SIGNAL(playListInfo()), MPDConnection::self(), SLOT(playListInfo()));
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(this, SIGNAL(prev()), MPDConnection::self(), SLOT(goToPrevious()));
    connect(this, SIGNAL(play()), MPDConnection::self(), SLOT(play()));
    connect(this, SIGNAL(pause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(next()), MPDConnection::self(), SLOT(goToNext()));
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(QList<Song>,bool)), this, SLOT(playlistUpdated(QList<Song>,bool)));
}

HttpRequestHandler::HandleStatus PlayQueueApi::handle(HttpRequest *request, HttpResponse *response)
{
    const QByteArray &path=request->path();
    DBUG << path;
    if (HttpRequest::Method_Get==request->method() && path=="/api/v1/playqueue") {
        emit playListInfo();
        awaitResponse(response);
        return Status_Handling;
    } else if (HttpRequest::Method_Delete==request->method() && path=="/api/v1/playqueue") {
        emit clear();
    } else if (HttpRequest::Method_Post==request->method() && path=="/api/v1/playqueue") {
        QString url=request->parameter("url");
        bool play="true"==request->parameter("play");
        QStringList files;
        DBUG << "params" << url;
        if (url.isEmpty()) {
            QString genre=request->parameter("genre");
            QString artistId=request->parameter("artistId");
            QString albumId=request->parameter("albumId");
            QString sort=request->parameter("sort");
            DBUG << "other params" << artistId << albumId << genre << sort;
            QList<Song> songs=MpdLibraryDb::self()->getTracks(artistId, albumId, genre, sort);
            foreach (const Song &s, songs) {
                files.append(s.file);
            }
        } else {
            files << url;
        }
        bool isStream=!url.isEmpty() && request->parameter("stream")=="true";
        bool isPlaylist=!url.isEmpty() && request->parameter("playlist")=="true";

        if (isStream) {
            if (!fetcher) {
                fetcher=new StreamFetcher(this);
                connect(fetcher, SIGNAL(result(QStringList,int,bool,quint8)), this, SLOT(streamResult(QStringList,int,bool,quint8)));
                fetcher->get(files, 0, true, 0);
            }
            awaitResponse(response);
            return Status_Handling;
        } else if (isPlaylist) {
            if (!files.isEmpty()) {
                emit loadPlaylist(url, play);
            }
            setResponse(response, !files.isEmpty());
        } else {
            if (!files.isEmpty()) {
                emit add(files, play, 0);
            }
            setResponse(response, !files.isEmpty());
        }
    } else if (HttpRequest::Method_Post==request->method() && path=="/api/v1/playqueue/control") {
        QString cmd=request->parameter("cmd");
        if ("prev"==cmd) {
            emit prev();
        } else if ("play"==cmd) {
            emit play();
        } else if("pause"==cmd) {
            emit pause(true);
        } else if ("next"==cmd) {
            emit next();
        } else {
            return Status_BadRequest;
        }
    } else {
        return Status_BadRequest;
    }

    return Status_Handled;
}

void PlayQueueApi::streamResult(const QStringList &items, int insertRow, bool replace, quint8 priority)
{
    Q_UNUSED(insertRow)
    Q_UNUSED(replace)
    Q_UNUSED(priority)

    DBUG << awaitingResponse();
    if (awaitingResponse()) {
        emit add(items, true, 0);
        responseReceived(true);
    }
}

void PlayQueueApi::playlistUpdated(const QList<Song> &songs, bool isComplete)
{
    DBUG << isComplete << awaitingResponse();
    if (isComplete && awaitingResponse()) {
        responseReceived(toVariant(songs));
    }
}
