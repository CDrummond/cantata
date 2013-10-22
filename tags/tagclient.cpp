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

#include "tagclient.h"
#include <QMutex>
#include <QMutexLocker>
#include <QImage>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
K_GLOBAL_STATIC(TagClient, instance)
#endif
#ifdef ENABLE_EXTERNAL_TAGS
#include <QProcess>
#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>
#include <QEventLoop>
#endif
#include <QDebug>

static const int constMaxWait=5000;

TagClient * TagClient::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static TagClient *instance=0;
    if(!instance) {
        instance=new TagClient;
    }
    return instance;
    #endif
}

TagClient::TagClient()
    : mutex(new QMutex)
    #ifdef ENABLE_EXTERNAL_TAGS
    , loop(0)
    , proc(0)
    , server(0)
    , socket(0)
    #endif
{
}

TagClient::~TagClient()
{
    stop();
    delete mutex;
}

void TagClient::stop()
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    stopHelper();
    #endif
}

Song TagClient::read(const QString &fileName)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    Song resp;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName;
        if (Wait_Ok==waitForReply()) {
            stream >> resp;
        }
    }
    return resp;
    #else
    return Tags::read(fileName);
    #endif
}

QImage TagClient::readImage(const QString &fileName)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    QByteArray data;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName;
        if (Wait_Ok==waitForReply()) {
            stream >> data;
        }
    }
    #else
    QByteArray data=Tags::readImage(fileName);
    #endif
    QImage img;
    if (!data.isEmpty()) {
        img.loadFromData(data);
        if (img.isNull()) {
            img.loadFromData(QByteArray::fromBase64(data));
        }
    }
    return img;
}

QString TagClient::readLyrics(const QString &fileName)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    QString resp;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName;
        if (Wait_Ok==waitForReply()) {
            stream >> resp;
        }
    }
    return resp;
    #else
    return Tags::readLyrics(fileName);
    #endif
}

Tags::Update TagClient::updateArtistAndTitle(const QString &fileName, const Song &song)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName << song;
        WaitReply wr=waitForReply();
        if (Wait_Ok==wr) {
            stream >> resp;
        } else {
            resp=Wait_Timeout==wr ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return (Tags::Update)resp;
    #else
    return Tags::updateArtistAndTitle(fileName, song);
    #endif
}

Tags::Update TagClient::update(const QString &fileName, const Song &from, const Song &to, int id3Ver)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName << from << to << id3Ver;
        WaitReply wr=waitForReply();
        if (Wait_Ok==wr) {
            stream >> resp;
        } else {
            resp=Wait_Timeout==wr ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return (Tags::Update)resp;
    #else
    return Tags::update(fileName, from, to, id3Ver);
    #endif
}

Tags::ReplayGain TagClient::readReplaygain(const QString &fileName)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    Tags::ReplayGain resp;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName;
        if (Wait_Ok==waitForReply()) {
            stream >> resp;
        }
    }
    return resp;
    #else
    return Tags::readReplaygain(fileName);
    #endif
}

Tags::Update TagClient::updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName << rg;
        WaitReply wr=waitForReply();
        if (Wait_Ok==wr) {
            stream >> resp;
        } else {
            resp=Wait_Timeout==wr ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return (Tags::Update)resp;
    #else
    return Tags::updateReplaygain(fileName, rg);
    #endif
}

Tags::Update TagClient::embedImage(const QString &fileName, const QByteArray &cover)
{
    QMutexLocker locker(mutex);
    #ifdef ENABLE_EXTERNAL_TAGS
    int resp=Tags::Update_Failed;
    if (startHelper()) {
        QDataStream stream(socket);
        stream << QString(__FUNCTION__) << fileName << cover;
        WaitReply wr=waitForReply();
        if (Wait_Ok==wr) {
            stream >> resp;
        } else {
            resp=Wait_Timeout==wr ? Tags::Update_Timedout : Tags::Update_BadFile;
        }
    }
    return (Tags::Update)resp;
    #else
    return Tags::embedImage(fileName, cover);
    #endif
}

#ifdef ENABLE_EXTERNAL_TAGS
enum ErrorCodes {
    Connected = 0,
    FailedToStart = 1,
    OtherError = 2
};

#endif

void TagClient::processError(QProcess::ProcessError error)
{
    #ifdef ENABLE_EXTERNAL_TAGS
    switch (error) {
    case QProcess::FailedToStart:
        qWarning() << "Failed to start tag reader/writer";
        if (loop && loop->isRunning()) {
            loop->exit(FailedToStart);
        }
        break;
    default:
        qWarning() << "Tag reader/writer failed with error " << error << " - restarting";
        stopHelper();
        if (loop && loop->isRunning()) {
            loop->exit(OtherError);
        }
        break;
    }
    #endif
}

void TagClient::newConnection()
{
    #ifdef ENABLE_EXTERNAL_TAGS
    socket = server->nextPendingConnection();
    socket->setParent(this);
    closeServerSocket();
    if (loop && loop->isRunning()) {
        loop->exit(Connected);
    }
    #endif
}

#ifdef ENABLE_EXTERNAL_TAGS
bool TagClient::startHelper()
{
    if (!proc) {
        for (int i=0; i<5; ++i) { // Max5 start attempts...
            proc=new QProcess(this);
            server=new QLocalServer(this);

            connect(server, SIGNAL(newConnection()), this, SLOT(newConnection()));
            connect(proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));

            forever {
                const QString name = QString("cantata_tags_%1").arg(qrand() ^ ((int)(quint64(this) & 0xFFFFFFFF)));
                if (server->listen(name)) {
                    break;
                }
            }

            proc->setProcessChannelMode(QProcess::ForwardedChannels);
            #ifdef Q_OS_WIN
            proc->start(qApp->applicationDirPath()+"/cantata-tags.exe", QStringList() << server->fullServerName());
            #else
            proc->start(INSTALL_PREFIX"/lib/cantata/cantata-tags", QStringList() << server->fullServerName());
            #endif

            if (!loop) {
                loop=new QEventLoop(this);
            }
            int rv=loop->exec(QEventLoop::ExcludeUserInputEvents);
            loop->deleteLater();
            loop=0;
            if (rv!=OtherError) {
                break;
            }
        }
    }

    return 0!=socket;
}

void TagClient::stopHelper()
{
    QProcess *p=proc;
    QLocalSocket *s=socket;

    proc=0;
    socket=0;
    if (p) {
        disconnect(p, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
        p->terminate();
        p->deleteLater();
    }
    if (s) {
        s->deleteLater();
    }
    closeServerSocket();
}

void TagClient::closeServerSocket()
{
    QLocalServer *s=server;
    server=0;
    if (s) {
        disconnect(s, SIGNAL(newConnection()), this, SLOT(newConnection()));
        s->deleteLater();
    }
}

TagClient::WaitReply TagClient::waitForReply()
{
    if (!socket) {
        return Wait_Closed;
    }
    if (socket->waitForReadyRead(constMaxWait)) {
        return Wait_Ok;
    }
    return !socket || QLocalSocket::ConnectedState!=socket->state() ? Wait_Closed : Wait_Timeout;
}

#endif
