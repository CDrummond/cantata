/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "localize.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#endif
#include <QtGui/QApplication>
#include <QtCore/QDebug>
#include <QtCore/QThread>
#include <QtCore/QStringList>
#include <QtCore/QFileInfo>
#include "debugtimer.h"

// #define DBUG qWarning() << "MPDConnection" << QThread::currentThreadId()
#define DBUG qDebug()

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(MPDConnection, conn)
#endif

MPDConnection * MPDConnection::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return conn;
    #else
    static MPDConnection *conn=0;;
    if (!conn) {
        conn=new MPDConnection;
    }
    return conn;
    #endif
}

static QByteArray encodeName(const QString &name)
{
    return '\"'+name.toUtf8().replace("\\", "\\\\").replace("\"", "\\\"")+'\"';
}

static QByteArray readFromSocket(MpdSocket &socket)
{
    QByteArray data;

    while (QAbstractSocket::ConnectedState==socket.state()) {
        while (socket.bytesAvailable() == 0 && QAbstractSocket::ConnectedState==socket.state()) {
            DBUG << (void *)(&socket) << "Waiting for read data.";
            if (socket.waitForReadyRead(5000)) {
                break;
            }
        }

        data.append(socket.readAll());

        if (data.endsWith("OK\n") || data.startsWith("OK") || data.startsWith("ACK")) {
            break;
        }
    }
    if (data.size()>256) {
        DBUG << (void *)(&socket) << "Read (bytes):" << data.size();
    } else {
        DBUG << (void *)(&socket) << "Read:" << data;
    }

    return data;
}

MPDConnection::Response readReply(MpdSocket &socket)
{
    QByteArray data = readFromSocket(socket);
    return MPDConnection::Response(data.endsWith("OK\n"), data);
}

MPDConnection::Response::Response(bool o, const QByteArray &d)
    : ok(o)
    , data(d)
{
}

QString MPDConnection::Response::getError()
{
    if (ok || data.isEmpty()) {
        return QString();
    }

    if (!ok && data.size()>0) {
//         int codeStart=data.indexOf(" [");
//         int codeEnd=data.indexOf("@", codeStart);
//         int cmdStart=data.indexOf(" {", codeEnd);
        int cmdEnd=data.indexOf("} ");
        if (-1!=cmdEnd) {
//             int code=data.mid(codeStart+2, codeEnd-(codeStart+2)).toInt();
//
//             #ifdef ENABLE_KDE_SUPPORT
//             switch (code) {
//             case 1: // MPD_SERVER_ERROR_NOT_LIST
//             case 2: // MPD_SERVER_ERROR_ARG
//             case 3: // MPD_SERVER_ERROR_PASSWORD
//                 break;
//             case 4: // MPD_SERVER_ERROR_PERMISSION
//                 return i18n("Permission denied");
//             case 5: // MPD_SERVER_ERROR_UNKNOWN_CMD
//                 return i18n("%1 - Uknown command", QString::fromUtf8(data.mid(cmdStart+2, cmdEnd-(cmdStart+2))));
//             case 50: // MPD_SERVER_ERROR_NO_EXIST
//                 return i18n("Song does not exist");
//             case 51: // MPD_SERVER_ERROR_PLAYLIST_MAX
//                 return i18n("Playlist limit reached");
//             case 52: // MPD_SERVER_ERROR_SYSTEM
//             case 53: // MPD_SERVER_ERROR_PLAYLIST_LOAD
//             case 54: // MPD_SERVER_ERROR_UPDATE_ALREADY
//             case 55: // MPD_SERVER_ERROR_PLAYER_SYNC
//             case 56: // MPD_SERVER_ERROR_EXIST
//             default:
//                 break;
//             }
//             #else
//              switch (code) {
//             case 1: // MPD_SERVER_ERROR_NOT_LIST
//             case 2: // MPD_SERVER_ERROR_ARG
//             case 3: // MPD_SERVER_ERROR_PASSWORD
//                 break;
//             case 4: // MPD_SERVER_ERROR_PERMISSION
//                 return QObject::tr("Permission denied");
//             case 5: // MPD_SERVER_ERROR_UNKNOWN_CMD
//                 return QObject::tr("%1 - Uknown command").arg(QString::fromUtf8(data.mid(cmdStart+2, cmdEnd-(cmdStart+2))));
//             case 50: // MPD_SERVER_ERROR_NO_EXIST
//                 return QObject::tr("Song does not exist");
//             case 51: // MPD_SERVER_ERROR_PLAYLIST_MAX
//                 return QObject::tr("Playlist limit reached");
//             case 52: // MPD_SERVER_ERROR_SYSTEM
//             case 53: // MPD_SERVER_ERROR_PLAYLIST_LOAD
//             case 54: // MPD_SERVER_ERROR_UPDATE_ALREADY
//             case 55: // MPD_SERVER_ERROR_PLAYER_SYNC
//             case 56: // MPD_SERVER_ERROR_EXIST
//             default:
//                 break;
//             }
//             #endif
            cmdEnd+=2;
            data=data.mid(cmdEnd, data.length()-(data.endsWith('\n') ? cmdEnd+1 : cmdEnd));
        }
    }
    return data;
}

