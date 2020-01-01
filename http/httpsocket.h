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

#ifndef _HTTP_SOCKET_H_
#define _HTTP_SOCKET_H_

#include <QTcpServer>
#include <QMap>
#include <QList>
#include <QSet>

struct Song;
class QHostAddress;
class QTcpSocket;

class HttpSocket : public QTcpServer
{
    Q_OBJECT

public:
    HttpSocket(const QString &iface, quint16 port);
    ~HttpSocket() override { }

    QString configuredInterface() { return cfgInterface; }
    quint16 boundPort();

public Q_SLOTS:
    void terminate();
    void mpdAddress(const QString &a);

private:
    bool openPort(quint16 p);
    bool isCantataStream(const QString &file) const;
    void sendErrorResponse(QTcpSocket *socket, int code);

private Q_SLOTS:
    void handleNewConnection();
    void readClient();
    void discardClient();
    void cantataStreams(const QStringList &files);
    void cantataStreams(const QList<Song> &songs, bool isUpdate);
    void removedIds(const QSet<qint32> &ids);

private:
    bool write(QTcpSocket *socket, char *buffer, qint32 bytesRead, bool &stop);
    void setUrlAddress();

private:
    QSet<QString> newlyAddedFiles; // Holds cantata strema filenames as added to MPD via "add"
    QMap<qint32, QString> streamIds; // Maps MPD playqueue song ID to fileName
    QString cfgInterface;
    QString mpdAddr;
    bool terminated;
};

#endif
