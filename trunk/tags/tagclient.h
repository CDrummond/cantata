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

#ifndef TAG_CLIENT_H
#define TAG_CLIENT_H

#include "tags.h"
#include "config.h"
#ifdef ENABLE_EXTERNAL_TAGS
#include <QVariant>
#include <QVariantList>
#endif
#include <QProcess>

#ifdef ENABLE_EXTERNAL_TAGS
class QEventLoop;
class QLocalServer;
class QLocalSocket;
#endif
class QMutex;

class TagClient : public QObject
{
    Q_OBJECT

public:
    static TagClient * self();

    TagClient();
    ~TagClient();

    void stop();

    Song read(const QString &fileName);
    QImage readImage(const QString &fileName);
    QString readLyrics(const QString &fileName);
    Tags::Update updateArtistAndTitle(const QString &fileName, const Song &song);
    Tags::Update update(const QString &fileName, const Song &from, const Song &to, int id3Ver=-1);
    Tags::ReplayGain readReplaygain(const QString &fileName);
    Tags::Update updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg);
    Tags::Update embedImage(const QString &fileName, const QByteArray &cover);

private Q_SLOTS:
    void processError(QProcess::ProcessError error);
    void newConnection();

private:
    #ifdef ENABLE_EXTERNAL_TAGS
    enum WaitReply {
        Wait_Ok,
        Wait_Timeout,
        Wait_Closed
    };

    bool startHelper();
    void stopHelper();
    void closeServerSocket();
    WaitReply waitForReply();
    #endif

private:
    QMutex *mutex;
    #ifdef ENABLE_EXTERNAL_TAGS
    QEventLoop *loop;
    QProcess *proc;
    QLocalServer *server;
    QLocalSocket *socket;
    #endif
};

#endif
