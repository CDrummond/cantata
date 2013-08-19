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

#include "config.h"
#include "httpsocket.h"
#include "httpserver.h"
#include "settings.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "cdparanoia.h"
#include "extractjob.h"
#endif
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QStringList>
#include <QTextStream>
#include <QFile>
#include <QUrl>
#include <QNetworkProxy>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMimeType>
#endif
#include <QFileInfo>
#ifdef TAGLIB_FOUND
#include <taglib/oggflacfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/vorbisfile.h>
#include <taglib/audioproperties.h>
#endif

static QString detectMimeType(const QString &file)
{
    #ifdef ENABLE_KDE_SUPPORT
    QString km=KMimeType::findByPath(file)->name();
    if (!km.isEmpty() && (km.startsWith("audio/") || km.startsWith("application/"))) {
        return QLatin1String("audio/x-ape")==km ? QLatin1String("audio/x-monkeys-audio") : km;
    }
    #endif
    QString suffix = QFileInfo(file).suffix().toLower();
    if (suffix == QLatin1String("mp3")) {
        return QLatin1String("audio/mpeg");
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
            mime=QLatin1String("audio/x-vorbis+ogg");
        }
        delete result;
        if (mime.isEmpty()) {
            result = new TagLib::Ogg::FLAC::File(encodedName, false, TagLib::AudioProperties::Fast);
            if (result->isValid()) {
                mime=QLatin1String("audio/x-flac+ogg");
            }
            delete result;
        }
        if (mime.isEmpty()) {
            result = new TagLib::TrueAudio::File(encodedName, false, TagLib::AudioProperties::Fast);
            if (result->isValid()) {
                mime=QLatin1String("audio/x-speex+ogg");
            }
            delete result;
        }
        #ifdef TAGLIB_OPUS_FOUND
        if (mime.isEmpty()) {
            result = new TagLib::Ogg::Opus::File(encodedName, false, TagLib::AudioProperties::Fast);
            if (result->isValid()) {
                mime=QLatin1String("audio/x-opus+ogg");
            }
            delete result;
        }
        #endif
        return QLatin1String("audio/ogg");
    }
    #endif
    else if (suffix == QLatin1String("flac")) {
        return QLatin1String("audio/x-flac");
    } else if (suffix == QLatin1String("wma")) {
        return QLatin1String("audio/x-ms-wma");
    } else if (suffix == QLatin1String("m4a") || suffix == QLatin1String("m4b") || suffix == QLatin1String("m4p") || suffix == QLatin1String("mp4")) {
        return QLatin1String("audio/mp4");
    } else if (suffix == QLatin1String("wav")) {
        return QLatin1String("audio/x-wav");
    } else if (suffix == QLatin1String("wv") || suffix == QLatin1String("wvp")) {
        return QLatin1String("audio/x-wavpack");
    } else if (suffix == QLatin1String("ape")) {
        return QLatin1String("audio/x-monkeys-audio"); // "audio/x-ape";
    } else if (suffix == QLatin1String("spx")) {
        return QLatin1String("audio/x-speex");
    } else if (suffix == QLatin1String("tta")) {
        return QLatin1String("audio/x-tta");
    } else if (suffix == QLatin1String("aiff") || suffix == QLatin1String("aif") || suffix == QLatin1String("aifc")) {
        return QLatin1String("audio/x-aiff");
    } else if (suffix == QLatin1String("mpc") || suffix == QLatin1String("mpp") || suffix == QLatin1String("mp+")) {
        return QLatin1String("audio/x-musepack");
    } else if (suffix == QLatin1String("dff")) {
        return QLatin1String("application/x-dff");
    } else if (suffix == QLatin1String("dsf")) {
        return QLatin1String("application/x-dsf");
    } else if (suffix == QLatin1String("opus")) {
        return QLatin1String("audio/opus");
    }

    return QString();
}

static void writeMimeType(const QString &mimeType, QTcpSocket *socket, qint32 size, bool allowSeek)
{
    if (!mimeType.isEmpty()) {
        QTextStream os(socket);
        os.setAutoDetectUnicode(true);
        if (allowSeek) {
            os << "HTTP/1.0 200 OK"
               << "\r\nAccept-Ranges: bytes"
               << "\r\nContent-Length: " << QString::number(size)
               << "\r\nContent-Type: " << mimeType << "\r\n\r\n";
        } else {
            os << "HTTP/1.0 200 OK"
               << "\r\nContent-Length: " << QString::number(size)
               << "\r\nContent-Type: " << mimeType << "\r\n\r\n";
        }
    }
}