MPDConnection::MPDConnection()
    : ver(0)
    , sock(this)
    , idleSocket(this)
    , state(State_Blank)
{
    qRegisterMetaType<Song>("Song");
    qRegisterMetaType<Output>("Output");
    qRegisterMetaType<Playlist>("Playlist");
    qRegisterMetaType<QList<Song> >("QList<Song>");
    qRegisterMetaType<QList<Output> >("QList<Output>");
    qRegisterMetaType<QList<Playlist> >("QList<Playlist>");
    qRegisterMetaType<QList<quint32> >("QList<quint32>");
    qRegisterMetaType<QList<qint32> >("QList<qint32>");
    qRegisterMetaType<QAbstractSocket::SocketState >("QAbstractSocket::SocketState");
    qRegisterMetaType<MPDStats>("MPDStats");
    qRegisterMetaType<MPDStatusValues>("MPDStatusValues");
}

MPDConnection::~MPDConnection()
{
//     disconnect(&sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(readyRead()), this, SLOT(idleDataReady()));
    sock.disconnectFromHost();
    idleSocket.disconnectFromHost();
}

MPDConnection::ConnectionReturn MPDConnection::connectToMPD(MpdSocket &socket, bool enableIdle)
{
    if (QAbstractSocket::ConnectedState!=socket.state()) {
        DBUG << (void *)(&socket) << "Connecting" << (enableIdle ? "(idle)" : "(std)");
        if (hostname.isEmpty() || port == 0) {
            DBUG << "no hostname and/or port supplied.";
            return Failed;
        }

        socket.connectToHost(hostname, port);
        if (socket.waitForConnected(5000)) {
            DBUG << (void *)(&socket) << "established";
            QByteArray recvdata = readFromSocket(socket);

            if (recvdata.startsWith("OK MPD")) {
                DBUG << (void *)(&socket) << "Received identification string";
            }

            lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=0;
            playQueueIds.clear();
            int min, maj, patch;
            if (3==sscanf(&(recvdata.constData()[7]), "%d.%d.%d", &maj, &min, &patch)) {
                long v=((maj&0xFF)<<16)+((min&0xFF)<<8)+(patch&0xFF);
                if (v!=ver) {
                    ver=v;
                    emit version(ver);
                }
            }

            recvdata.clear();

            if (!password.isEmpty()) {
                DBUG << (void *)(&socket) << "setting password...";
                socket.write("password "+password.toUtf8()+"\n");
                socket.waitForBytesWritten(5000);
                if (!readReply(socket).ok) {
                    DBUG << (void *)(&socket) << "password rejected";
                    socket.close();
                    return IncorrectPassword;
                }
            }

            if (enableIdle) {
                connect(&socket, SIGNAL(readyRead()), this, SLOT(idleDataReady()), Qt::QueuedConnection);
                connect(&socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)), Qt::QueuedConnection);
                DBUG << (void *)(&socket) << "Enabling idle";
                socket.write("idle\n");
                socket.waitForBytesWritten();
            }
            return Success;
        } else {
            DBUG << (void *)(&socket) << "Couldn't connect";
            return Failed;
        }
    }

//     DBUG << "Already connected" << (enableIdle ? "(idle)" : "(std)");
    return Success;
}

MPDConnection::ConnectionReturn MPDConnection::connectToMPD()
{
    if (State_Connected==state && (QAbstractSocket::ConnectedState!=sock.state() || QAbstractSocket::ConnectedState!=idleSocket.state())) {
        DBUG << "Something has gone wrong with sockets, so disconnect";
        disconnectFromMPD();
    }
    ConnectionReturn status=Failed;
    if (Success==(status=connectToMPD(sock)) && Success==(status=connectToMPD(idleSocket, true))) {
        state=State_Connected;
    } else {
        disconnectFromMPD();
        state=State_Disconnected;
    }

    return status;
}

