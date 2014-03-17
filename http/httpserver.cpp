/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef TAGLIB_FOUND
#include "tags.h"
#endif
#include "settings.h"
#include "thread.h"
#include "globalstatic.h"
#include <QFile>
#include <QUrl>
#include <QTimer>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

#include <QDebug>
static bool debugIsEnabled=false;
#define DBUG if (debugIsEnabled) qWarning() << "HttpServer" << __FUNCTION__

#ifdef Q_OS_WIN
static inline QString fixWindowsPath(const QString &f)
{
    return f.length()>3 && f.startsWith(QLatin1Char('/')) && QLatin1Char(':')==f.at(2) ? f.mid(1) : f;
}
#endif

void HttpServer::enableDebug()
{
    debugIsEnabled=true;
}

bool HttpServer::debugEnabled()
{
    return debugIsEnabled;
}

GLOBAL_STATIC(HttpServer, instance)

#ifdef ENABLE_HTTP_SERVER
HttpServer::HttpServer()
    : QObject(0)
    , thread(0)
    , socket(0)
    , closeTimer(0)
{
    force=Settings::self()->alwaysUseHttp();
    connect(MPDConnection::self(), SIGNAL(socketAddress(QString)), this, SLOT(mpdAddress(QString)));
    connect(MPDConnection::self(), SIGNAL(cantataStreams(QList<Song>,bool)), this, SLOT(cantataStreams(QList<Song>,bool)));
    connect(MPDConnection::self(), SIGNAL(cantataStreams(QStringList)), this, SLOT(cantataStreams(QStringList)));
    connect(MPDConnection::self(), SIGNAL(removedIds(QSet<qint32>)), this, SLOT(removedIds(QSet<qint32>)));
}

bool HttpServer::start()
{
    if (closeTimer) {
        DBUG << "stop close timer";
        closeTimer->stop();
    }

    if (socket) {
        DBUG << "already open";
        return true;
    }

    DBUG << "open new socket";
    quint16 prevPort=Settings::self()->httpAllocatedPort();
    bool newThread=0==thread;
    if (newThread) {
        thread=new Thread("HttpServer");
    }
    socket=new HttpSocket(Settings::self()->httpInterface(), prevPort);
    socket->mpdAddress(mpdAddr);
    connect(this, SIGNAL(terminateSocket()), socket, SLOT(terminate()), Qt::QueuedConnection);
    if (socket->serverPort()!=prevPort) {
        Settings::self()->saveHttpAllocatedPort(socket->serverPort());
    }
    socket->moveToThread(thread);
    bool started=socket->isListening();
    if (newThread) {
        thread->start();
    }
    return started;
}

void HttpServer::stop()
{
    if (socket) {
        DBUG;
        emit terminateSocket();
        socket=0;
    }
}

void HttpServer::readConfig()
{
    QString iface=Settings::self()->httpInterface();

    if (socket && socket->isListening() && iface==socket->configuredInterface()) {
        return;
    }

    bool wasStarted=0!=socket;
    stop();
    if (wasStarted) {
        start();
    }
}

QString HttpServer::address() const
{
    return socket ? QLatin1String("http://")+socket->address()+QChar(':')+QString::number(socket->serverPort())
                  : QLatin1String("http://127.0.0.1:*");
}

bool HttpServer::isOurs(const QString &url) const
{
    return 0!=socket ? url.startsWith(address()+"/") : false;
}

QByteArray HttpServer::encodeUrl(const Song &s)
{
    DBUG << "song" << s.file << isAlive();
    if (!start()) {
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
    if (!s.composer.isEmpty()) {
        query.addQueryItem("composer", s.composer);
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
    query.addQueryItem("id", QString::number(s.id));
    query.addQueryItem("cantata", "song");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    DBUG << "encoded as" << url.toString();
    return url.toEncoded();
}

QByteArray HttpServer::encodeUrl(const QString &file)
{
    Song s;
    #ifdef Q_OS_WIN
    QString f=fixWindowsPath(file);
    DBUG << "file" << f << "orig" << file;
    // For some reason, drag'n' drop of \\share\path\file.mp3 is changed to share/path/file.mp3!
    if (!f.startsWith(QLatin1String("//")) && !QFile::exists(f)) {
        QString share=f.startsWith(QLatin1Char('/')) ? (QLatin1Char('/')+f) : (QLatin1String("//")+f);
        if (QFile::exists(share)) {
            f=share;
            DBUG << "converted to share-path" << f;
        }
    }
    #ifdef TAGLIB_FOUND
    s=Tags::read(f);
    #endif
    s.file=f;
    #else
    DBUG << "file" << file;
    #ifdef TAGLIB_FOUND
    s=Tags::read(file);
    #endif
    s.file=file;
    #endif
    return encodeUrl(s);
}

Song HttpServer::decodeUrl(const QString &url) const
{
    #if QT_VERSION >= 0x050000
    return decodeUrl(QUrl(url));
    #else
    QUrl u;
    u.setEncodedUrl(url.toUtf8());
    return decodeUrl(u);
    #endif
}

Song HttpServer::decodeUrl(const QUrl &url) const
{
    Song s;
    #if QT_VERSION < 0x050000
    const QUrl &q=url;
    #else
    QUrlQuery q(url);
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
        if (q.hasQueryItem("composer")) {
            s.composer=q.queryItemValue("composer");
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
        if (q.hasQueryItem("id")) {
            s.id=q.queryItemValue("id").toInt();
        }
        s.file=url.path();
        s.type=Song::CantataStream;
        #ifdef Q_OS_WIN
        s.file=fixWindowsPath(s.file);
        #endif
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        if (s.file.startsWith(QChar('/')+Song::constCddaProtocol)) {
            s.file=s.file.mid(1);
            s.type=Song::Cdda;
        }
        #endif
        DBUG << s.file << s.albumArtist() << s.album << s.title;
    }

    return s;
}

void HttpServer::startCloseTimer()
{
    if (!closeTimer) {
        closeTimer=new QTimer(this);
        closeTimer->setSingleShot(true);
        connect(closeTimer, SIGNAL(timeout()), this, SLOT(stop()));
    }
    DBUG;
    closeTimer->start(1000);
}

void HttpServer::mpdAddress(const QString &a)
{
    mpdAddr=a;
}

void HttpServer::cantataStreams(const QStringList &files)
{
    DBUG << files;
    foreach (const QString &f, files) {
        Song s=HttpServer::self()->decodeUrl(f);
        if (s.isCantataStream() || s.isCdda()) {
            start();
            break;
        }
    }
}

void HttpServer::cantataStreams(const QList<Song> &songs, bool isUpdate)
{
    DBUG << isUpdate << songs.count();
    if (!isUpdate) {
        streamIds.clear();
    }

    foreach (const Song &s, songs) {
        streamIds.insert(s.id);
    }

    if (streamIds.isEmpty()) {
        startCloseTimer();
    } else {
        start();
    }
}

void HttpServer::removedIds(const QSet<qint32> &ids)
{
    streamIds+=ids;
    if (streamIds.isEmpty()) {
        startCloseTimer();
    }
}

#endif