static QHostAddress getAddress(const QNetworkInterface &iface)
{
    QList<QNetworkAddressEntry> addresses=iface.addressEntries();
    foreach (const QNetworkAddressEntry &addr, addresses) {
        QHostAddress hostAddress=addr.ip();
        if (QAbstractSocket::IPv4Protocol==hostAddress.protocol()) {
            return hostAddress;
        }
    }
    return QHostAddress();
}

static int getSep(const QByteArray &a, int pos)
{
    for (int i=pos+1; i<a.length(); ++i) {
        if ('\n'==a[i] || '\r'==a[i] || ' '==a[i]) {
            return i;
        }
    }
    return -1;
}

static QList<QByteArray> split(const QByteArray &a)
{
    QList<QByteArray> rv;
    int lastPos=-1;
    for (;;) {
        int pos=getSep(a, lastPos);

        if (pos==(lastPos+1)) {
            lastPos++;
        } else if (pos>-1) {
            lastPos++;
            rv.append(a.mid(lastPos, pos-lastPos));
            lastPos=pos;
        } else {
            lastPos++;
            rv.append(a.mid(lastPos));
            break;
        }
    }
    return rv;
}

static bool isFromMpd(const QStringList &params)
{
    foreach (const QString &str, params) {
        if (str.startsWith("User-Agent:") && str.contains("Music Player Daemon")) {
            return true;
        }
    }
    return false;
}

static void getRange(const QStringList &params, qint32 &from, qint32 &to)
{
    foreach (const QString &str, params) {
        if (str.startsWith("Range:")) {
            int start=str.indexOf("bytes=");
            if (start>0) {
                QStringList range=str.mid(start+6).split("-", QString::SkipEmptyParts);
                if (1==range.length()) {
                    from=range.at(0).toLong();
                } else if (2==range.length()) {
                    from=range.at(0).toLong();
                    to=range.at(1).toLong();
                }
            }
            break;
        }
    }
}

HttpSocket::HttpSocket(const QString &iface, quint16 port)
    : QTcpServer(0)
    , cfgInterface(iface)
    , terminated(false)
{
    // Get network address...
    QHostAddress a;
    if (!iface.isEmpty()) {
        // Try rquested interface...
        QNetworkInterface netIface=QNetworkInterface::interfaceFromName(iface);
        if (netIface.isValid()) {
            a=getAddress(netIface);
        }
    }

    if (a.isNull()) {
        // Hmm... try first active interface...
        QNetworkInterface loIface;
        QList<QNetworkInterface> ifaces=QNetworkInterface::allInterfaces();
        foreach (const QNetworkInterface &iface, ifaces) {
            if (iface.flags()&QNetworkInterface::IsUp) {
                if (QLatin1String("lo")==iface.name()) {
                    loIface=iface;
                } else {
                    a=getAddress(iface);
                    if (!a.isNull()) {
                        break;
                    }
                }
            }
        }
        if (a.isNull() && !loIface.isValid()) {
            // Get address of 'loopback' interface...
            a=getAddress(loIface);
        }
    }

    if (!openPort(a, port)) {
        openPort(a, 0);
    }

    if (isListening() && ifaceAddress.isEmpty()) {
        ifaceAddress=QLatin1String("127.0.0.1");
    }
}

