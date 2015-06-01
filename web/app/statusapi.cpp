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

#include "statusapi.h"
#include "apihandler.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "support/globalstatic.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/song.h"
#include "support/localize.h"
#include <QByteArray>
#include <QWebSocket>
#include <QJsonDocument>
#include <QDebug>

#define DBUG if (ApiHandler::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

GLOBAL_STATIC(StatusApi, instance)

static QByteArray encode(const char *key, int val, bool last=false)
{
    return QByteArray("\"")+key+QByteArray("\":")+QByteArray::number(val)+QByteArray(last ? "" : ",");
}

static QByteArray encode(const char *key, bool val, bool last=false)
{
    return QByteArray("\"")+key+QByteArray("\":")+QByteArray(val ? "true" : "false")+QByteArray(last ? "" : ",");
}

static QByteArray encode(const char *key, MPDState val, bool last=false)
{
    QByteArray rv=QByteArray("\"")+key+QByteArray("\":\"");

    switch (val) {
    case MPDState_Inactive: rv+="inactive"; break;
    case MPDState_Playing:  rv+="playing"; break;
    case MPDState_Stopped:  rv+="stopped"; break;
    case MPDState_Paused:   rv+="paused"; break;
    }

    rv+=QByteArray(last ? "\"" : "\",");
    return rv;
}

static bool diff(const MPDStatusValues &a, const MPDStatusValues &b)
{
    return a.volume!=b.volume || a.consume!=b.consume || a.repeat!=b.repeat || a.single!=b.single || a.random!=b.random ||
           a.playlist!=b.playlist || a.playlistLength!=b.playlistLength || a.state!=b.state ||
           a.timeElapsed!=b.timeElapsed || a.timeTotal!=b.timeTotal;
}

StatusApi::StatusApi()
    : QWebSocketServer("Cantata", NonSecureMode)
{
    connect(this, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));
    connect(MPDConnection::self(), SIGNAL(statusUpdated(MPDStatusValues)), this, SLOT(statusUpdated(MPDStatusValues)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(Song)), this, SLOT(currentSongUpdated(Song)));
    connect(this, SIGNAL(currentSong()), MPDConnection::self(), SLOT(currentSong()));
    connect(this, SIGNAL(setConsume(bool)), MPDConnection::self(), SLOT(setConsume(bool)));
    connect(this, SIGNAL(setRandom(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(this, SIGNAL(setRepeat(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(this, SIGNAL(setSingle(bool)), MPDConnection::self(), SLOT(setSingle(bool)));
    listen();
}

static inline bool isSet(const QString &v) { return QLatin1String("1")==v || QLatin1String("true")==v; }

HttpRequestHandler::HandleStatus StatusApi::handle(HttpRequest *request, HttpResponse *response)
{
    if (HttpRequest::Method_Get==request->method()) {
        if (request->path()=="/api/v1/status/socket") {
            response->write("{\"port\":"+QByteArray::number(serverPort())+"}");
            return Status_Handled;
        } else if (request->path()=="/api/v1/status") {
            response->write(statusMessage());
            return Status_Handled;
        } else if (request->path()=="/api/v1/status/current") {
            response->write(currentSongMessage());
            return Status_Handled;
        }
    } else if(HttpRequest::Method_Post==request->method() && request->path()=="/api/v1/status") {
        QString repeat=request->parameter("repeat");
        QString random=request->parameter("random");
        QString single=request->parameter("single");
        QString consume=request->parameter("consume");
        DBUG << repeat << random << single << consume;
        if (!repeat.isEmpty()) {
            emit setRepeat(isSet(repeat));
        }
        if (!random.isEmpty()) {
            emit setRandom(isSet(random));
        }
        if (!single.isEmpty()) {
            emit setSingle(isSet(single));
        }
        if (!consume.isEmpty()) {
            emit setConsume(isSet(consume));
        }
        return Status_Handled;
    }
    return Status_BadRequest;
}

void StatusApi::slotNewConnection()
{
    while (hasPendingConnections()) {
        QWebSocket *sock=nextPendingConnection();
        DBUG << (void *)sock;
        connect(sock, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
        clients.append(sock);
        sock->sendTextMessage(statusMessage());
        sock->sendTextMessage(currentSongMessage());
    }
}

void StatusApi::clientDisconnected()
{
    QWebSocket *sock=qobject_cast<QWebSocket *>(sender());
    if (!sock) {
        return;
    }
    DBUG << (void *)sock;
    sock->deleteLater();
    clients.removeAll(sock);
}

void StatusApi::statusUpdated(const MPDStatusValues &updatedStatus)
{
    DBUG << clients.count();
    if (diff(status, updatedStatus)) {
        if (status.playlist!=updatedStatus.playlist) {
            // Clear playqueue album keys
            Song::clearKeyStore(MPDParseUtils::Loc_PlayQueue);
        }
        if (status.state!=updatedStatus.state || status.songId!=updatedStatus.songId) {
            DBUG << "Get current song";
            emit currentSong();
        }
        status=updatedStatus;
        QString message=statusMessage();
        foreach (QWebSocket *client, clients) {
            client->sendTextMessage(message);
        }
    }
}

void StatusApi::currentSongUpdated(const Song &s)
{
    QString name=s.name();

    if (s.isEmpty()) {
        song.track=QString();
        song.artist=QString();
    } else if (s.isStream() && !s.isCantataStream() && !s.isCdda()) {
        song.track=name.isEmpty() ? Song::unknown() : name;
        if (s.artist.isEmpty() && s.title.isEmpty() && !name.isEmpty()) {
            song.artist=i18n("(Stream)");
        } else {
            song.artist=s.artist.isEmpty() ? s.title : s.artistSong();
        }
    } else {
        if (s.title.isEmpty() && s.artist.isEmpty() && (!name.isEmpty() || !s.file.isEmpty())) {
            song.track=name.isEmpty() ? s.file : name;
        } else {
            song.track=s.title;
        }
        if (s.album.isEmpty() && s.artist.isEmpty()) {
            song.artist=song.track.isEmpty() ? QString() : Song::unknown();
        } else if (s.album.isEmpty()) {
            song.artist=(s.artist);
        } else {
            song.artist=s.artist+QLatin1String(" - ")+s.displayAlbum();
        }
    }
    song.id=s.id;
    DBUG << song.track << song.artist << song.duration << song.id;
    QString message=currentSongMessage();
    foreach (QWebSocket *client, clients) {
        client->sendTextMessage(message);
    }
}

QByteArray StatusApi::statusMessage() const
{
    return QByteArray("{"+
                    encode("volume", (int)status.volume)+
                    encode("consume", status.consume)+
                    encode("repeat", status.repeat)+
                    encode("single", status.single)+
                    encode("random", status.random)+
                    encode("playlist", (int)status.playlist)+
                    encode("playlistLength", (int)status.playlistLength)+
                    encode("state", status.state)+
                    encode("timeElapsed", (int)status.timeElapsed)+
                    encode("timeTotal", (int)status.timeTotal, true)+"}");
}

QByteArray StatusApi::currentSongMessage() const
{
    QVariantMap map;
    map["track"]=song.track;
    map["artist"]=song.artist;
    map["duration"]=song.duration;
    map["id"]=song.id;
    return QJsonDocument::fromVariant(map).toJson();
}
