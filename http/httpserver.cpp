/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "tags/tags.h"
#endif
#include "gui/settings.h"
#include "support/thread.h"
#include "support/globalstatic.h"
#include "mpd-interface/mpdconnection.h"
#include <QFile>
#include <QUrl>
#include <QTimer>
#include <QUrlQuery>

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
    : QObject(nullptr)
    , thread(nullptr)
    , socket(nullptr)
    , closeTimer(nullptr)
{
    connect(MPDConnection::self(), SIGNAL(cantataStreams(QList<Song>,bool)), this, SLOT(cantataStreams(QList<Song>,bool)));
    connect(MPDConnection::self(), SIGNAL(cantataStreams(QStringList)), this, SLOT(cantataStreams(QStringList)));
    connect(MPDConnection::self(), SIGNAL(removedIds(QSet<qint32>)), this, SLOT(removedIds(QSet<qint32>)));
    connect(MPDConnection::self(), SIGNAL(ifaceIp(QString)), this, SLOT(ifaceIp(QString)));
}

bool HttpServer::isAlive() const
{
    // started on demand, but only start if allowed
    return MPDConnection::self()->getDetails().allowLocalStreaming;
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
    bool newThread=nullptr==thread;
    if (newThread) {
        thread=new Thread("HttpServer");
    }
    socket=new HttpSocket(Settings::self()->httpInterface(), prevPort);
    socket->mpdAddress(MPDConnection::self()->ipAddress());
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
        socket=nullptr;
    }
}

void HttpServer::readConfig()
{
    QString iface=Settings::self()->httpInterface();

    if (socket && socket->isListening() && iface==socket->configuredInterface()) {
        return;
    }

    bool wasStarted=nullptr!=socket;
    stop();
    if (wasStarted) {
        start();
    }
}

static inline QString serverUrl(const QString &ip, quint16 port)
{
    return QLatin1String("http://")+ip+QLatin1Char(':')+QString::number(port);
}

QString HttpServer::address() const
{
    return socket ? serverUrl(currentIfaceIp, socket->serverPort()) : QLatin1String("http://127.0.0.1:*");
}

bool HttpServer::isOurs(const QString &url) const
{
    if (nullptr==socket || !url.startsWith(QLatin1String("http://"))) {
        return false;
    }

    for (const QString &ip: ipAddresses) {
        if (url.startsWith(serverUrl(ip, socket->serverPort()))) {
            return true;
        }
    }

    return false;
}

QByteArray HttpServer::encodeUrl(const Song &s)
{
    DBUG << "song" << s.file << isAlive();
    if (!start()) {
        return QByteArray();
    }
    QUrl url;
    QUrlQuery query;
    url.setScheme("http");
    url.setHost(currentIfaceIp);
    url.setPort(socket->serverPort());
    #ifdef Q_OS_WIN
    // Use a query item, as s.file might have a driver specifier
    query.addQueryItem("file", s.file);
    url.setPath("/"+Utils::getFile(s.file));
    #else
    url.setPath(s.file);
    #endif
    if (!s.album.isEmpty()) {
        query.addQueryItem("album", s.album);
    }
    if (!s.artist.isEmpty()) {
        query.addQueryItem("artist", s.artist);
    }
    if (!s.albumartist.isEmpty()) {
        query.addQueryItem("albumartist", s.albumartist);
    }
    if (!s.composer().isEmpty()) {
        query.addQueryItem("composer", s.composer());
    }
    if (!s.title.isEmpty()) {
        query.addQueryItem("title", s.title);
    }
    if (!s.genres[0].isEmpty()) {
        query.addQueryItem("genre", s.firstGenre());
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
    if (s.isFromOnlineService()) {
        query.addQueryItem("onlineservice", s.onlineService());
    }
    query.addQueryItem("id", QString::number(s.id));
    query.addQueryItem("cantata", "song");
    url.setQuery(query);
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
    return /*s.isEmpty() ? QByteArray() :*/ encodeUrl(s);
}

Song HttpServer::decodeUrl(const QString &url) const
{
    return decodeUrl(QUrl(url));
}

Song HttpServer::decodeUrl(const QUrl &url) const
{
    Song s;
    QUrlQuery q(url);

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
            s.setComposer(q.queryItemValue("composer"));
        }
        if (q.hasQueryItem("title")) {
            s.title=q.queryItemValue("title");
        }
        if (q.hasQueryItem("genre")) {
            s.addGenre(q.queryItemValue("genre"));
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
        if (q.hasQueryItem("onlineservice")) {
            s.setIsFromOnlineService(q.queryItemValue("onlineservice"));
        }
        #ifdef Q_OS_WIN
        s.file=fixWindowsPath(q.queryItemValue("file"));
        #else
        s.file=url.path();
        #endif
        s.type=Song::CantataStream;
        #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
        if (s.file.startsWith(Song::constCddaProtocol)) {
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

void HttpServer::cantataStreams(const QStringList &files)
{
    DBUG << files;
    for (const QString &f: files) {
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

    for (const Song &s: songs) {
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

void HttpServer::ifaceIp(const QString &ip)
{
    DBUG << "MPD interface ip" << ip;
    if (ip.isEmpty()) {
        return;
    }
    currentIfaceIp=ip;
    ipAddresses.insert(ip);
}

#endif

#include "moc_httpserver.cpp"
