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

#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include <QObject>
#include <QByteArray>
#include <QSet>
#include "song.h"
#include "config.h"

class HttpSocket;
class Thread;
class QUrl;
class QTimer;

class HttpServer : public QObject
{
    Q_OBJECT

public:
    static void enableDebug();
    static bool debugEnabled();

    static HttpServer * self();

    HttpServer();
    virtual ~HttpServer() { }

    bool start();
    bool isAlive() const { return true; } // Started on-demamnd!
    bool forceUsage() const { return force; }
    void readConfig();
    QString address() const;
    bool isOurs(const QString &url) const;
    QByteArray encodeUrl(const Song &s);
    QByteArray encodeUrl(const QString &file);
    Song decodeUrl(const QUrl &url) const;
    Song decodeUrl(const QString &file) const;

public Q_SLOTS:
    void stop();

private Q_SLOTS:
    void startCloseTimer();
    void mpdAddress(const QString &a);
    void cantataStreams(const QStringList &files);
    void cantataStreams(const QList<Song> &songs, bool isUpdate);
    void removedIds(const QSet<qint32> &ids);

Q_SIGNALS:
    void terminateSocket();

private:
    bool force;
    Thread *thread;
    HttpSocket *socket;

    QString mpdAddr;
    QSet<qint32> streamIds; // Currently playing MPD stream IDs
    QTimer *closeTimer;
};

#endif