void MPDConnection::disconnectFromMPD()
{
    DBUG << "disconnectFromMPD";
    disconnect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(readyRead()), this, SLOT(idleDataReady()));
    if (QAbstractSocket::ConnectedState==sock.state()) {
        sock.disconnectFromHost();
    }
    if (QAbstractSocket::ConnectedState==idleSocket.state()) {
        idleSocket.disconnectFromHost();
    }
    sock.close();
    idleSocket.close();
    state=State_Disconnected;
}

void MPDConnection::setDetails(const QString &host, quint16 p, const QString &pass)
{
    if (hostname!=host || (!sock.isLocal() && port!=p) || password!=pass || State_Connected!=state) {
        DBUG << "setDetails" << host << p << (pass.isEmpty() ? false : true);
        bool wasConnected=State_Connected==state;
        disconnectFromMPD();
        hostname=host;
        port=p;
        password=pass;
        DBUG << "call connectToMPD";
        switch (connectToMPD()) {
        case Success:
            getUrlHandlers();
            if (!wasConnected) {
                emit stateChanged(true);
            }
            break;
        case Failed:
            emit stateChanged(false);
            #ifdef ENABLE_KDE_SUPPORT
            if (host.startsWith('/')) {
                emit error(i18n("Connection to %1 failed", host), true);
            } else {
                emit error(i18nc("Connection to host:port", "Connection to %1:%2 failed", host, QString::number(MPDConnection::self()->getPort())), true);
            }
            #else
            if (host.startsWith('/')) {
                emit error(tr("Connection to %1 failed").arg(host), true);
            } else {
                emit error(tr("Connection to %1:%2 failed").arg(host).arg(QString::number(MPDConnection::self()->getPort())), true);
            }
            #endif
            break;
        case IncorrectPassword:
            emit stateChanged(false);
            #ifdef ENABLE_KDE_SUPPORT
            if (host.startsWith('/')) {
                emit error(i18n("Connection to %1 failed - incorrect password", host), true);
            } else {
                emit error(i18nc("Connection to host:port", "Connection to %1:%2 failed - incorrect password", host, QString::number(MPDConnection::self()->getPort())), true);
            }
            #else
            if (host.startsWith('/')) {
                emit error(tr("Connection to %1 failed - incorrect password").arg(host), true);
            } else {
                emit error(tr("Connection to %1:%2 failed - incorrect password").arg(host).arg(QString::number(MPDConnection::self()->getPort())), true);
            }
            #endif
            break;
        }
    }
}

MPDConnection::Response MPDConnection::sendCommand(const QByteArray &command, bool emitErrors, bool retry)
{
    DBUG << (void *)(&sock) << "sendCommand:" << command << emitErrors << retry;

    if (QAbstractSocket::ConnectedState!=sock.state()) {
        sock.close();
        if (Success!=connectToMPD(sock)) {
            // Failed to connect, so close *both* sockets!
            disconnectFromMPD();
            emit stateChanged(false);
            return Response(false);
        }
    }

    sock.write(command+"\n");
    sock.waitForBytesWritten(5000);
    Response response=readReply(sock);

    if (!response.ok) {
        DBUG << command << "failed";
        if (response.data.isEmpty() && retry && QAbstractSocket::ConnectedState!=sock.state()) {
            // Try one more time...
            // This scenario, where socket seems to be closed during/after 'write' seems to occor more often
            // when dynamizer is running. However, simply reconnecting seems to resolve the issue.
            qWarning() << "Socket is not connected, so try reconnecting and re-send command";
            return sendCommand(command, emitErrors, false);
        }
        if (emitErrors) {
            if ((command.startsWith("add ") || command.startsWith("command_list_begin\nadd ")) && -1!=command.indexOf("\"file:///")) {
                if (isLocal() && response.data=="Permission denied") {
                    emit error(i18n("Failed to load. Please check user \"mpd\" has read permission."));
                } else if (!isLocal() && response.data=="Access denied") {
                    emit error(i18n("Failed to load. MPD can only play local files if connected via a local socket."));
                } else {
                    emit error(response.getError());
                }
            } else {
                emit error(response.getError());
            }
        }
    }
    DBUG << (void *)(&sock) << "sendCommand - sent";
    return response;
}

