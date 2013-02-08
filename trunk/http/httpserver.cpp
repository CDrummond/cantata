/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "httpsocket.h"
#include "utils.h"
#include "tags.h"
#include "settings.h"
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QThread>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(HttpServer, instance)
#endif

HttpServer * HttpServer::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static HttpServer *instance=0;
    if(!instance) {
        instance=new HttpServer;
    }
    return instance;
    #endif
}

void HttpServer::stop()
{
    if (socket) {
        socket->terminate();
        socket=0;
    }

    if (thread) {
        Utils::stopThread(thread);
        thread=0;
    }
}

bool HttpServer::setDetails(const QString &addr, quint16 port)
{
    if (socket && port==socket->serverPort() && addr==socket->configuredAddress()) {
        return true;
    }

    if (socket) {
        socket->terminate();
        socket=0;
    }

    if (thread) {
        thread->quit();
        thread=0;
    }

    if (0!=port) {
        thread=new QThread(0);
        socket=new HttpSocket(addr, port);
        socket->moveToThread(thread);
        thread->start();
        return socket->isListening();
    } else {
        return true;
    }
}

bool HttpServer::isAlive() const
{
    return socket && socket->isListening();
}

QString HttpServer::address() const
{
    return socket ? QLatin1String("http://")+socket->address()+QChar(':')+QString::number(socket->serverPort())
                  : QLatin1String("http://127.0.0.1:")+QString::number(Settings::self()->httpPort());
}

bool HttpServer::isOurs(const QString &url) const
{
    return isAlive() ? url.startsWith(address()+"/") : false;
}

QByteArray HttpServer::encodeUrl(const Song &s) const
{
    if (!isAlive()) {
        return QByteArray();
    }
    #if QT_VERSION < 0x050000
    QUrl url;
    QUrl &query=url;
    #else
    QUrl url;
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost(socket->address());
    url.setPort(socket->serverPort());
    url.setPath(s.file);
    if (!s.album.isEmpty()) {
        query.addQueryItem("album", s.album);
    }
    if (!s.artist.isEmpty()) {
        query.addQueryItem("artist", s.artist);
    }
    if (!s.albumartist.isEmpty()) {
        query.addQueryItem("albumartist", s.albumartist);
    }
    if (!s.title.isEmpty()) {
        query.addQueryItem("title", s.title);
    }
    if (!s.genre.isEmpty()) {
        query.addQueryItem("genre", s.genre);
    }
    if (s.disc) {
        query.addQueryItem("disc", QString::number(s.disc));
    }
    if (s.year) {
        query.addQueryItem("year", QString::number(s.year));
    }
    if (s.time) {
        query.addQueryItem("time", QString::number(s.time));
    }
    if (s.track) {
        query.addQueryItem("track", QString::number(s.track));
    }
    query.addQueryItem("cantata", "song");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    return url.toEncoded();
}

QByteArray HttpServer::encodeUrl(const QString &file) const
{
    Song s=Tags::read(file);
    s.file=file;
    return encodeUrl(s);
}

Song HttpServer::decodeUrl(const QString &url) const
{
    Song s;
    #if QT_VERSION < 0x050000
    QUrl u(url);
    QUrl &q=u;
    #else
    QUrl u(url);
    QUrlQuery q(u); 
    #endif

    if (q.hasQueryItem("cantata") && q.queryItemValue("cantata")=="song") {
        if (q.hasQueryItem("album")) {
            s.album=q.queryItemValue("album");
        }
        if (q.hasQueryItem("artist")) {
            s.artist=q.queryItemValue("artist");
        }
        if (q.hasQueryItem("albumartist")) {
            s.albumartist=q.queryItemValue("albumartist");
        }
        if (q.hasQueryItem("title")) {
            s.title=q.queryItemValue("title");
        }
        if (q.hasQueryItem("genre")) {
            s.genre=q.queryItemValue("genre");
        }
        if (q.hasQueryItem("disc")) {
            s.disc=q.queryItemValue("disc").toInt();
        }
        if (q.hasQueryItem("year")) {
            s.year=q.queryItemValue("year").toInt();
        }
        if (q.hasQueryItem("time")) {
            s.time=q.queryItemValue("time").toInt();
        }
        if (q.hasQueryItem("track")) {
            s.track=q.queryItemValue("track").toInt();
        }
        s.file=u.path();
    }

    return s;
}
