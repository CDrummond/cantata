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

#ifndef STATUS_API_H
#define STATUS_API_H

#include "http/httprequesthandler.h"
#include "mpd-interface/mpdstatus.h"
#include <QWebSocketServer>

class HttpRequest;
class HttpResponse;
struct Song;

class StatusApi : public QWebSocketServer, public HttpRequestHandler
{
    Q_OBJECT
public:
    static StatusApi *self();

    StatusApi();
    HandleStatus handle(HttpRequest *request, HttpResponse *response);

Q_SIGNALS:
    void currentSong();
    void setRepeat(bool);
    void setRandom(bool);
    void setSingle(bool);
    void setConsume(bool);

private Q_SLOTS:
    void slotNewConnection();
    void clientDisconnected();
    void statusUpdated(const MPDStatusValues &updatedStatus);
    void currentSongUpdated(const Song &s);

private:
    QByteArray statusMessage() const;
    QByteArray currentSongMessage() const;

private:
    struct SongDetails
    {
        QString track;
        QString artist;
        int id;
        int duration;
    };

    QList<QWebSocket *> clients;
    MPDStatusValues status;
    SongDetails song;
};

#endif