/*
 * Playlist commands
 */
void MPDConnection::add(const QStringList &files, bool replace)
{
    if (replace) {
        clear();
        getStatus();
    }

    QByteArray send = "command_list_begin\n";
    for (int i = 0; i < files.size(); i++) {
        QString ext = QFileInfo(files.at(i)).suffix();
        if (!QString::compare(ext, "asx", Qt::CaseInsensitive) || !QString::compare(ext, "cue", Qt::CaseInsensitive) ||
            !QString::compare(ext, "m3u", Qt::CaseInsensitive) || !QString::compare(ext, "pls", Qt::CaseInsensitive) ||
            !QString::compare(ext, "xspf", Qt::CaseInsensitive)) {
            send+="load ";
        } else {
            send += "add ";
        }
        send += encodeName(files.at(i));
        send += "\n";
    }
    send += "command_list_end";

    if (sendCommand(send).ok) {
        if (replace && !files.isEmpty()) {
            startPlayingSong();
        }
        emit added(files);
    }
}

/**
 * Add all the files in the QStringList to the playlist at
 * postition post.
 *
 * NOTE: addid is not fully supported in < 0.14 So add everything
 * and then move everything to the right spot.
 *
 * @param files A QStringList with all the files to add
 * @param pos Position to add the files
 * @param size The size of the current playlist
 */
void MPDConnection::addid(const QStringList &files, quint32 pos, quint32 size, bool replace)
{
    if (replace) {
        clear();
        getStatus();
    }

    QByteArray send = "command_list_begin\n";
    int cur_size = size;
    for (int i = 0; i < files.size(); i++) {
        send += "add ";
        send += encodeName(files.at(i));
        send += "\n";
        send += "move ";
        send += QByteArray::number(cur_size);
        send += " ";
        send += QByteArray::number(pos);
        send += "\n";
        cur_size++;
    }

    send += "command_list_end";

    if (sendCommand(send).ok) {
        emit added(files);
    }
}

void MPDConnection::clear()
{
    if (sendCommand("clear").ok) {
        lastUpdatePlayQueueVersion=0;
        playQueueIds.clear();
    }
}

void MPDConnection::removeSongs(const QList<qint32> &items)
{
    QByteArray send = "command_list_begin\n";

    foreach (qint32 i, items) {
        send += "deleteid ";
        send += QByteArray::number(i);
        send += "\n";
    }

    send += "command_list_end";
    sendCommand(send);
}

void MPDConnection::move(quint32 from, quint32 to)
{
    sendCommand("move "+QByteArray::number(from)+' '+QByteArray::number(to));
}

void MPDConnection::move(const QList<quint32> &items, quint32 pos, quint32 size)
{
    doMoveInPlaylist(QString(), items, pos, size);
    #if 0
    QByteArray send = "command_list_begin\n";
    QList<quint32> moveItems;

    moveItems.append(items);
    qSort(moveItems);

    int posOffset = 0;

    //first move all items (starting with the biggest) to the end so we don't have to deal with changing rownums
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        if (moveItems.at(i) < pos && moveItems.at(i) != size - 1) {
            // we are moving away an item that resides before the destinatino row, manipulate destination row
            posOffset++;
        }
        send += "move ";
        send += QByteArray::number(moveItems.at(i));
        send += " ";
        send += QByteArray::number(size - 1);
        send += "\n";
    }
    //now move all of them to the destination position
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        send += "move ";
        send += QByteArray::number(size - 1 - i);
        send += " ";
        send += QByteArray::number(pos - posOffset);
        send += "\n";
    }

    send += "command_list_end";
    sendCommand(send);
    #endif
}

void MPDConnection::shuffle()
{
    sendCommand("shuffle");
}

void MPDConnection::shuffle(quint32 from, quint32 to)
{
    sendCommand("shuffle "+QByteArray::number(from)+':'+QByteArray::number(to+1));
}

void MPDConnection::currentSong()
{
    Response response=sendCommand("currentsong");
    if (response.ok) {
        emit currentSongUpdated(MPDParseUtils::parseSong(response.data));
    }
}

/*
 * Call "plchangesposid" to recieve a list of positions+ids that have been changed since the last update.
 * If we have ids in this list that we don't know about, then these are new songs - so we call
 * "playlistinfo <pos>" to get the song information.
 *
 * Any songs that are know about, will actually be sent with empty data - as the playqueue model will
 * already hold these songs.
 */
