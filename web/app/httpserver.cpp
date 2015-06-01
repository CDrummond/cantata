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

#include "httpserver.h"
#include "libraryapi.h"
#include "streamsapi.h"
#include "playlistsapi.h"
#include "playqueueapi.h"
#include "coversapi.h"
#include "statusapi.h"
#include "http/staticfilecontroller.h"
#include <QDebug>

#define DBUG if (BasicHttpServer::debugEnabled()) qWarning() << metaObject()->className() << (void *)this << __FUNCTION__

HttpServer::HttpServer(QObject *p, const QSettings *set)
    : BasicHttpServer(p, set)
    , covers(0)
{
}

HttpServer::~HttpServer()
{
}

HttpRequestHandler * HttpServer::getHandler(const QString &path)
{
    if (path.startsWith("/api/")) {
        if (path.startsWith("/api/v1/library")) {
            return new LibraryApi;
        } else if (path.startsWith("/api/v1/streams")) {
            return new StreamsApi;
        } else if (path.startsWith("/api/v1/playlists")) {
            return new PlaylistsApi;
        } else if (path.startsWith("/api/v1/playqueue")) {
            return new PlayQueueApi;
        } else if (path.startsWith("/api/v1/covers")) {
            if (!covers) {
                covers=new CoversApi(settings());
            }
            return covers;
        } else if (path.startsWith("/api/v1/status")) {
            return StatusApi::self();
        }
        return 0;
    }
    return BasicHttpServer::getHandler(path);
}

void HttpServer::handlerFinished(HttpRequestHandler *handler)
{
    if (handler==covers || handler==StatusApi::self()) {
        return;
    }
    ApiHandler *h=dynamic_cast<ApiHandler *>(handler);
    if (h) {
        DBUG << (void *)h;
        h->deleteLater();
    } else {
        BasicHttpServer::handlerFinished(handler);
    }
}
