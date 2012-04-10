/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "httpsocket.h"
#include <QtNetwork/QTcpSocket>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QUrl>

HttpSocket::HttpSocket(quint16 p)
    : QTcpServer(0)
    , portNumber(p)
    , terminated(false)
{
    listen(QHostAddress::LocalHost, portNumber);
}

void HttpSocket::terminate()
{
    terminated=true;
    deleteLater();
}

void HttpSocket::incomingConnection(int socket)
{
    QTcpSocket *s = new QTcpSocket(this);
    connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);
}

void HttpSocket::readClient()
{
    if (terminated) {
        return;
    }

    QTcpSocket *socket = (QTcpSocket*)sender();
    if (socket->canReadLine()) {
        QString line=socket->readLine();
        QStringList tokens = line.split(QRegExp("[ \r\n][ \r\n]*"));
        if (QLatin1String("GET")==tokens[0]) {
            QUrl url(tokens[1]);
            bool ok=false;
            if (url.hasQueryItem("cantata")) {
                QFile f(url.path());

                if (f.open(QIODevice::ReadOnly)) {
                    ok=true;
                    for(; !terminated;) {
                        QByteArray buffer=f.read(4096);
                        if (buffer.size()>0) {
                            socket->write(buffer);
                        } else {
                            break;
                        }
                    }
                }
            }

            if (!ok) {
                QTextStream os(socket);
                os.setAutoDetectUnicode(true);
                os << "HTTP/1.0 404 Ok\r\n"
                      "Content-Type: text/html; charset=\"utf-8\"\r\n"
                      "\r\n"
                      "<h1>Nothing to see here</h1>\n";
            }

            socket->close();

            if (QTcpSocket::UnconnectedState==socket->state()) {
                delete socket;
            }
        }
    }
}

void HttpSocket::discardClient()
{
    QTcpSocket *socket = (QTcpSocket*)sender();
    socket->deleteLater();
}
