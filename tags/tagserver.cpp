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

#include "tagserver.h"
#include "tags.h"
#include <QDataStream>
#include <QVariant>
#include <QCoreApplication>

TagServer::TagServer(const char *socketName)
    : QObject(0)
    , socket(new QLocalSocket(this))
{
    socket->connectToServer(socketName);
    if (socket->waitForConnected(1000)) {
        connect(socket, SIGNAL(readyRead()), this, SLOT(readRequest()));
        connect(socket, SIGNAL(stateChanged(QLocalSocket::LocalSocketState)), this, SLOT(stateChanged(QLocalSocket::LocalSocketState)));
    } else {
        socket->close();
        socket->deleteLater();
        socket=0;
    }
}

TagServer::~TagServer()
{
    if (socket) {
        socket->close();
        socket->deleteLater();
    }
}

void TagServer::readRequest()
{
    QDataStream stream(socket);
    QString request;
    QString fileName;
    stream >> request >> fileName;

    if (QLatin1String("read")==request) {
        stream << Tags::read(fileName);
    } else if (QLatin1String("readImage")==request) {
        stream << Tags::readImage(fileName);
    } else if (QLatin1String("readLyrics")==request) {
        stream << Tags::readLyrics(fileName);
    } else if (QLatin1String("updateArtistAndTitle")==request) {
        Song song;
        stream << (int)Tags::updateArtistAndTitle(fileName, song);
    } else if (QLatin1String("update")==request) {
        Song from;
        Song to;
        int id3Ver;
        stream >> from >> to >> id3Ver;
        stream << (int)Tags::update(fileName, from, to, id3Ver);
    } else if (QLatin1String("readReplaygain")==request) {
        stream << Tags::readReplaygain(fileName);
    } else if (QLatin1String("updateReplaygain")==request) {
        Tags::ReplayGain rg;
        stream >> rg;
        stream << (int)Tags::updateReplaygain(fileName, rg);
    } else if (QLatin1String("embedImage")==request) {
        QByteArray cover;
        stream >> cover;
        stream << (int)Tags::embedImage(fileName, cover);
    }
}

void TagServer::stateChanged(QLocalSocket::LocalSocketState state)
{
    if (QLocalSocket::ClosingState==state) {
        qApp->exit(0);
    }
}
