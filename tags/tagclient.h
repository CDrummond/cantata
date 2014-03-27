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

#ifndef TAG_CLIENT_H
#define TAG_CLIENT_H

#include "song.h"
#include <QImage>
#include <QString>
#include <QMutex>
#include <QSemaphore>

class QLocalServer;
class QLocalSocket;
class QProcess;
class Thread;

namespace Tags
{
    struct ReplayGain;
}

class TagClient : public QObject
{
    Q_OBJECT

public:
    static void enableDebug();
    static TagClient * self();

    struct Reply
    {
        int status;
        QByteArray data;
    };

    TagClient();
    void stop();
    Song read(const QString &fileName);
    QImage readImage(const QString &fileName);
    QString readLyrics(const QString &fileName);
    QString readComment(const QString &fileName);
    int updateArtistAndTitle(const QString &fileName, const Song &song);
    int update(const QString &fileName, const Song &from, const Song &to, int id3Ver, bool saveComment);
    Tags::ReplayGain readReplaygain(const QString &fileName);
    int updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg);
    int embedImage(const QString &fileName, const QByteArray &cover);
    QString oggMimeType(const QString &fileName);

private:
    bool helperIsRunning();
    Reply sendMessage(const QByteArray &msg);
    bool startHelper();

private Q_SLOTS:
    void stopHelper();
    void sendMsg();

private:
    QMutex mutex;
    QByteArray msgData;
    int msgStatus;
    Thread *thread;
    QSemaphore sema;
    QProcess *proc;
    QLocalServer *server;
    QLocalSocket *sock;
};

#endif