void MPDConnection::playListChanges()
{
    if (0==lastUpdatePlayQueueVersion || 0==playQueueIds.size()) {
        playListInfo();
        return;
    }

    QByteArray data = "plchangesposid ";
    data += QByteArray::number(lastUpdatePlayQueueVersion);
    Response response=sendCommand(data);
    if (response.ok) {
        // We need an updated status so as to detect deletes at end of list...
        Response status=sendCommand("status");
        if (status.ok) {
            MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
            lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=sv.playlist;
            emit statusUpdated(sv);
            QList<MPDParseUtils::IdPos> changes=MPDParseUtils::parseChanges(response.data);
            if (!changes.isEmpty()) {
                bool first=true;
//                 quint32 firstPos=0;
                QList<Song> songs;
                QList<qint32> ids;
                QSet<qint32> prevIds=playQueueIds.toSet();
                QSet<qint32> strmIds;

                foreach (const MPDParseUtils::IdPos &idp, changes) {
                    if (first) {
                        first=false;
//                         firstPos=idp.pos;
                        if (idp.pos!=0) {
                            for (quint32 i=0; i<idp.pos; ++i) {
                                Song s;
                                s.id=playQueueIds.at(i);
                                songs.append(s);
                                ids.append(s.id);
                                if (streamIds.contains(s.id)) {
                                    strmIds.insert(s.id);
                                }
                            }
                        }
                    }

                    if (prevIds.contains(idp.id) && !streamIds.contains(idp.id)) {
                        Song s;
                        s.id=idp.id;
//                         s.pos=idp.pos;
                        songs.append(s);
                    } else {
                        // New song!
                        data = "playlistinfo ";
                        data += QByteArray::number(idp.pos);
                        response=sendCommand(data);
                        if (!response.ok) {
                            playListInfo();
                            return;
                        }
                        Song s=MPDParseUtils::parseSong(response.data);
                        s.setKey();
                        s.id=idp.id;
//                         s.pos=idp.pos;
                        songs.append(s);
                        if (s.isStream() && !s.isCantataStream()) {
                            strmIds.insert(s.id);
                        }
                    }
                    ids.append(idp.id);
                }

                #if 0 // Remove, as causes oddities when dynamizer is running - I think becase plchangesposid/status are two separate calls.
                // Dont think this section is ever called, but leave here to be safe!!!
                // For some reason if we have 10 songs in our playlist and we move pos 2 to pos 1, MPD sends all ids from pos 1 onwards
                if (firstPos+changes.size()<sv.playlistLength && (sv.playlistLength<=(unsigned int)playQueueIds.length())) {
                    for (quint32 i=firstPos+changes.size(); i<sv.playlistLength; ++i) {
                        Song s;
                        s.id=playQueueIds.at(i);
                        songs.append(s);
                        ids.append(s.id);
                        if (s.isStream() && !s.isCantataStream()) {
                            strmIds.insert(s.id);
                        }
                    }
                }
                #endif

                playQueueIds=ids;
                streamIds=strmIds;
                emit playlistUpdated(songs);
                return;
            }
        }
    }

    playListInfo();
}

void MPDConnection::playListInfo()
{
    Response response=sendCommand("playlistinfo");
    if (response.ok) {
        lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion;
        QList<Song> songs=MPDParseUtils::parseSongs(response.data);
        playQueueIds.clear();
        streamIds.clear();
        foreach (const Song &s, songs) {
            playQueueIds.append(s.id);
            if (s.isStream() && !s.isCantataStream()) {
                streamIds.insert(s.id);
            }
        }
        emit playlistUpdated(songs);
    }
}

/*
 * Playback commands
 */
void MPDConnection::setCrossFade(int secs)
{
    sendCommand("crossfade "+QByteArray::number(secs));
}

void MPDConnection::setReplayGain(const QString &v)
{
    sendCommand("replay_gain_mode "+v.toLatin1());
}

void MPDConnection::getReplayGain()
{
    QStringList lines=QString(sendCommand("replay_gain_status").data).split('\n', QString::SkipEmptyParts);

    if (2==lines.count() && "OK"==lines[1] && lines[0].startsWith(QLatin1String("replay_gain_mode: "))) {
        emit replayGain(lines[0].mid(18));
    } else {
        emit replayGain(QString());
    }
}

