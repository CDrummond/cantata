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

#include "taghelper.h"
#include "tags.h"
#include <QDataStream>
#include <QVariant>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QThread>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__

void TagHelper::enableDebug()
{
    debugEnabled=true;
}

static QString socketName;

static void deleteSocket()
{
    // Just in case Cantata has crashed, ensure we delete the socket...
    if (!socketName.isEmpty()) {
        QLocalServer::removeServer(socketName);
    }
}

TagHelper::TagHelper(const QString &sockName, int parent)
    : parentPid(parent)
    , dataSize(0)
{
    socket=new QLocalSocket(this);
    socket->connectToServer(sockName);
    connect(socket, SIGNAL(readyRead()), SLOT(dataReady()));
    connect(socket, SIGNAL(disconnected()), qApp, SLOT(quit()));
    QTimer *timer=new QTimer(this);
    timer->setSingleShot(false);
    timer->start(5000);
    connect(timer, SIGNAL(timeout()), SLOT(checkParent()));
    socketName=sockName;
    atexit(deleteSocket);
}

TagHelper::~TagHelper()
{
}

void TagHelper::dataReady()
{
    DBUG << dataSize;
    while (socket->bytesAvailable()) {
        if (0==dataSize) {
            QDataStream stream(socket);
            stream >> dataSize;
            if (dataSize<=0) {
                qApp->exit();
                return;
            }
        }

        data+=socket->read(dataSize-data.length());
        DBUG << data.length() << "/" << dataSize;
        if (data.length() == dataSize) {
            process();
        }
    }
}

void TagHelper::checkParent()
{
    // If parent process (Cantata) has terminated, then we need to exit...
    #ifdef Q_OS_WIN
    if (0==OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, parentPid)) {
        qApp->exit();
    }
    #else
    if (0!=::kill(parentPid, 0)) {
        qApp->exit();
    }
    #endif
}

void TagHelper::process()
{
    QByteArray response;
    QDataStream inStream(data);
    QDataStream outStream(&response, QIODevice::WriteOnly);
    QString request;
    QString fileName;

    inStream >> request >> fileName;

    DBUG << "REQ" << request << fileName;
    if (QLatin1String("read")==request) {
        outStream << Tags::read(fileName);
    } else if (QLatin1String("readImage")==request) {
        outStream << Tags::readImage(fileName);
    } else if (QLatin1String("readLyrics")==request) {
        outStream << Tags::readLyrics(fileName);
    } else if (QLatin1String("readComment")==request) {
        outStream << Tags::readComment(fileName);
    } else if (QLatin1String("updateArtistAndTitle")==request) {
        Song song;
        outStream << (int)Tags::updateArtistAndTitle(fileName, song);
    } else if (QLatin1String("update")==request) {
        Song from;
        Song to;
        int id3Ver;
        bool saveComment;
        inStream >> from >> to >> id3Ver >> saveComment;
        outStream << (int)Tags::update(fileName, from, to, id3Ver, saveComment);
    } else if (QLatin1String("readReplaygain")==request) {
        Tags::ReplayGain rg=Tags::readReplaygain(fileName);
        outStream << rg;
    } else if (QLatin1String("updateReplaygain")==request) {
        Tags::ReplayGain rg;
        inStream >> rg;
        outStream << (int)Tags::updateReplaygain(fileName, rg);
    } else if (QLatin1String("embedImage")==request) {
        QByteArray cover;
        inStream >> cover;
        outStream << (int)Tags::embedImage(fileName, cover);
    } else if (QLatin1String("oggMimeType")==request) {
        outStream << Tags::oggMimeType(fileName);
    } else if (QLatin1String("readRating")==request) {
        outStream << Tags::readRating(fileName);
    } else if (QLatin1String("updateRating")==request) {
        int rating=-1;
        inStream >> rating;
        outStream << (int)Tags::updateRating(fileName, rating);
    } else if (QLatin1String("readAll")==request) {
        outStream << Tags::readAll(fileName);
    } else {
        qApp->exit();
    }

    DBUG << "RESP" << response.size();
    QDataStream writeStream(socket);
    writeStream << qint32(response.length());
    if (!response.isEmpty()) {
        writeStream.writeRawData(response.data(), response.length());
    }
    socket->flush();
    data.clear();
    dataSize=0;
}

#include "moc_taghelper.cpp"
