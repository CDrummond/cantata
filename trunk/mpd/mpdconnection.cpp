/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpduser.h"
#include "localize.h"
#include "utils.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#endif
#include <QApplication>
#include <QStringList>
#include <QTimer>
#include "thread.h"
#include "settings.h"
#include "cuefile.h"
#ifndef Q_OS_WIN32
#include "powermanagement.h"
#endif
#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "MPDConnection" << QThread::currentThreadId()
void MPDConnection::enableDebug()
{
    debugEnabled=true;
}

static const int constSocketCommsTimeout=2000;
static const int constMaxReadAttempts=4;

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(MPDConnection, conn)
#endif

MPDConnection * MPDConnection::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return conn;
    #else
    static MPDConnection *conn=0;
    if (!conn) {
        conn=new MPDConnection;
    }
    return conn;
    #endif
}

QByteArray MPDConnection::encodeName(const QString &name)
{
    return '\"'+name.toUtf8().replace("\\", "\\\\").replace("\"", "\\\"")+'\"';
}

static QByteArray readFromSocket(MpdSocket &socket)
{
    QByteArray data;

    while (QAbstractSocket::ConnectedState==socket.state()) {
        int attempt=0;
        while (0==socket.bytesAvailable() && QAbstractSocket::ConnectedState==socket.state()) {
            DBUG << (void *)(&socket) << "Waiting for read data." << attempt;
            if (socket.waitForReadyRead(constSocketCommsTimeout)) {
                break;
            }
            if (++attempt>=constMaxReadAttempts) {
                DBUG << "ERROR: Timedout waiting for response";
                socket.close();
                return QByteArray();
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
        int cmdEnd=data.indexOf("} ");
        if (-1!=cmdEnd) {
            cmdEnd+=2;
            data=data.mid(cmdEnd, data.length()-(data.endsWith('\n') ? cmdEnd+1 : cmdEnd));
        }
    }
    return data;
}

MPDConnectionDetails::MPDConnectionDetails()
    : port(6600)
    , dynamizerPort(0)
    , dirReadable(false)
{
}

QString MPDConnectionDetails::getName() const
{
    return name.isEmpty() ? i18n("Default") : (name==MPDUser::constName ? MPDUser::translatedName() : name);
}

QString MPDConnectionDetails::description() const
{
    if (hostname.startsWith('/')) {
        return i18nc("name (host)", "\"%1\"", getName());
    } else {
        return i18nc("name (host:port)", "\"%1\" (%2:%3)", getName(), hostname, port);
    }
}

MPDConnection::MPDConnection()
    : thread(0)
    , ver(0)
    , sock(this)
    , idleSocket(this)
    , lastStatusPlayQueueVersion(0)
    , lastUpdatePlayQueueVersion(0)
    , state(State_Blank)
    , reconnectTimer(0)
    , reconnectStart(0)
    , stopAfterCurrent(false)
    , currentSongId(-1)
    , songPos(0)
    , unmuteVol(-1)
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
    qRegisterMetaType<MPDStatsValues>("MPDStatsValues");
    qRegisterMetaType<MPDStatusValues>("MPDStatusValues");
    qRegisterMetaType<MPDConnectionDetails>("MPDConnectionDetails");
    #ifndef Q_OS_WIN32
    connect(PowerManagement::self(), SIGNAL(resuming()), this, SLOT(reconnect()));
    #endif
}

MPDConnection::~MPDConnection()
{
//     disconnect(&sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(readyRead()), this, SLOT(idleDataReady()));
    sock.disconnectFromHost();
    idleSocket.disconnectFromHost();
}

void MPDConnection::start()
{
    if (!thread) {
        thread=new Thread(metaObject()->className());
        moveToThread(thread);
        thread->start();
    }
}

void MPDConnection::stop()
{
    if (details.name==MPDUser::constName && Settings::self()->stopOnExit()) {
        MPDUser::self()->stop();
    }

    if (thread) {
        thread->stop();
        thread=0;
    }
}

MPDConnection::ConnectionReturn MPDConnection::connectToMPD(MpdSocket &socket, bool enableIdle)
{
    if (QAbstractSocket::ConnectedState!=socket.state()) {
        DBUG << (void *)(&socket) << "Connecting" << (enableIdle ? "(idle)" : "(std)");
        if (details.isEmpty()) {
            DBUG << "no hostname and/or port supplied.";
            return Failed;
        }

        socket.connectToHost(details.hostname, details.port);
        if (socket.waitForConnected(constSocketCommsTimeout)) {
            DBUG << (void *)(&socket) << "established";
            QByteArray recvdata = readFromSocket(socket);

            if (recvdata.isEmpty()) {
                DBUG << (void *)(&socket) << "Couldn't connect";
                return Failed;
            }

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
                }
            }

            recvdata.clear();

            if (!details.password.isEmpty()) {
                DBUG << (void *)(&socket) << "setting password...";
                socket.write("password "+details.password.toUtf8()+"\n");
                socket.waitForBytesWritten(constSocketCommsTimeout);
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
    ver=0;
}

// This function is mainly intended to be called after the computer (laptop) has been 'resumed'
void MPDConnection::reconnect()
{
    if (reconnectTimer && reconnectTimer->isActive()) {
        return;
    }
    if (0==reconnectStart) {
        if (isConnected()) {
            disconnectFromMPD();
        } else {
            return;
        }
    }

    if (isConnected()) { // Perhaps the user pressed a button which caused the reconnect???
        reconnectStart=0;
        return;
    }

    time_t now=time(NULL);

    switch (connectToMPD()) {
    case Success:
        getStatus();
        getStats(false);
        getUrlHandlers();
        playListInfo();
        outputs();
        reconnectStart=0;
        emit stateChanged(true);
        break;
    case Failed:
        if (0==reconnectStart || abs(now-reconnectStart)<15) {
            if (0==reconnectStart) {
                reconnectStart=now;
            }
            if (!reconnectTimer) {
                reconnectTimer=new QTimer(this);
                reconnectTimer->setSingleShot(true);
                connect(reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()), Qt::QueuedConnection);
            }
            if (abs(now-reconnectStart)>1) {
                emit info(i18n("Connecting to %1", details.description()));
            }
            reconnectTimer->start(500);
        } else {
            emit stateChanged(false);
            emit error(i18n("Connection to %1 failed", details.description()), true);
            reconnectStart=0;
        }
        break;
    case IncorrectPassword:
        emit stateChanged(false);
        emit error(i18n("Connection to %1 failed - incorrect password", details.description()), true);
        reconnectStart=0;
        break;
    }
}

void MPDConnection::setDetails(const MPDConnectionDetails &d)
{
    bool isUser=d.name==MPDUser::constName;
    const MPDConnectionDetails &det=isUser ? MPDUser::self()->details() : d;
    bool changedDir=det.dir!=details.dir;
    bool diffName=det.name!=details.name;
    bool diffDetails=det!=details;
    bool diffDynamicPort=det.dynamizerPort!=details.dynamizerPort;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool diffStreamUrl=det.streamUrl!=details.streamUrl;
    #endif

    details=det;
    if (diffDetails || State_Connected!=state) {
        DBUG << "setDetails" << details.hostname << details.port << (details.password.isEmpty() ? false : true);
        disconnectFromMPD();
        DBUG << "call connectToMPD";
        unmuteVol=-1;
        toggleStopAfterCurrent(false);
        if (isUser) {
            MPDUser::self()->start();
        }
        switch (connectToMPD()) {
        case Success:
            getStatus();
            getStats(false);
            getUrlHandlers();
            emit stateChanged(true);
            break;
        case Failed:
            emit stateChanged(false);
            emit error(i18n("Connection to %1 failed", details.description()), true);
            break;
        case IncorrectPassword:
            emit stateChanged(false);
            emit error(i18n("Connection to %1 failed - incorrect password", details.description()), true);
            break;
        }
    } else if (diffName) {
         emit stateChanged(true);
    }
    if (diffDynamicPort) {
        if (details.dynamizerPort<=0) {
            dynamicUrl(QString());
        } else {
            emit dynamicUrl("http://"+(details.isLocal() ? "127.0.0.1" : details.hostname)+":"+QString::number(details.dynamizerPort));
        }
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    if (diffStreamUrl) {
        emit streamUrl(details.streamUrl);
    }
    #endif
    if (changedDir) {
        emit dirChanged();
    }
}

void MPDConnection::disconnectMpd()
{
    if (State_Connected==state) {
        disconnectFromMPD();
        emit stateChanged(false);
    }
}

MPDConnection::Response MPDConnection::sendCommand(const QByteArray &command, bool emitErrors, bool retry)
{
    DBUG << (void *)(&sock) << "sendCommand:" << command << emitErrors << retry;

    if (!isConnected()) {
        emit error(i18n("Failed to send command to %1 - not connected", details.description()), true);
        return Response(false);
    }

    if (QAbstractSocket::ConnectedState!=sock.state()) {
        sock.close();
        if (Success!=connectToMPD(sock)) {
            // Failed to connect, so close *both* sockets!
            disconnectFromMPD();
            emit stateChanged(false);
            emit error(i18n("Connection to %1 failed", details.description()), true);
            return Response(false);
        }
    }

    Response response;
    if (-1==sock.write(command+"\n")) {
        // If we fail to write, dont wait for bytes to be written!!
        response=Response(false);
        sock.close();
    } else {
        sock.waitForBytesWritten(constSocketCommsTimeout);
        response=readReply(sock);
    }

    if (!response.ok) {
        DBUG << command << "failed";
        if (response.data.isEmpty() && retry && QAbstractSocket::ConnectedState!=sock.state()) {
            // Try one more time...
            // This scenario, where socket seems to be closed during/after 'write' seems to occiu more often
            // when dynamizer is running. However, simply reconnecting seems to resolve the issue.
            return sendCommand(command, emitErrors, false);
        }
        if (emitErrors) {
            if ((command.startsWith("add ") || command.startsWith("command_list_begin\nadd ")) && -1!=command.indexOf("\"file:///")) {
                if (details.isLocal() && response.data=="Permission denied") {
                    emit error(i18n("Failed to load. Please check user \"mpd\" has read permission."));
                } else if (!details.isLocal() && response.data=="Access denied") {
                    emit error(i18n("Failed to load. MPD can only play local files if connected via a local socket."));
                } else if (!response.getError().isEmpty()) {
                    emit error(response.getError());
                } else {
                    disconnectFromMPD();
                    emit stateChanged(false);
                    emit error(i18n("Failed to send command. Disconnected from %1", details.description()), true);
                }
            } else if (!response.getError().isEmpty()) {
                emit error(response.getError());
            } else {
                disconnectFromMPD();
                emit stateChanged(false);
                emit error(i18n("Failed to send command. Disconnected from %1", details.description()), true);
            }
        }
    }
    DBUG << (void *)(&sock) << "sendCommand - sent";
    return response;
}

/*
 * Playlist commands
 */
bool MPDConnection::isPlaylist(const QString &file)
{
    return file.endsWith("asx", Qt::CaseInsensitive) || file.endsWith("cue", Qt::CaseInsensitive) ||
           file.endsWith("m3u", Qt::CaseInsensitive) || file.endsWith("pls", Qt::CaseInsensitive) ||
           file.endsWith("xspf", Qt::CaseInsensitive);
}

void MPDConnection::add(const QStringList &files, bool replace, quint8 priority)
{
    add(files, 0, 0, replace, priority);
}

void MPDConnection::add(const QStringList &files, quint32 pos, quint32 size, bool replace, quint8 priority)
{
    toggleStopAfterCurrent(false);
    if (replace) {
        clear();
        getStatus();
    }

    QByteArray send = "command_list_begin\n";
    int curSize = size;
    int curPos = pos;
//    bool addedFile=false;
    bool havePlaylist=false;
    bool usePrio=priority>0 && canUsePriority();
    for (int i = 0; i < files.size(); i++) {
        if (CueFile::isCue(files.at(i))) {
            send += "load "+CueFile::getLoadLine(files.at(i))+"\n";
        } else {
            if (isPlaylist(files.at(i))) {
                send+="load ";
                havePlaylist=true;
            } else {
//                addedFile=true;
                send += "add ";
            }
            send += encodeName(files.at(i))+"\n";
        }
        if (!havePlaylist) {
            if (0!=size) {
                send += "move "+QByteArray::number(curSize)+" "+QByteArray::number(curPos)+"\n";
            }
            if (usePrio && !havePlaylist) {
                send += "prio "+QByteArray::number(priority)+" "+QByteArray::number(curPos)+" "+QByteArray::number(curPos)+"\n";
            }
        }
        curSize++;
        curPos++;
    }

    send += "command_list_end";

    if (sendCommand(send).ok) {
        if (replace /*&& addedFile */&& !files.isEmpty()) {
            // Dont emit error if fail plays, might be that playlist was not loaded...
            sendCommand("play "+QByteArray::number(0), false);
        }
        emit added(files);
    }
}

void MPDConnection::addAndPlay(const QString &file)
{
    toggleStopAfterCurrent(false);
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        QByteArray send = "command_list_begin\n";
        send+="add "+encodeName(file)+"\n";
        send+="play "+QByteArray::number(sv.playlistLength)+"\n";
        send+="command_list_end";
        sendCommand(send);
    }
}

void MPDConnection::clear()
{
    toggleStopAfterCurrent(false);
    if (sendCommand("clear").ok) {
        lastUpdatePlayQueueVersion=0;
        playQueueIds.clear();
    }
}

void MPDConnection::removeSongs(const QList<qint32> &items)
{
    toggleStopAfterCurrent(false);
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
    toggleStopAfterCurrent(false);
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
    toggleStopAfterCurrent(false);
    sendCommand("shuffle");
}

void MPDConnection::shuffle(quint32 from, quint32 to)
{
    toggleStopAfterCurrent(false);
    sendCommand("shuffle "+QByteArray::number(from)+':'+QByteArray::number(to+1));
}

void MPDConnection::currentSong()
{
    Response response=sendCommand("currentsong");
    if (response.ok) {
        emit currentSongUpdated(MPDParseUtils::parseSong(response.data, true));
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
    Response status=sendCommand("status"); // We need an updated status so as to detect deletes at end of list...
    Response response=sendCommand(data);
    if (response.ok && status.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
        lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=sv.playlist;
        emit statusUpdated(sv);
        QList<MPDParseUtils::IdPos> changes=MPDParseUtils::parseChanges(response.data);
        if (!changes.isEmpty()) {
            bool first=true;
            quint32 firstPos=0;
            QList<Song> songs;
            QList<qint32> ids;
            QSet<qint32> prevIds=playQueueIds.toSet();
            QSet<qint32> strmIds;

            foreach (const MPDParseUtils::IdPos &idp, changes) {
                if (first) {
                    first=false;
                    firstPos=idp.pos;
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
//                     s.pos=idp.pos;
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
                    Song s=MPDParseUtils::parseSong(response.data, true);
                    s.id=idp.id;
//                     s.pos=idp.pos;
                    songs.append(s);
                    if (s.isStream() && !s.isCantataStream()) {
                        strmIds.insert(s.id);
                    }
                }
                ids.append(idp.id);
            }

            // Dont think this section is ever called, but leave here to be safe!!!
            // For some reason if we have 10 songs in our playlist and we move pos 2 to pos 1, MPD sends all ids from pos 1 onwards
            if (firstPos+changes.size()<=sv.playlistLength && (sv.playlistLength<=(unsigned int)playQueueIds.length())) {
                for (quint32 i=firstPos+changes.size(); i<sv.playlistLength; ++i) {
                    Song s;
                    s.id=playQueueIds.at(i);
                    songs.append(s);
                    ids.append(s.id);
                    if (streamIds.contains(s.id)) {
                        strmIds.insert(s.id);
                    }
                }
            }

            playQueueIds=ids;
            streamIds=strmIds;
            emit playlistUpdated(songs);
            return;
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
    toggleStopAfterCurrent(false);
    sendCommand("next");
}

static inline QByteArray value(bool b)
{
    return b ? "1" : "0";
}

void MPDConnection::setPause(bool toggle)
{
    toggleStopAfterCurrent(false);
    sendCommand("pause "+value(toggle));
}

void MPDConnection::startPlayingSong(quint32 song)
{
    toggleStopAfterCurrent(false);
    sendCommand("play "+QByteArray::number(song));
}

void MPDConnection::startPlayingSongId(qint32 songId)
{
    toggleStopAfterCurrent(false);
    sendCommand("playid "+QByteArray::number(songId));
}

void MPDConnection::goToPrevious()
{
    toggleStopAfterCurrent(false);
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
    toggleStopAfterCurrent(false);
    sendCommand("seek "+QByteArray::number(song)+' '+QByteArray::number(time));
}

void MPDConnection::setSeekId(qint32 songId, quint32 time)
{
    if (-1==songId) {
        songId=currentSongId;
    }
    if (-1==songId) {
        return;
    }
    if (songId!=currentSongId || 0==time) {
        toggleStopAfterCurrent(false);
    }
    if (sendCommand("seekid "+QByteArray::number(songId)+' '+QByteArray::number(time)).ok) {
        if (stopAfterCurrent && songId==currentSongId && songPos>time) {
            songPos=time;
        }
    }
}

void MPDConnection::setVolume(int vol)
{
    if (vol>=0) {
        unmuteVol=-1;
        sendCommand("setvol "+QByteArray::number(vol), false);
    }
}

void MPDConnection::toggleMute()
{
    if (unmuteVol>0) {
        sendCommand("setvol "+QByteArray::number(unmuteVol), false);
        unmuteVol=-1;
    } else {
        Response status=sendCommand("status");
        if (status.ok) {
            MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
            if (sv.volume>0) {
                unmuteVol=sv.volume;
                sendCommand("setvol "+QByteArray::number(0), false);
            }
        }
    }
}

void MPDConnection::stopPlaying(bool afterCurrent)
{
    toggleStopAfterCurrent(afterCurrent);
    if (!stopAfterCurrent) {
        sendCommand("stop");
    }
}

void MPDConnection::clearStopAfter()
{
    toggleStopAfterCurrent(false);
}

void MPDConnection::getStats(bool andUpdate)
{
    Response response=sendCommand(andUpdate ? "command_list_begin\nupdate\nstats\ncommand_list_end" : "stats");
    if (response.ok) {
        MPDStatsValues stats=MPDParseUtils::parseStats(response.data);
        dbUpdate=stats.dbUpdate;
        emit statsUpdated(stats);
    }
}

void MPDConnection::getStatus()
{
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        lastStatusPlayQueueVersion=sv.playlist;
        if (stopAfterCurrent && (currentSongId!=sv.songId || (songPos>0 && sv.timeElapsed<(qint32)songPos))) {
            if (sendCommand("stop").ok) {
                sv.state=MPDState_Stopped;
            }
            toggleStopAfterCurrent(false);
        }
        currentSongId=sv.songId;
        emit statusUpdated(sv);
    }
}

void MPDConnection::getUrlHandlers()
{
    Response response=sendCommand("urlhandlers");
    if (response.ok) {
        handlers=MPDParseUtils::parseUrlHandlers(response.data).toSet();
    }
}

/*
 * Data is written during idle.
 * Retrieve it and parse it
 */
void MPDConnection::idleDataReady()
{
    DBUG << "idleDataReady";
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
            emit error(i18n("Connection to %1 failed", details.description()), true);
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
            emit error(i18n("Connection to %1 failed", details.description()), true);
        }
        return;
    }

    QList<QByteArray> lines = data.split('\n');

    /*
     * See http://www.musicpd.org/doc/protocol/ch02.html
     */
    bool playListUpdated=false;
    bool statusUpdated=false;
    foreach(const QByteArray &line, lines) {
        if (line == "changed: database") {
            getStats(false);
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
        } else if (!statusUpdated && (line == "changed: player" || line == "changed: mixer" || line == "changed: options")) {
            getStatus();
            getReplayGain();
            statusUpdated=true;
        } else if (line == "changed: output") {
            outputs();
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

void MPDConnection::enableOutput(int id, bool enable)
{
    sendCommand((enable ? "enableoutput " : "disableoutput ")+QByteArray::number(id));
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
 */
void MPDConnection::loadLibrary()
{
    emit updatingLibrary();
    Response response=sendCommand("listallinfo");
    if (response.ok) {
        emit musicLibraryUpdated(MPDParseUtils::parseLibraryItems(response.data, details.dir, ver), dbUpdate);
    }
    emit updatedLibrary();
}

/**
* Get all the files and dir in the mpdmusic dir.
*
*/
void MPDConnection::loadFolders()
{
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
    Response response=sendCommand("listplaylists");
    if (response.ok) {
        emit playlistsRetrieved(MPDParseUtils::parsePlaylists(response.data));
    }
}

void MPDConnection::playlistInfo(const QString &name)
{
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
        emit error(i18n("Failed to rename <b>%1</b> to <b>%2</b>", oldName, newName));
    }
}

void MPDConnection::removePlaylist(const QString &name)
{
    sendCommand("rm "+encodeName(name));
}

void MPDConnection::savePlaylist(const QString &name)
{
    if (!sendCommand("save "+encodeName(name), false).ok) {
        emit error(i18n("Failed to save <b>%1</b>", name));
    }
}

void MPDConnection::addToPlaylist(const QString &name, const QStringList &songs, quint32 pos, quint32 size)
{
    if (songs.isEmpty()) {
        return;
    }

    if (!name.isEmpty()) {
        foreach (const QString &song, songs) {
            if (CueFile::isCue(song)) {
                emit error(i18n("You cannot add parts of a cue sheet to a playlist!"));
                return;
            } else if (isPlaylist(song)) {
                emit error(i18n("You cannot add a playlist to another playlist!"));
                return;
            }
        }
    }

    QByteArray encodedName=encodeName(name);
    QByteArray send = "command_list_begin\n";
    foreach (const QString &s, songs) {
        send += "playlistadd "+encodedName+" "+encodeName(s)+"\n";
    }
    send += "command_list_end";

    if (sendCommand(send).ok) {
        if (size>0) {
            QList<quint32> items;
            for(int i=0; i<songs.count(); ++i) {
                items.append(size+i);
            }
            doMoveInPlaylist(name, items, pos, size+songs.count());
        }
    } else {
        playlistInfo(name);
    }
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

void MPDConnection::setPriority(const QList<qint32> &ids, quint8 priority)
{
    if (canUsePriority()) {
        QByteArray send = "command_list_begin\n";

        foreach (quint32 id, ids) {
            send += "prioid "+QByteArray::number(priority)+" "+QByteArray::number(id)+"\n";
        }

        send += "command_list_end";
        if (sendCommand(send).ok) {
            emit prioritySet(ids, priority);
        }
    }
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
    if (name.isEmpty()) {
        toggleStopAfterCurrent(false);
    }
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

void MPDConnection::toggleStopAfterCurrent(bool afterCurrent)
{
    if (afterCurrent!=stopAfterCurrent) {
        stopAfterCurrent=afterCurrent;
        songPos=0;
        if (stopAfterCurrent && 1==playQueueIds.count()) {
            Response response=sendCommand("status");
            if (response.ok) {
                MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
                songPos=sv.timeElapsed;
            }
        }
        emit stopAfterCurrentChanged(stopAfterCurrent);
    }
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
