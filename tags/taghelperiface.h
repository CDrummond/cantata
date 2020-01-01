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

#ifndef TAG_HELPER_IFACE_H
#define TAG_HELPER_IFACE_H

#include "mpd-interface/song.h"
#include <QImage>
#include <QString>
#include <QMutex>
#include <QSemaphore>
#include <QMap>

class QLocalServer;
class QLocalSocket;
class QProcess;
class Thread;

namespace Tags
{
    struct ReplayGain;
}

class TagHelperIface : public QObject
{
    Q_OBJECT

public:
    static void enableDebug();
    static TagHelperIface * self();

    struct Reply
    {
        bool status;
        QByteArray data;
    };

    TagHelperIface();
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
    int readRating(const QString &fileName);
    int updateRating(const QString &fileName, int rating);
    QMap<QString, QString> readAll(const QString &fileName);

private:
    bool helperIsRunning();
    Reply sendMessage(const QByteArray &msg);
    bool startHelper();
    void setStatus(bool st);

private Q_SLOTS:
    void close();
    void stopHelper();
    void sendMsg();
    void dataReady();
    void helperClosed();

private:
    QMutex mutex;
    QByteArray data;
    bool msgStatus;
    qint32 dataSize;
    bool awaitingResponse;
    Thread *thread;
    QSemaphore sema;
    QProcess *proc;
    QLocalServer *server;
    QLocalSocket *sock;
};

#endif
