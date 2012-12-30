/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtCore/QUrl>
#include <QtCore/QThread>
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
        thread->deleteLater();
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
    QUrl url;
    url.setScheme("http");
    url.setHost(socket->address());
    url.setPort(socket->serverPort());
    url.setPath(s.file);
    if (!s.album.isEmpty()) {
        url.addQueryItem("album", s.album);
    }
    if (!s.artist.isEmpty()) {
        url.addQueryItem("artist", s.artist);
    }
    if (!s.albumartist.isEmpty()) {
        url.addQueryItem("albumartist", s.albumartist);
    }
    if (!s.title.isEmpty()) {
        url.addQueryItem("title", s.title);
    }
    if (!s.genre.isEmpty()) {
        url.addQueryItem("genre", s.genre);
    }
    if (s.disc) {
        url.addQueryItem("disc", QString::number(s.disc));
    }
    if (s.year) {
        url.addQueryItem("year", QString::number(s.year));
    }
    if (s.time) {
        url.addQueryItem("time", QString::number(s.time));
    }
    if (s.track) {
        url.addQueryItem("track", QString::number(s.track));
    }
    if (!s.file.isEmpty()) {
        url.addQueryItem("file", s.file);
    }
    url.addQueryItem("cantata", "song");
    return url.toEncoded();
}

QByteArray HttpServer::encodeUrl(const QString &file) const
{
    Song s=Tags::read(file);
    s.fillEmptyFields();
    return encodeUrl(s);
}

Song HttpServer::decodeUrl(const QString &url) const
{
    Song s;
    QUrl u(url);

    if (u.hasQueryItem("cantata") && u.queryItemValue("cantata")=="song") {
        if (u.hasQueryItem("album")) {
            s.album=u.queryItemValue("album");
        }
        if (u.hasQueryItem("artist")) {
            s.artist=u.queryItemValue("artist");
        }
        if (u.hasQueryItem("albumartist")) {
            s.albumartist=u.queryItemValue("albumartist");
        }
        if (u.hasQueryItem("title")) {
            s.title=u.queryItemValue("title");
        }
        if (u.hasQueryItem("genre")) {
            s.genre=u.queryItemValue("genre");
        }
        if (u.hasQueryItem("disc")) {
            s.disc=u.queryItemValue("disc").toInt();
        }
        if (u.hasQueryItem("year")) {
            s.year=u.queryItemValue("year").toInt();
        }
        if (u.hasQueryItem("time")) {
            s.time=u.queryItemValue("time").toInt();
        }
        if (u.hasQueryItem("track")) {
            s.track=u.queryItemValue("track").toInt();
        }
        if (u.hasQueryItem("file")) {
            s.file=u.queryItemValue("file");
        } else {
            s.file=u.path();
        }
    }

    return s;
}