void MPDConnection::goToNext()
{
    sendCommand("next");
}

static inline QByteArray value(bool b)
{
    return b ? "1" : "0";
}

void MPDConnection::setPause(bool toggle)
{
    sendCommand("pause "+value(toggle));
}

void MPDConnection::startPlayingSong(quint32 song)
{
    sendCommand("play "+QByteArray::number(song));
}

void MPDConnection::startPlayingSongId(quint32 songId)
{
    sendCommand("playid "+QByteArray::number(songId));
}

void MPDConnection::goToPrevious()
{
    sendCommand("previous");
}

void MPDConnection::setConsume(bool toggle)
{
    sendCommand("consume "+value(toggle));
}

void MPDConnection::setRandom(bool toggle)
{
    sendCommand("random "+value(toggle));
}

void MPDConnection::setRepeat(bool toggle)
{
    sendCommand("repeat "+value(toggle));
}

void MPDConnection::setSingle(bool toggle)
{
    sendCommand("single "+value(toggle));
}

void MPDConnection::setSeek(quint32 song, quint32 time)
{
    sendCommand("seek "+QByteArray::number(song)+' '+QByteArray::number(time));
}

void MPDConnection::setSeekId(quint32 songId, quint32 time)
{
    sendCommand("seekid "+QByteArray::number(songId)+' '+QByteArray::number(time));
}

void MPDConnection::setVolume(int vol)
{
    sendCommand("setvol "+QByteArray::number(vol));
}

void MPDConnection::stopPlaying()
{
    sendCommand("stop");
}

void MPDConnection::getStats()
{
    Response response=sendCommand("stats");
    if (response.ok) {
        emit statsUpdated(MPDParseUtils::parseStats(response.data));
    }
}

void MPDConnection::getStatus()
{
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        lastStatusPlayQueueVersion=sv.playlist;
        emit statusUpdated(sv);
    }
}

void MPDConnection::getUrlHandlers()
{
    Response response=sendCommand("urlhandlers");
    if (response.ok) {
        emit urlHandlers(MPDParseUtils::parseUrlHandlers(response.data));
    }
}

/*
 * Data is written during idle.
 * Retrieve it and parse it
 */
void MPDConnection::idleDataReady()
{
    DBUG << "idleDataReady";
    QByteArray data;

    if (idleSocket.bytesAvailable() == 0) {
        return;
    }

    parseIdleReturn(readFromSocket(idleSocket));
}

/*
 * Socket state has changed, currently we only use this to gracefully
 * handle disconnects.
 */
void MPDConnection::onSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    if (socketState == QAbstractSocket::ClosingState){
        bool wasConnected=State_Connected==state;
        disconnect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
        DBUG << "onSocketStateChanged";
        if (QAbstractSocket::ConnectedState==idleSocket.state()) {
            idleSocket.disconnectFromHost();
        }
        idleSocket.close();
        if (wasConnected && Success!=connectToMPD(idleSocket, true)) {
            // Failed to connect idle socket - so close *both*
            disconnectFromMPD();
            emit stateChanged(false);
        }
        if (QAbstractSocket::ConnectedState==idleSocket.state()) {
            connect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
        }
    }
}

/*
 * Parse data returned by the mpd deamon on an idle commond.
 */
void MPDConnection::parseIdleReturn(const QByteArray &data)
{
    DBUG << "parseIdleReturn:" << data;

    Response response(data.endsWith("OK\n"), data);
    if (response.ok) {
        DBUG << (void *)(&idleSocket) << "write idle";
        idleSocket.write("idle\n");
        idleSocket.waitForBytesWritten();
    } else {
        DBUG << "idle failed? reconnect";
        idleSocket.close();
        if (Success!=connectToMPD(idleSocket, true)) {
            // Failed to connect idle socket - so close *both*
            disconnectFromMPD();
            emit stateChanged(false);
        }
        return;
    }

    QList<QByteArray> lines = data.split('\n');

    /*
     * See http://www.musicpd.org/doc/protocol/ch02.html
     */
    bool playListUpdated=false;
    foreach(const QByteArray &line, lines) {
        if (line == "changed: database") {
            /*
             * Temp solution
             */
            getStats();
            playListInfo();
            playListUpdated=true;
        } else if (line == "changed: update") {
            emit databaseUpdated();
        } else if (line == "changed: stored_playlist") {
            emit storedPlayListUpdated();
        } else if (line == "changed: playlist") {
            if (!playListUpdated) {
                playListChanges();
            }
        } else if (line == "changed: player") {
            getStatus();
        } else if (line == "changed: mixer") {
            getStatus();
        } else if (line == "changed: output") {
            outputs();
        } else if (line == "changed: options") {
            getStatus();
        } else if (line == "OK" || line.startsWith("OK MPD ") || line.isEmpty()) {
            ;
        } else {
            DBUG << "Unknown command in idle return: " << line;
        }
    }
}