bool HttpSocket::openPort(const QHostAddress &a, quint16 p)
{
    if (!a.isNull() && listen(a, p)) {
        ifaceAddress=a.toString();
        return true;
    }
    // Listen probably failed due to proxy, so unset and try again!
    setProxy(QNetworkProxy::NoProxy);
    if (!a.isNull() && listen(a, p)) {
        ifaceAddress=a.toString();
        return true;
    }

    if (listen(QHostAddress::Any, p)) {
        ifaceAddress=serverAddress().toString();
        return true;
    }

    if (listen(QHostAddress::LocalHost, p)) {
        ifaceAddress=QLatin1String("127.0.0.1");
        return true;
    }

    return false;
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
        QList<QByteArray> tokens = split(socket->readLine()); // QRegExp("[ \r\n][ \r\n]*"));
        if (tokens.length()>=2 && "GET"==tokens[0]) {
            QStringList params = QString(socket->readAll()).split(QRegExp("[\r\n][\r\n]*"));

            if (!isFromMpd(params)) {
                return;
            }

            #if QT_VERSION < 0x050000
            QUrl url(QUrl::fromEncoded(tokens[1]));
            QUrl &q=url;
            #else
            QUrl url(QUrl::fromEncoded(tokens[1]));
            QUrlQuery q(url);
            #endif
            bool ok=false;
            qint32 readBytesFrom=0;
            qint32 readBytesTo=0;
            getRange(params, readBytesFrom, readBytesTo);

            if (q.hasQueryItem("cantata")) {
                Song song=HttpServer::self()->decodeUrl(url);
                if (song.isCdda()) {
                    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
                    QStringList parts=song.file.split("/", QString::SkipEmptyParts);
                    if (parts.length()>=3) {
                        QString dev=QLatin1Char('/')+parts.at(1)+QLatin1Char('/')+parts.at(2);
                        CdParanoia cdparanoia(dev, false, false, true);

                        if (cdparanoia) {
                            int firstSector = cdparanoia.firstSectorOfTrack(song.id);
                            int lastSector = cdparanoia.lastSectorOfTrack(song.id);
                            qint32 totalSize = ((lastSector-firstSector)+1)*CD_FRAMESIZE_RAW;
                            int count = 0;
                            //int bytesToDiscard = 0; // Number of bytes to discard in first read sector due to range request in HTTP header

                            //if (readBytesFrom>0) {
                            //    int sectorsToSeek=readBytesFrom/CD_FRAMESIZE_RAW;
                            //    firstSector+=sectorsToSeek;
                            //    bytesToDiscard=readBytesFrom-(sectorsToSeek*CD_FRAMESIZE_RAW);
                            //}
                            cdparanoia.seek(firstSector, SEEK_SET);
                            ok=true;
                            writeMimeType(QLatin1String("audio/x-wav"), socket, totalSize+ExtractJob::constWavHeaderSize, false);
                            //if (0==readBytesFrom) { // Only write header if we are no seeking...
                                ExtractJob::writeWavHeader(*socket, totalSize);
                            //}
                            bool stop=false;
                            while (!terminated && (firstSector+count) <= lastSector && !stop) {
                                qint16 *buf = cdparanoia.read();
                                if (!buf) {
                                    break;
                                }
                                char *buffer=(char *)buf;
                                qint32 writePos=0;
                                qint32 toWrite=CD_FRAMESIZE_RAW;

                                //if (0==count && bytesToDiscard) {
                                //    writePos=bytesToDiscard;
                                //    toWrite-=bytesToDiscard;
                                //    bytesToDiscard=0;
                                //}

                                do {
                                    qint32 bytesWritten=socket->write(&buffer[writePos], toWrite - writePos);
                                    if (terminated || -1==bytesWritten) {
                                        stop=true;
                                        break;
                                    }
                                    socket->flush();
                                    writePos+=bytesWritten;
                                } while (!terminated && writePos<toWrite);
                                count++;
                            }
                        }
                    }
                    #endif
                } else {
                    QFile f(url.path());

                    if (f.open(QIODevice::ReadOnly)) {                        
                        qint32 totalBytes = f.size();

                        writeMimeType(detectMimeType(url.path()), socket, totalBytes, true);
                        ok=true;
                        static const int constChunkSize=32768;
                        char buffer[constChunkSize];
                        qint32 readPos = 0;
                        qint32 bytesRead = 0;
                        bool stop=false;

                        if (0!=readBytesFrom) {
                            if (!f.seek(readBytesFrom)) {
                                ok=false;
                            }
                            bytesRead+=readBytesFrom;
                        }

                        if (0!=readBytesTo && readBytesTo>readBytesFrom && readBytesTo!=totalBytes) {
                            totalBytes-=(totalBytes-readBytesTo);
                        }

                        if (ok) {
                            do {
                                bytesRead = f.read(buffer, constChunkSize);
                                readPos+=bytesRead;
                                if (bytesRead<0 || terminated) {
                                    break;
                                }

                                qint32 writePos=0;
                                do {
                                    qint32 bytesWritten = socket->write(&buffer[writePos], bytesRead - writePos);
                                    if (terminated || -1==bytesWritten) {
                                        stop=true;
                                        break;
                                    }
                                    socket->flush();
                                    writePos+=bytesWritten;
                                } while (writePos<bytesRead);

                                if (f.atEnd()) {
                                    break;
                                }
                                if (QAbstractSocket::ConnectedState==socket->state()) {
                                    socket->waitForBytesWritten();
                                }
                                if (QAbstractSocket::ConnectedState!=socket->state()) {
                                    break;
                                }
                            } while ((readPos+bytesRead)<totalBytes && !stop && !terminated);
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
