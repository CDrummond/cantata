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

#include "tagserver.h"
#include "tags.h"
#include <QDataStream>
#include <QVariant>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

TagServer::TagServer(const QString &sockName)
    : socketName(sockName)
{
    socket=new QLocalSocket(this);
    socket->connectToServer(socketName);
    connect(socket, SIGNAL(readyRead()), SLOT(process()));
    connect(socket, SIGNAL(disconnected()), qApp, SLOT(quit()));
    QTimer *timer=new QTimer(this);
    timer->setSingleShot(false);
    timer->start(5000);
    connect(timer, SIGNAL(timeout()), SLOT(checkParent()));
}

TagServer::~TagServer()
{
}

void TagServer::process()
{
    if (socket->bytesAvailable()) {
        QByteArray message=socket->readAll();
        QByteArray response;
        QDataStream inStream(message);
        QDataStream outStream(&response, QIODevice::WriteOnly);
        QString request;
        QString fileName;

        inStream >> request >> fileName;

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
        } else {
            qApp->exit();
        }
        if (!response.isEmpty()) {
            socket->write(response);
        }
    }
}

void TagServer::checkParent()
{
    // If parent process (Cantata) has terminated, then we need to exit...
    if (0!=::kill(getppid(), 0)) {
        qApp->exit();
    }
}
