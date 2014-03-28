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
 * along with this program; see the file COPYING.  If not, readStatusite to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "tagclient.h"
#include "tags.h"
#include "config.h"
#include "globalstatic.h"
#include "thread.h"
#include <QMutexLocker>
#include <QProcess>
#include <QDataStream>
#include <QApplication>
#include <QLocalSocket>
#include <QLocalServer>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << QThread::currentThread()->objectName() << __FUNCTION__

enum ReadStatus {
    Read_Ok,
    Read_Timeout,
    Read_Closed,
    Read_Error
};

void TagClient::enableDebug()
{
    debugEnabled=true;
}

GLOBAL_STATIC(TagClient, instance)

TagClient::TagClient()
    : msgStatus(true)
    , dataSize(0)
    , awaitingResponse(false)
    , thread(0)
    , proc(0)
    , server(0)
    , sock(0)
{
    qRegisterMetaType<QAbstractSocket::SocketError >("QAbstractSocket::SocketError");
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

void TagClient::stop()
{
    if (thread) {
        thread->stop();
        thread=0;
    }
}

Song TagClient::read(const QString &fileName)
{
    DBUG << fileName;
    Song resp;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    }
    return resp;
}

QImage TagClient::readImage(const QString &fileName)
{
    DBUG << fileName;
    QImage resp;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    }
    return resp;
}

QString TagClient::readLyrics(const QString &fileName)
{
    DBUG << fileName;
    QString resp;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    }
    return resp;
}

QString TagClient::readComment(const QString &fileName)
{
    DBUG << fileName;
    QString resp;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    }
    return resp;
}

int TagClient::updateArtistAndTitle(const QString &fileName, const Song &song)
{
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName << song;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    } else {
        resp=Tags::Update_BadFile;
    }
    return resp;
}

int TagClient::update(const QString &fileName, const Song &from, const Song &to, int id3Ver, bool saveComment)
{
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName << from << to << id3Ver << saveComment;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    } else {
        resp=Tags::Update_BadFile;
    }
    return resp;
}

Tags::ReplayGain TagClient::readReplaygain(const QString &fileName)
{
    DBUG << fileName;
    Tags::ReplayGain resp;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    }
    return resp;
}

int TagClient::updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg)
{
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName << rg;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    } else {
        resp=Tags::Update_BadFile;
    }
    return resp;
}

int TagClient::embedImage(const QString &fileName, const QByteArray &cover)
{
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName << cover;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    } else {
        resp=Tags::Update_BadFile;
    }
    return resp;
}

QString TagClient::oggMimeType(const QString &fileName)
{
    DBUG << fileName;
    QString resp;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    }
    return resp;
}

TagClient::Reply TagClient::sendMessage(const QByteArray &msg)
{
    QMutexLocker locker(&mutex);
    data=msg;
    metaObject()->invokeMethod(this, "sendMsg", Qt::QueuedConnection);
    sema.acquire();
    TagClient::Reply reply;
    reply.status=msgStatus;
    reply.data=data;
    DBUG << "Message response - " << reply.status << reply.data.length();
    return reply;
}

static const int constMaxWait=5000;

bool TagClient::startHelper()
{
    DBUG << (void *)proc;
    if (!helperIsRunning()) {
        stopHelper();
        #ifdef Q_OS_WIN
        int currentPid=GetCurrentProcessId();
        #else
        int currentPid=getpid();
        #endif
        DBUG << "Create server";
        server=new QLocalServer(this);

        forever {
            QString name="cantata-tags-"+QString::number(currentPid)+QLatin1Char('-')+QString::number(Utils::random());
            QLocalServer::removeServer(name);
            if (server->listen(name)) {
                DBUG << "Listening on" << server->fullServerName();
                break;
            }
        }
        DBUG << "Start process";
        proc=new QProcess(this);
        #ifdef Q_OS_WIN
        proc->start(qApp->applicationDirPath()+"/helpers/cantata-tags.exe", QStringList() << server->fullServerName() << QString::number(currentPid));
        #else
        proc->start(INSTALL_PREFIX"/lib/cantata/cantata-tags", QStringList() << server->fullServerName() << QString::number(currentPid));
        #endif
        connect(proc, SIGNAL(finished(int)), this, SLOT(helperClosed()));
        if (proc->waitForStarted(constMaxWait)) {
            DBUG << "Process started, on pid" << proc->pid() << "- wait for helper to connect";
            if (server->waitForNewConnection(constMaxWait)) {
                sock=server->nextPendingConnection();
            }
            if (sock) {
                DBUG << "Helper connected";
                connect(sock, SIGNAL(readyRead()), this, SLOT(dataReady()));
                connect(sock, SIGNAL(disconnected()), this, SLOT(helperClosed()));
                return true;
            } else {
                DBUG << "Helper did not connect";
            }
        } else {
            DBUG << "Failed to start process";
        }
        DBUG << "Failed to start";
        stopHelper();
        return false;
    }
    return true;
}

void TagClient::stopHelper()
{
    if (sock) {
        DBUG << "Socket" << (void *)sock;
        disconnect(sock, SIGNAL(readyRead()), this, SLOT(dataReady()));
        disconnect(sock, SIGNAL(disconnected()), this, SLOT(helperClosed()));
        sock->flush();
        sock->close();
        sock->deleteLater();
        sock=0;
    }
    if (server) {
        DBUG << "Server" << (void *)server;
        server->close();
        server->deleteLater();
        server=0;
    }
    if (proc) {
        disconnect(proc, SIGNAL(finished(int)), this, SLOT(helperClosed()));
        DBUG << "Process" << (void *)proc;
        if (QProcess::NotRunning!=proc->state()) {
            proc->kill();
            proc->waitForFinished();
        }
        proc->deleteLater();
        proc=0;
    }
    setStatus(false);
}

bool TagClient::helperIsRunning()
{
    return proc && QProcess::Running==proc->state() && sock && QLocalSocket::ConnectedState==sock->state();
}

void TagClient::sendMsg()
{
    DBUG;
    if (startHelper()) {
        awaitingResponse=true; // In here because startHelper might call stopHelper, which call setStatus!
        QDataStream stream(sock);
        stream << qint32(data.length());
        stream.writeRawData(data.data(), data.length());
        sock->flush();
        DBUG << "Message sent";
        data.clear();
        dataSize=0;
    } else {
        awaitingResponse=true;
        setStatus(false);
    }
}

void TagClient::dataReady()
{
    DBUG;
    if (!awaitingResponse) {
        DBUG << "Socket is ready to read, but not expecting any message!!!";
        stopHelper();
        return;
    }

    while (sock->bytesAvailable()) {
        if (0==dataSize) {
            QDataStream stream(sock);
            stream >> dataSize;
            DBUG << "Expecting" << dataSize << "bytes in response";
            if (dataSize<=0) {
                setStatus(false);
                return;
            }
        }

        data+=sock->read(dataSize-data.length());
        if (data.length() == dataSize) {
            DBUG << "Response fully received";
            setStatus(true);
            break;
        }
    }
}

void TagClient::helperClosed()
{
    DBUG;
    setStatus(false);
}

void TagClient::setStatus(bool st)
{
    DBUG << st << awaitingResponse;
    if (awaitingResponse) {
        awaitingResponse=false;
        msgStatus=st;
        sema.release();
    }
}