void MPDConnection::outputs()
{
    Response response=sendCommand("outputs");
    if (response.ok) {
        emit outputsUpdated(MPDParseUtils::parseOuputs(response.data));
    }
}

void MPDConnection::enableOutput(int id)
{
    sendCommand("enableoutput "+QByteArray::number(id));
}

void MPDConnection::disableOutput(int id)
{
    sendCommand("disableoutput "+QByteArray::number(id));
}

/*
 * Admin commands
 */
void MPDConnection::update()
{
    sendCommand("update");
}

/*
 * Database commands
 */

/**
 * Get all files in the playlist with detailed info (artist, album,
 * title, time etc).
 *
 * @param db_update The last update time of the library
 */
void MPDConnection::listAllInfo(const QDateTime &dbUpdate)
{
    TF_DEBUG
    emit updatingLibrary();
    Response response=sendCommand("listallinfo");
    if (response.ok) {
        emit musicLibraryUpdated(MPDParseUtils::parseLibraryItems(response.data), dbUpdate);
    }
    emit updatedLibrary();
}

/**
* Get all the files and dir in the mpdmusic dir.
*
*/
void MPDConnection::listAll()
{
    TF_DEBUG
    emit updatingFileList();
    Response response=sendCommand("listall");
    if (response.ok) {
        emit dirViewUpdated(MPDParseUtils::parseDirViewItems(response.data));
    }
    emit updatedFileList();
}

/*
 * Playlists commands
 */
// void MPDConnection::listPlaylist(const QString &name)
// {
//     QByteArray data = "listplaylist ";
//     data += encodeName(name);
//     sendCommand(data);
// }

void MPDConnection::listPlaylists()
{
    TF_DEBUG
    Response response=sendCommand("listplaylists");
    if (response.ok) {
        emit playlistsRetrieved(MPDParseUtils::parsePlaylists(response.data));
    }
}

void MPDConnection::playlistInfo(const QString &name)
{
    TF_DEBUG
    Response response=sendCommand("listplaylistinfo "+encodeName(name));
    if (response.ok) {
        emit playlistInfoRetrieved(name, MPDParseUtils::parseSongs(response.data));
    }
}

void MPDConnection::loadPlaylist(const QString &name, bool replace)
{
    if (replace) {
        clear();
        getStatus();
    }

    if (sendCommand("load "+encodeName(name)).ok) {
        emit playlistLoaded(name);
    }
}

void MPDConnection::renamePlaylist(const QString oldName, const QString newName)
{
    if (sendCommand("rename "+encodeName(oldName)+' '+encodeName(newName), false).ok) {
        emit playlistRenamed(oldName, newName);
    } else {
        #ifdef ENABLE_KDE_SUPPORT
        emit error(i18n("Failed to rename <b>%1</b> to <b>%2</b>").arg(oldName).arg(newName));
        #else
        emit error(tr("Failed to rename <b>%1</b> to <b>%2</b>").arg(oldName).arg(newName));
        #endif
    }
}

void MPDConnection::removePlaylist(const QString &name)
{
    sendCommand("rm "+encodeName(name));
}

void MPDConnection::savePlaylist(const QString &name)
{
    if (!sendCommand("save "+encodeName(name), false).ok) {
        #ifdef ENABLE_KDE_SUPPORT
        emit error(i18n("Failed to save <b>%1</b>").arg(name));
        #else
        emit error(tr("Failed to save <b>%1</b>").arg(name));
        #endif
    }
}

void MPDConnection::addToPlaylist(const QString &name, const QStringList &songs, quint32 pos, quint32 size)
{
    if (songs.isEmpty()) {
        return;
    }

    QByteArray encodedName=encodeName(name);
    QStringList added;

    foreach (const QString &s, songs) {
        QByteArray data = "playlistadd ";
        data += encodedName;
        data += " ";
        data += encodeName(s);
        if (sendCommand(data).ok) {
            added << s;
        } else {
            break;
        }
    }

    if (!added.isEmpty()) {
        if (size>0) {
            QList<quint32> items;
            for(int i=0; i<added.count(); ++i) {
                items.append(size+i);
            }
            doMoveInPlaylist(name, items, pos, size+added.count());
        }
    }

//     playlistInfo(name);
}

