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

#include "config.h"
#include "httpsocket.h"
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkInterface>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QUrl>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMimeType>
#else
#include <QtCore/QFileInfo>
#ifdef TAGLIB_FOUND
#include <taglib/oggflacfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/audioproperties.h>
#endif
#endif

static QString detectMimeType(const QString &file)
{
    #ifdef ENABLE_KDE_SUPPORT
    return KMimeType::findByPath(file)->name();
    #else
    QString suffix = QFileInfo(file).suffix();
    if (suffix == QLatin1String("mp3")) {
        return "audio/mpeg";
    }
    #ifdef TAGLIB_FOUND
    if (suffix == QLatin1String("ogg")) {
        #ifdef Q_OS_WIN32
        const wchar_t *encodedName = reinterpret_cast< const wchar_t * >(file.utf16());
        #elif defined COMPLEX_TAGLIB_FILENAME
        const wchar_t *encodedName = reinterpret_cast< const wchar_t * >(file.utf16());
        #else
        QByteArray fileName = QFile::encodeName(file);
        const char *encodedName = fileName.constData(); // valid as long as fileName exists
        #endif

        QString mime;
        TagLib::File *result = new TagLib::Ogg::Vorbis::File(encodedName, false, TagLib::AudioProperties::Fast);
        if (result->isValid()) {
            mime="audio/x-vorbis+ogg";
        }
        delete result;
        if (mime.isEmpty()) {
            result = new TagLib::Ogg::FLAC::File(encodedName, false, TagLib::AudioProperties::Fast);
            if (result->isValid()) {
                mime="audio/x-flac+ogg";
            }
            delete result;
        }
        return mime;
    }
    #endif
    else if (suffix == QLatin1String("flac")) {
        return "audio/x-flac";
    } else if (suffix == QLatin1String("wma")) {
        return "audio/x-ms-wma";
    }
    else if (suffix == QLatin1String("m4a") || suffix == QLatin1String("m4b") || suffix == QLatin1String("m4p") || suffix == QLatin1String("mp4")) {
         return "audio/mp4";
    }
    else if (suffix == QLatin1String("wav")) {
        return "audio/x-wav";
    }
    #endif
    return QString();
}

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
                    QString mimeType=detectMimeType(url.path());
                    if (!mimeType.isEmpty()) {
                        QTextStream os(socket);
                        os.setAutoDetectUnicode(true);
                        os << "HTTP/1.0 200 OK\r\nContent-Type: " << mimeType << "\r\n\r\n";
                    }

                    ok=true;
                    static const int constChunkSize=8192;
                    char buffer[constChunkSize];
                    qint64 totalBytes = f.size();
                    qint64 readPos = 0;
                    qint64 bytesRead = 0;

                    do {
                        if (terminated) {
                            break;
                        }
                        bytesRead = f.read(buffer, constChunkSize);
                        readPos+=bytesRead;
                        if (bytesRead<0 || terminated) {
                            break;
                        }

                        qint64 writePos=0;
                        do {
                            qint64 bytesWritten = socket->write(&buffer[writePos], bytesRead - writePos);
                            if (terminated) {
                                break;
                            }
                            if (-1==bytesWritten) {
                                ok=false;
                                break;
                            }
                            writePos+=bytesWritten;
                        } while (writePos<bytesRead);

                        if (f.atEnd()) {
                            break;
                        }
                    } while ((readPos+bytesRead)<totalBytes);
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
