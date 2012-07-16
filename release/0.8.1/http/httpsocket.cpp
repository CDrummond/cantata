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
#include <QtNetwork/QNetworkInterface>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QUrl>

// static int level(const QString &s)
// {
//     return QLatin1String("Link-local")==s
//             ? 1
//             : QLatin1String("Site-local")==s
//                 ? 2
//                 : QLatin1String("Global")==s
//                     ? 3
//                     : 0;
// }

HttpSocket::HttpSocket(const QString &addr, quint16 p)
    : QTcpServer(0)
    , cfgAddr(addr)
    , terminated(false)
{
    QHostAddress a;
    if (!addr.isEmpty()) {
        a=QHostAddress(addr);

        if (a.isNull()) {
            QString ifaceName=addr;
//             bool ipV4=true;
//
//             if (ifaceName.endsWith("::6")) {
//                 ifaceName=ifaceName.left(ifaceName.length()-3);
//                 ipV4=false;
//             }

            QNetworkInterface iface=QNetworkInterface::interfaceFromName(ifaceName);
            if (iface.isValid()) {
                QList<QNetworkAddressEntry> addresses=iface.addressEntries();
//                 int ip6Scope=-1;
                foreach (const QNetworkAddressEntry &addr, addresses) {
                    QHostAddress ha=addr.ip();
                    if (QAbstractSocket::IPv4Protocol==ha.protocol()) {
//                     if ((ipV4 && QAbstractSocket::IPv4Protocol==ha.protocol()) || (!ipV4 && QAbstractSocket::IPv6Protocol==ha.protocol())) {
//                         if (ipV4) {
                            a=ha;
                            break;
//                         } else {
//                             int scope=level(a.scopeId());
//                             if (scope>ip6Scope) {
//                                 ip6Scope=scope;
//                                 a=ha;
//                             }
//                         }
                    }
                }
            }
        }
    }
    listen(a.isNull() ? QHostAddress::LocalHost : a, p);
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
