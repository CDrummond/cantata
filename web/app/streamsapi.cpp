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

#include "streamsapi.h"
#include "mpd-interface/mpdconnection.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "httpserver.h"
#include <QDebug>

#define DBUG if (ApiHandler::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

static QVariant toVariant(const QList<Stream> &streams)
{
    QVariantList list;
    foreach (const Stream &stream, streams) {
        QVariantMap map;
        map["name"]=stream.name;
        map["url"]=stream.url;
        list.append(map);
    }
    QVariantMap map;
    map["streams"]=list;
    map["count"]=list.size();
    return map;
}

StreamsApi::StreamsApi(QObject *p)
    : ApiHandler(p)
{
    connect(this, SIGNAL(listStreams()), MPDConnection::self(), SLOT(listStreams()));
    connect(MPDConnection::self(), SIGNAL(streamList(QList<Stream>)), this, SLOT(streamList(QList<Stream>)));
    connect(this, SIGNAL(removeStreams(QList<quint32>)), MPDConnection::self(), SLOT(removeStreams(QList<quint32>)));
    connect(MPDConnection::self(), SIGNAL(removedStreams(QList<quint32>)), this, SLOT(removedStreams(QList<quint32>)));
}

HttpRequestHandler::HandleStatus StreamsApi::handle(HttpRequest *request, HttpResponse *response)
{
    const QByteArray &path=request->path();
    if (HttpRequest::Method_Get==request->method() && path=="/api/v1/streams") {
        emit listStreams();
        awaitResponse(response);
        return Status_Handling;
    } else if (HttpRequest::Method_Post==request->method() && path=="/api/v1/streams") {
        // TODO: Add stream - check for failures???
    } else if (HttpRequest::Method_Delete==request->method() && path=="/api/v1/streams") {
        int index=request->parameter("index").toInt();
        DBUG << "params" << index;
        emit removeStreams(QList<quint32>() << index);
        awaitResponse(response);
        return Status_Handling;
    }  else {
        return Status_BadRequest;
    }
    return Status_Handled;
}

void StreamsApi::streamList(const QList<Stream> &streams)
{
    if (awaitingResponse()) {
        responseReceived(toVariant(streams));
    }
}

void StreamsApi::removedStreams(const QList<quint32> &rows)
{
    Q_UNUSED(rows)
    if (awaitingResponse()) {
        responseReceived(true);
    }
}
