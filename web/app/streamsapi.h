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

#ifndef STREAMS_API_H
#define STREAMS_API_H

#include "apihandler.h"
#include "mpd-interface/stream.h"

class HttpRequest;
class HttpResponse;

class StreamsApi : public ApiHandler
{
    Q_OBJECT

public:
    StreamsApi(QObject *p=0);
    HandleStatus handle(HttpRequest *request, HttpResponse *response);

Q_SIGNALS:
    void listStreams();
    void removeStreams(const QList<quint32> &rows);

private Q_SLOTS:
    void streamList(const QList<Stream> &streams);
    void removedStreams(const QList<quint32> &rows);
};

#endif
