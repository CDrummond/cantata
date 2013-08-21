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

#ifndef _HTTP_SOCKET_H_
#define _HTTP_SOCKET_H_

#include <QTcpServer>

class QHostAddress;

class HttpSocket : public QTcpServer
{
    Q_OBJECT

public:
     HttpSocket(const QString &iface, quint16 port);
     virtual ~HttpSocket() { }

     void terminate();
     void incomingConnection(int socket);
     QString address() const { return ifaceAddress; }
     QString configuredInterface() { return cfgInterface; }

private:
     bool openPort(const QHostAddress &a, quint16 p);

private Q_SLOTS:
     void readClient();
     void discardClient();

private:
    QString cfgInterface;
    QString ifaceAddress;
    bool terminated;
};

#endif
