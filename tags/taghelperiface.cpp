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
 * along with this program; see the file COPYING.  If not, readStatusite to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "taghelperiface.h"
#include "tags.h"
#include "config.h"
#include "support/globalstatic.h"
#include "support/thread.h"
#include <QMutexLocker>
#include <QProcess>
#include <QDataStream>
#include <QApplication>
#include <QLocalSocket>
#include <QLocalServer>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << QThread::currentThread()->objectName() << __FUNCTION__

void TagHelperIface::enableDebug()
{
    debugEnabled=true;
}

GLOBAL_STATIC(TagHelperIface, instance)

TagHelperIface::TagHelperIface()
    : msgStatus(true)
    , dataSize(0)
    , awaitingResponse(false)
    , thread(nullptr)
    , proc(nullptr)
    , server(nullptr)
    , sock(nullptr)
{
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

void TagHelperIface::stop()
{
    if (thread) {
        DBUG;
        QMutexLocker locker(&mutex);
        metaObject()->invokeMethod(this, "close", Qt::QueuedConnection);
        sema.acquire();
        DBUG << "Stop thread";
        thread->stop();
        thread=nullptr;
    }
}

Song TagHelperIface::read(const QString &fileName)
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

QImage TagHelperIface::readImage(const QString &fileName)
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

QString TagHelperIface::readLyrics(const QString &fileName)
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

QString TagHelperIface::readComment(const QString &fileName)
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

int TagHelperIface::updateArtistAndTitle(const QString &fileName, const Song &song)
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

int TagHelperIface::update(const QString &fileName, const Song &from, const Song &to, int id3Ver, bool saveComment)
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

Tags::ReplayGain TagHelperIface::readReplaygain(const QString &fileName)
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

int TagHelperIface::updateReplaygain(const QString &fileName, const Tags::ReplayGain &rg)
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

int TagHelperIface::embedImage(const QString &fileName, const QByteArray &cover)
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

QString TagHelperIface::oggMimeType(const QString &fileName)
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

int TagHelperIface::readRating(const QString &fileName)
{
    DBUG << fileName;
    int resp=0;
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

int TagHelperIface::updateRating(const QString &fileName, int rating)
{
    DBUG << fileName;
    int resp=Tags::Update_Failed;
    QByteArray message;
    QDataStream outStream(&message, QIODevice::WriteOnly);
    outStream << QString(__FUNCTION__) << fileName << rating;
    Reply reply=sendMessage(message);
    if (reply.status) {
        QDataStream inStream(reply.data);
        inStream >> resp;
    } else {
        resp=Tags::Update_BadFile;
    }
    return resp;
}

QMap<QString, QString> TagHelperIface::readAll(const QString &fileName)
{
    DBUG << fileName;
    QMap<QString, QString> resp;
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

TagHelperIface::Reply TagHelperIface::sendMessage(const QByteArray &msg)
{
    QMutexLocker locker(&mutex);
    data=msg;
    metaObject()->invokeMethod(this, "sendMsg", Qt::QueuedConnection);
    sema.acquire();
    TagHelperIface::Reply reply;
    reply.status=msgStatus;
    reply.data=data;
    DBUG << "Message response - " << reply.status << reply.data.length();
    return reply;
}

static const int constMaxWait=5000;

bool TagHelperIface::startHelper()
{
    DBUG << (void *)proc;
    if (!helperIsRunning()) {
        stopHelper();
        qint64 currentPid=QCoreApplication::applicationPid();
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
        proc->setProcessChannelMode(QProcess::ForwardedChannels);
        proc->start(Utils::helper(QLatin1String("cantata-tags")), QStringList() << server->fullServerName() << QString::number(currentPid)
                                                                                << QString(debugEnabled ? "true" : "false"));

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

void TagHelperIface::close()
{
    DBUG;
    awaitingResponse=true;
    stopHelper();
}

void TagHelperIface::stopHelper()
{
    if (sock) {
        DBUG << "Socket" << (void *)sock;
        disconnect(sock, SIGNAL(readyRead()), this, SLOT(dataReady()));
        disconnect(sock, SIGNAL(disconnected()), this, SLOT(helperClosed()));
        sock->flush();
        sock->close();
        sock->deleteLater();
        sock=nullptr;
    }
    if (server) {
        DBUG << "Server" << (void *)server;
        server->close();
        server->deleteLater();
        server=nullptr;
    }
    if (proc) {
        disconnect(proc, SIGNAL(finished(int)), this, SLOT(helperClosed()));
        DBUG << "Process" << (void *)proc;
        if (QProcess::NotRunning!=proc->state()) {
            proc->kill();
            proc->waitForFinished(10);
        }
        proc->deleteLater();
        proc=nullptr;
    }
    setStatus(false);
}

bool TagHelperIface::helperIsRunning()
{
    return proc && QProcess::Running==proc->state() && sock && QLocalSocket::ConnectedState==sock->state();
}

void TagHelperIface::sendMsg()
{
    DBUG;
    if (startHelper()) {
        awaitingResponse=true; // In here because startHelper might call stopHelper, which calls setStatus!
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

void TagHelperIface::dataReady()
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

void TagHelperIface::helperClosed()
{
    DBUG;
    setStatus(false);
}

void TagHelperIface::setStatus(bool st)
{
    DBUG << st << awaitingResponse;
    if (awaitingResponse) {
        awaitingResponse=false;
        msgStatus=st;
        sema.release();
    }
}

#include "moc_taghelperiface.cpp"