void MPDConnection::removeFromPlaylist(const QString &name, const QList<quint32> &positions)
{
    if (positions.isEmpty()) {
        return;
    }

    QByteArray encodedName=encodeName(name);
    QList<quint32> sorted=positions;
    QList<quint32> removed;

    qSort(sorted);

    for (int i=sorted.count()-1; i>=0; --i) {
        quint32 idx=sorted.at(i);
        QByteArray data = "playlistdelete ";
        data += encodedName;
        data += " ";
        data += QByteArray::number(idx);
        if (sendCommand(data).ok) {
            removed.prepend(idx);
        } else {
            break;
        }
    }

    if (removed.count()) {
        emit removedFromPlaylist(name, removed);
    }
//     playlistInfo(name);
}

void MPDConnection::moveInPlaylist(const QString &name, const QList<quint32> &items, quint32 pos, quint32 size)
{
    if (doMoveInPlaylist(name, items, pos, size)) {
        emit movedInPlaylist(name, items, pos);
    }
//     playlistInfo(name);
}

bool MPDConnection::doMoveInPlaylist(const QString &name, const QList<quint32> &items, quint32 pos, quint32 size)
{
    QByteArray cmd = name.isEmpty() ? "move " : ("playlistmove "+encodeName(name)+" ");
    QByteArray send = "command_list_begin\n";
    QList<quint32> moveItems=items;
    int posOffset = 0;

    qSort(moveItems);

    //first move all items (starting with the biggest) to the end so we don't have to deal with changing rownums
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        if (moveItems.at(i) < pos && moveItems.at(i) != size - 1) {
            // we are moving away an item that resides before the destinatino row, manipulate destination row
            posOffset++;
        }
        send += cmd;
        send += QByteArray::number(moveItems.at(i));
        send += " ";
        send += QByteArray::number(size - 1);
        send += "\n";
    }
    //now move all of them to the destination position
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        send += cmd;
        send += QByteArray::number(size - 1 - i);
        send += " ";
        send += QByteArray::number(pos - posOffset);
        send += "\n";
    }

    send += "command_list_end";
    return sendCommand(send).ok;
}

MpdSocket::MpdSocket(QObject *parent)
    : QObject(parent)
    , tcp(0)
    , local(0)
{
}

MpdSocket::~MpdSocket()
{
    deleteTcp();
    deleteLocal();
}

void MpdSocket::connectToHost(const QString &hostName, quint16 port, QIODevice::OpenMode mode)
{
//     qWarning() << "connectToHost" << hostName << port;
    if (hostName.startsWith('/')) {
        deleteTcp();
        if (!local) {
            local = new QLocalSocket(this);
            connect(local, SIGNAL(stateChanged(QLocalSocket::LocalSocketState)), this, SLOT(localStateChanged(QLocalSocket::LocalSocketState)));
            connect(local, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        }
//         qWarning() << "Connecting to LOCAL socket";
        local->connectToServer(hostName, mode);
    } else {
        deleteLocal();
        if (!tcp) {
            tcp = new QTcpSocket(this);
            connect(tcp, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SIGNAL(stateChanged(QAbstractSocket::SocketState)));
            connect(tcp, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        }
//         qWarning() << "Connecting to TCP socket";
        tcp->connectToHost(hostName, port, mode);
    }
}

void MpdSocket::localStateChanged(QLocalSocket::LocalSocketState state)
{
    emit stateChanged((QAbstractSocket::SocketState)state);
}

void MpdSocket::deleteTcp()
{
    if (tcp) {
        disconnect(tcp, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SIGNAL(stateChanged(QAbstractSocket::SocketState)));
        disconnect(tcp, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        tcp->deleteLater();
        tcp=0;
    }
}

void MpdSocket::deleteLocal()
{
    if (local) {
        disconnect(local, SIGNAL(stateChanged(QLocalSocket::LocalSocketState)), this, SLOT(localStateChanged(QLocalSocket::LocalSocketState)));
        disconnect(local, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        local->deleteLater();
        local=0;
    }
}
