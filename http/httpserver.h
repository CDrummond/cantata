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

#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include <QObject>
#include <QByteArray>
#include <QSet>
#include "mpd-interface/song.h"
#include "config.h"

class HttpSocket;
class Thread;
class QUrl;
class QTimer;

class HttpServer : public QObject
{
    #if defined ENABLE_HTTP_SERVER
    Q_OBJECT
    #endif

public:
    static void enableDebug();
    static bool debugEnabled();

    static HttpServer * self();

    ~HttpServer() override { }

    #ifdef ENABLE_HTTP_SERVER
    HttpServer();
    bool isAlive() const; // Started on-demamnd!
    void readConfig();
    QString address() const;
    bool isOurs(const QString &url) const;
    QByteArray encodeUrl(const Song &s);
    QByteArray encodeUrl(const QString &file);
    Song decodeUrl(const QUrl &url) const;
    Song decodeUrl(const QString &file) const;
    #else
    HttpServer() { }
    bool isAlive() const { return false; } // Not used!
    void readConfig() { }
    QString address() const { return QString(); }
    bool isOurs(const QString &) const { return false; }
    QByteArray encodeUrl(const Song &) { return QByteArray(); }
    QByteArray encodeUrl(const QString &) { return QByteArray(); }
    Song decodeUrl(const QUrl &) const { return Song(); }
    Song decodeUrl(const QString &) const { return Song(); }
    #endif

private:
    #ifdef ENABLE_HTTP_SERVER
    bool start();

private Q_SLOTS:
    void stop();
    void startCloseTimer();
    void cantataStreams(const QStringList &files);
    void cantataStreams(const QList<Song> &songs, bool isUpdate);
    void removedIds(const QSet<qint32> &ids);
    void ifaceIp(const QString &ip);

Q_SIGNALS:
    void terminateSocket();

private:
    Thread *thread;
    HttpSocket *socket;

    QSet<qint32> streamIds; // Currently playing MPD stream IDs
    QTimer *closeTimer;

    QString currentIfaceIp;
    QSet<QString> ipAddresses;
    #endif
};

#endif

