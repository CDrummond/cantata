/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "musiclibraryitemroot.h"
#include "mpduser.h"
#include "localize.h"
#include "utils.h"
#include "globalstatic.h"
#include <QApplication>
#include <QStringList>
#include <QTimer>
#include <QDir>
#include "thread.h"
#include "settings.h"
#include "cuefile.h"
#ifdef QT_QTDBUS_FOUND
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
static int maxFilesPerAddCommand=10000;
static bool alwaysUseLsInfo=false;

static const QByteArray constOkValue("OK");
static const QByteArray constOkMpdValue("OK MPD");
static const QByteArray constOkNlValue("OK\n");
static const QByteArray constAckValue("ACK");
static const QByteArray constIdleChangedKey("changed: ");
static const QByteArray constIdleDbValue("database");
static const QByteArray constIdleUpdateValue("update");
static const QByteArray constIdleStoredPlaylistValue("stored_playlist");
static const QByteArray constIdlePlaylistValue("playlist");
static const QByteArray constIdlePlayerValue("player");
static const QByteArray constIdleMixerValue("mixer");
static const QByteArray constIdleOptionsValue("options");
static const QByteArray constIdleOutputValue("output");

static inline int socketTimeout(int dataSize)
{
    static const int constDataBlock=100000;
    return ((dataSize/constDataBlock)+((dataSize%constDataBlock) ? 1 : 0))*constSocketCommsTimeout;
}

static QByteArray log(const QByteArray &data)
{
    if (data.length()<256) {
        return data;
    } else {
        return data.left(96) + "... ..." + data.right(96) + " (" + QByteArray::number(data.length()) + " bytes)";
    }
}

GLOBAL_STATIC(MPDConnection, instance)

QByteArray MPDConnection::quote(int val)
{
    return '\"'+QByteArray::number(val)+'\"';
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
            DBUG << (void *)(&socket) << "Waiting for read data, attempt" << attempt;
            if (socket.waitForReadyRead()) {
                break;
            }
            DBUG << (void *)(&socket) << "Wait for read failed";
            if (++attempt>=constMaxReadAttempts) {
                DBUG << "ERROR: Timedout waiting for response";
                socket.close();
                return QByteArray();
            }
        }

        data.append(socket.readAll());

        if (data.endsWith(constOkNlValue) || data.startsWith(constOkValue) || data.startsWith(constAckValue)) {
            break;
        }
    }
    DBUG << (void *)(&socket) << "Read:" << log(data) << ", socket state:" << socket.state();

    return data;
}

static MPDConnection::Response readReply(MpdSocket &socket)
{
    QByteArray data = readFromSocket(socket);
    return MPDConnection::Response(data.endsWith(constOkNlValue), data);
}

MPDConnection::Response::Response(bool o, const QByteArray &d)
    : ok(o)
    , data(d)
{
}

QString MPDConnection::Response::getError(const QByteArray &command)
{
    if (ok || data.isEmpty()) {
        return QString();
    }

    if (!ok && data.size()>0) {
        int cmdEnd=data.indexOf("} ");
        if (-1==cmdEnd) {
            return i18n("Uknown")+QLatin1String(" (")+command+QLatin1Char(')');
        } else {
            cmdEnd+=2;
            QString rv=data.mid(cmdEnd, data.length()-(data.endsWith('\n') ? cmdEnd+1 : cmdEnd));
            if (data.contains("{listplaylists}")) {
                // NOTE: NOT translated, as refers to config item
                return QLatin1String("playlist_directory - ")+rv;
            }
            return rv;
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
        return i18nc("name (host:port)", "\"%1\" (%2:%3)", getName(), hostname, QString::number(port)); // Use QString::number to prevent KDE's i18n converting 6600 to 6,600!
    }
}

void MPDConnectionDetails::setDirReadable()
{
    dirReadable=Utils::isDirReadable(dir);
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
    , mopidy(false)
    , isUpdatingDb(false)
{
    qRegisterMetaType<Song>("Song");
    qRegisterMetaType<Output>("Output");
    qRegisterMetaType<Playlist>("Playlist");
    qRegisterMetaType<QList<Song> >("QList<Song>");
    qRegisterMetaType<QList<Output> >("QList<Output>");
    qRegisterMetaType<QList<Playlist> >("QList<Playlist>");
    qRegisterMetaType<QList<quint32> >("QList<quint32>");
    qRegisterMetaType<QList<qint32> >("QList<qint32>");
    qRegisterMetaType<QList<quint8> >("QList<quint8>");
    qRegisterMetaType<QSet<qint32> >("QSet<qint32>");
    qRegisterMetaType<QSet<QString> >("QSet<QString>");
    qRegisterMetaType<QAbstractSocket::SocketState >("QAbstractSocket::SocketState");
    qRegisterMetaType<MPDStatsValues>("MPDStatsValues");
    qRegisterMetaType<MPDStatusValues>("MPDStatusValues");
    qRegisterMetaType<MPDConnectionDetails>("MPDConnectionDetails");
    #ifdef QT_QTDBUS_FOUND
    connect(PowerManagement::self(), SIGNAL(resuming()), this, SLOT(reconnect()));
    #endif
    maxFilesPerAddCommand=Settings::self()->mpdListSize();
    alwaysUseLsInfo=Settings::self()->alwaysUseLsInfo();
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

            if (recvdata.startsWith(constOkMpdValue)) {
                DBUG << (void *)(&socket) << "Received identification string";
            }

            lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=0;
            playQueueIds.clear();
            emit cantataStreams(QList<Song>(), false);
            int min, maj, patch;
            if (3==sscanf(&(recvdata.constData()[7]), "%3d.%3d.%3d", &maj, &min, &patch)) {
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
        emit socketAddress(sock.address());
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
    emit socketAddress(QString());
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
        getStats();
        getUrlHandlers();
        getTagTypes();
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
        mopidy=false;
        if (isUser) {
            MPDUser::self()->start();
        }
        if (isUpdatingDb) {
            isUpdatingDb=false;
            emit updatingDatabase(); // Stop any spinners...
        }
        switch (connectToMPD()) {
        case Success:
            getStatus();
            getStats();
            getUrlHandlers();
            getTagTypes();
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

//void MPDConnection::disconnectMpd()
//{
//    if (State_Connected==state) {
//        disconnectFromMPD();
//        emit stateChanged(false);
//    }
//}

MPDConnection::Response MPDConnection::sendCommand(const QByteArray &command, bool emitErrors, bool retry)
{
    DBUG << (void *)(&sock) << "sendCommand:" << log(command) << emitErrors << retry;

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
        DBUG << "Failed to write";
        // If we fail to write, dont wait for bytes to be written!!
        response=Response(false);
        sock.close();
    } else {
        int timeout=socketTimeout(command.length());
        DBUG << "Timeout (ms):" << timeout;
        sock.waitForBytesWritten(timeout);
        DBUG << "Socket state after write:" << (int)sock.state();
        response=readReply(sock);
    }

    if (!response.ok) {
        DBUG << log(command) << "failed";
        if (response.data.isEmpty() && retry && QAbstractSocket::ConnectedState!=sock.state()) {
            // Try one more time...
            // This scenario, where socket seems to be closed during/after 'write' seems to occur more often
            // when dynamizer is running. However, simply reconnecting seems to resolve the issue.
            return sendCommand(command, emitErrors, false);
        }
        if (emitErrors) {
            bool emitError=true;
            // Mopidy returns "incorrect arguments" for commands it does not support. The docs state that crossfade and replaygain mode
            // setting commands are not supported. So, if we get this error then just ignore it.
            if ((command.startsWith("crossfade ") || command.startsWith("replay_gain_mode ")) &&
                "incorrect arguments"==response.getError(command)) {
                emitError=false;
            }
            if (emitError) {
                if ((command.startsWith("add ") || command.startsWith("command_list_begin\nadd ")) && -1!=command.indexOf("\"file:///")) {
                    if (details.isLocal() && -1!=response.data.indexOf("Permission denied")) {
                        emit error(i18n("Failed to load. Please check user \"mpd\" has read permission."));
                    } else if (!details.isLocal() && -1!=response.data.indexOf("Access denied")) {
                        emit error(i18n("Failed to load. MPD can only play local files if connected via a local socket."));
                    } else if (!response.getError(command).isEmpty()) {
                        emit error(i18n("MPD reported the following error: %1", response.getError(command)));
                    } else {
                        disconnectFromMPD();
                        emit stateChanged(false);
                        emit error(i18n("Failed to send command. Disconnected from %1", details.description()), true);
                    }
                } else if (!response.getError(command).isEmpty()) {
                    emit error(i18n("MPD reported the following error: %1", response.getError(command)));
                } /*else if ("listallinfo"==command && ver>=MPD_MAKE_VERSION(0,18,0)) {
                    disconnectFromMPD();
                    emit stateChanged(false);
                    emit error(i18n("Failed to load library. Please increase \"max_output_buffer_size\" in MPD's config file."));
                } */ else {
                    disconnectFromMPD();
                    emit stateChanged(false);
                    emit error(i18n("Failed to send command. Disconnected from %1", details.description()), true);
                }
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
    return file.endsWith(QLatin1String(".asx"), Qt::CaseInsensitive) || file.endsWith(QLatin1String(".cue"), Qt::CaseInsensitive) ||
           file.endsWith(QLatin1String(".m3u"), Qt::CaseInsensitive) || file.endsWith(QLatin1String(".pls"), Qt::CaseInsensitive) ||
           file.endsWith(QLatin1String(".xspf"), Qt::CaseInsensitive);
}

void MPDConnection::add(const QStringList &files, bool replace, quint8 priority)
{
    add(files, 0, 0, replace ? AddReplaceAndPlay : AddToEnd, priority);
}

void MPDConnection::add(const QStringList &files, quint32 pos, quint32 size, int action, quint8 priority)
{
    QList<quint8> prioList;
    if (priority>0) {
        prioList << priority;
    }
    add(files, pos, size, action, prioList);
}

void MPDConnection::add(const QStringList &origList, quint32 pos, quint32 size, int action, const QList<quint8> &priority)
{
    toggleStopAfterCurrent(false);
    if (AddToEnd!=action) {
        clear();
        getStatus();
    }

    QList<QStringList> fileLists;
    if (priority.count()<=1 && origList.count()>maxFilesPerAddCommand) {
        int numChunks=(origList.count()/maxFilesPerAddCommand)+(origList.count()%maxFilesPerAddCommand ? 1 : 0);
        for (int i=0; i<numChunks; ++i) {
            fileLists.append(origList.mid(i*maxFilesPerAddCommand, maxFilesPerAddCommand));
        }
    } else {
        fileLists.append(origList);
    }

    int curSize = size;
    int curPos = pos;
    //    bool addedFile=false;
    bool havePlaylist=false;
    bool usePrio=!priority.isEmpty() && canUsePriority() && (1==priority.count() || priority.count()==origList.count());
    quint8 singlePrio=usePrio && 1==priority.count() ? priority.at(0) : 0;
    QStringList cStreamFiles;
    bool sentOk=false;

    if (usePrio && AddToEnd==action && 0==curPos) {
        curPos=playQueueIds.size();
    }

    foreach (const QStringList &files, fileLists) {
        QByteArray send = "command_list_begin\n";

        for (int i = 0; i < files.size(); i++) {
            QString fileName=files.at(i);
            if (fileName.startsWith(QLatin1String("http://")) && fileName.contains(QLatin1String("cantata=song"))) {
                cStreamFiles.append(fileName);
            }
            if (CueFile::isCue(fileName)) {
                send += "load "+CueFile::getLoadLine(fileName)+"\n";
            } else {
                if (isPlaylist(fileName)) {
                    send+="load ";
                    havePlaylist=true;
                } else {
                    //                addedFile=true;
                    send += "add ";
                }
                send += encodeName(fileName)+"\n";
            }
            if (!havePlaylist) {
                if (0!=size) {
                    send += "move "+quote(curSize)+" "+quote(curPos)+"\n";
                }
                if (usePrio && !havePlaylist) {
                    send += "prio "+quote(singlePrio || i>=priority.count() ? singlePrio : priority.at(i))+" "+quote(curPos)+" "+quote(curPos)+"\n";
                }
            }
            curSize++;
            curPos++;
        }

        send += "command_list_end";
        sentOk=sendCommand(send).ok;
        if (!sentOk) {
            break;
        }
    }

    if (sentOk) {
        if (!cStreamFiles.isEmpty()) {
            emit cantataStreams(cStreamFiles);
        }

        if (AddReplaceAndPlay==action /*&& addedFile */&& !origList.isEmpty()) {
            // Dont emit error if fail plays, might be that playlist was not loaded...
            sendCommand("play "+quote(0), false);
        }
        emit added(origList);
    }
}

void MPDConnection::populate(const QStringList &files, const QList<quint8> &priority)
{
    add(files, 0, 0, AddAndReplace, priority);
}

void MPDConnection::addAndPlay(const QString &file)
{
    toggleStopAfterCurrent(false);
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        QByteArray send = "command_list_begin\n";
        send+="add "+encodeName(file)+"\n";
        send+="play "+quote(sv.playlistLength)+"\n";
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
        emit cantataStreams(QList<Song>(), false);
    }
}

void MPDConnection::removeSongs(const QList<qint32> &items)
{
    toggleStopAfterCurrent(false);
    QByteArray send = "command_list_begin\n";
    foreach (qint32 i, items) {
        send += "deleteid "+quote(i)+"\n";
    }

    send += "command_list_end";
    sendCommand(send);
}

void MPDConnection::move(quint32 from, quint32 to)
{
    toggleStopAfterCurrent(false);
    sendCommand("move "+quote(from)+' '+quote(to));
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
        send += quote(moveItems.at(i));
        send += " ";
        send += quote(size - 1);
        send += "\n";
    }
    //now move all of them to the destination position
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        send += "move ";
        send += quote(size - 1 - i);
        send += " ";
        send += quote(pos - posOffset);
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
    sendCommand("shuffle "+quote(from)+':'+quote(to+1));
}

void MPDConnection::currentSong()
{
    Response response=sendCommand("currentsong");
    if (response.ok) {
        emit currentSongUpdated(MPDParseUtils::parseSong(response.data, MPDParseUtils::Loc_PlayQueue));
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
    DBUG << lastUpdatePlayQueueVersion << playQueueIds.size();
    if (0==lastUpdatePlayQueueVersion || 0==playQueueIds.size()) {
        playListInfo();
        return;
    }

    QByteArray data = "plchangesposid "+quote(lastUpdatePlayQueueVersion);
    Response status=sendCommand("status"); // We need an updated status so as to detect deletes at end of list...
    Response response=sendCommand(data, false);
    if (response.ok && status.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
        lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=sv.playlist;
        emit statusUpdated(sv);
        QList<MPDParseUtils::IdPos> changes=MPDParseUtils::parseChanges(response.data);
        if (!changes.isEmpty()) {
            bool first=true;
            quint32 firstPos=0;
            QList<Song> songs;
            QList<Song> newCantataStreams;
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
                    data += quote(idp.pos);
                    response=sendCommand(data);
                    if (!response.ok) {
                        playListInfo();
                        return;
                    }
                    Song s=MPDParseUtils::parseSong(response.data, MPDParseUtils::Loc_PlayQueue);
                    s.id=idp.id;
//                     s.pos=idp.pos;
                    songs.append(s);
                    if (s.isCdda()) {
                        newCantataStreams.append(s);
                    } else if (s.isStream()) {
                        if (s.isCantataStream()) {
                            newCantataStreams.append(s);
                        } else {
                            strmIds.insert(s.id);
                        }
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
            if (!newCantataStreams.isEmpty()) {
                emit cantataStreams(newCantataStreams, true);
            }
            QSet<qint32> removed=prevIds-playQueueIds.toSet();
            if (!removed.isEmpty()) {
                emit removedIds(removed);
            }
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
        QList<Song> songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_PlayQueue);
        playQueueIds.clear();
        streamIds.clear();

        QList<Song> cStreams;
        foreach (const Song &s, songs) {
            playQueueIds.append(s.id);
            if (s.isCdda()) {
                cStreams.append(s);
            } else if (s.isStream()) {
                if (s.isCantataStream()) {
                    cStreams.append(s);
                } else {
                    streamIds.insert(s.id);
                }
            }
        }
        emit cantataStreams(cStreams, false);
        emit playlistUpdated(songs);
    }
}

/*
 * Playback commands
 */
void MPDConnection::setCrossFade(int secs)
{
    sendCommand("crossfade "+quote(secs));
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
    return MPDConnection::quote(b ? 1 : 0);
}

void MPDConnection::setPause(bool toggle)
{
    toggleStopAfterCurrent(false);
    sendCommand("pause "+value(toggle));
}

void MPDConnection::play()
{
    toggleStopAfterCurrent(false);
    sendCommand("play");
}

//void MPDConnection::startPlayingSong(quint32 song)
//{
//    toggleStopAfterCurrent(false);
//    sendCommand("play "+quote(song));
//}

void MPDConnection::startPlayingSongId(qint32 songId)
{
    toggleStopAfterCurrent(false);
    sendCommand("playid "+quote(songId));
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
    sendCommand("seek "+quote(song)+' '+quote(time));
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
    if (sendCommand("seekid "+quote(songId)+' '+quote(time)).ok) {
        if (stopAfterCurrent && songId==currentSongId && songPos>time) {
            songPos=time;
        }
    }
}

void MPDConnection::setVolume(int vol)
{
    if (vol>=0) {
        unmuteVol=-1;
        sendCommand("setvol "+quote(vol), false);
    }
}

void MPDConnection::toggleMute()
{
    if (unmuteVol>0) {
        sendCommand("setvol "+quote(unmuteVol), false);
        unmuteVol=-1;
    } else {
        Response status=sendCommand("status");
        if (status.ok) {
            MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
            if (sv.volume>0) {
                unmuteVol=sv.volume;
                sendCommand("setvol "+quote(0), false);
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

void MPDConnection::getStats()
{
    Response response=sendCommand("stats");
    if (response.ok) {
        MPDStatsValues stats=MPDParseUtils::parseStats(response.data);
        dbUpdate=stats.dbUpdate;
        mopidy=0==stats.artists && 0==stats.albums && 0==stats.songs &&
               0==stats.uptime && 0==stats.playtime && 0==stats.dbPlaytime && 0==dbUpdate.toTime_t();
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
        if (!isUpdatingDb && -1!=sv.updatingDb) {
            isUpdatingDb=true;
            emit updatingDatabase();
        } else if (isUpdatingDb && -1==sv.updatingDb) {
            isUpdatingDb=false;
            emit updatedDatabase();
        }
        emit statusUpdated(sv);
    }
}

void MPDConnection::getUrlHandlers()
{
    Response response=sendCommand("urlhandlers");
    if (response.ok) {
        handlers=MPDParseUtils::parseList(response.data, QByteArray("handler: ")).toSet();
    }
}

void MPDConnection::getTagTypes()
{
    Response response=sendCommand("tagtypes");
    if (response.ok) {
        tagTypes=MPDParseUtils::parseList(response.data, QByteArray("tagtype: ")).toSet();
    }
}


/*
 * Data is written during idle.
 * Retrieve it and parse it
 */
void MPDConnection::idleDataReady()
{
    DBUG << "idleDataReady";
    if (0==idleSocket.bytesAvailable()) {
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

    Response response(data.endsWith(constOkNlValue), data);
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
        if (line.startsWith(constIdleChangedKey)) {
            QByteArray value=line.mid(constIdleChangedKey.length());
            if (constIdleDbValue==value) {
                getStats();
                getStatus();
                playListInfo();
                playListUpdated=true;
            } else if (constIdleUpdateValue==value) {
                getStatus();
            } else if (constIdleStoredPlaylistValue==value) {
                emit storedPlayListUpdated();
            } else if (constIdlePlaylistValue==value) {
                if (!playListUpdated) {
                    playListChanges();
                }
            } else if (!statusUpdated && (constIdlePlayerValue==value || constIdleMixerValue==value || constIdleOptionsValue==value)) {
                getStatus();
                getReplayGain();
                statusUpdated=true;
            } else if (constIdleOutputValue==value) {
                outputs();
            }
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
    sendCommand((enable ? "enableoutput " : "disableoutput ")+quote(id));
}

/*
 * Admin commands
 */
void MPDConnection::update()
{
    if (sendCommand("update").ok) {
        if (isMopdidy()) {
            // Mopidy does not support MPD's update command. So, when user presses update DB, what we
            // do instead is clear library/dir caches, then when response to getStats is received,
            // library/dir should get refreshed...
            getStats();
        }
    }
}

/*
 * Database commands
 */
void MPDConnection::loadLibrary()
{
    emit updatingLibrary();
    Response response=alwaysUseLsInfo ? Response(false) : sendCommand("listallinfo", false);
    MusicLibraryItemRoot *root=0;
    if (response.ok) {
        root = new MusicLibraryItemRoot;
        MPDParseUtils::parseLibraryItems(response.data, details.dir, ver, mopidy, root);
    } else { // MPD >=0.18 can fail listallinfo for large DBs, so get info dir by dir...
        root = new MusicLibraryItemRoot;
        if (!listDirInfo("/", root)) {
            delete root;
            root=0;
        }
    }

    if (root) {
        root->applyGrouping();
        emit musicLibraryUpdated(root, dbUpdate);
    }
    emit updatedLibrary();
}

void MPDConnection::loadFolders()
{
    emit updatingFileList();
    Response response=sendCommand("listall");
    if (response.ok) {
        emit dirViewUpdated(MPDParseUtils::parseDirViewItems(response.data, mopidy), dbUpdate);
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
        emit playlistInfoRetrieved(name, MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Playlists));
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
                emit error(i18n("You cannot add parts of a cue sheet to a playlist!")+QLatin1String(" (")+song+QLatin1Char(')'));
                return;
            } else if (isPlaylist(song)) {
                emit error(i18n("You cannot add a playlist to another playlist!")+QLatin1String(" (")+song+QLatin1Char(')'));
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
        data += quote(idx);
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
            send += "prioid "+quote(priority)+" "+quote(id)+"\n";
        }

        send += "command_list_end";
        if (sendCommand(send).ok) {
            emit prioritySet(ids, priority);
        }
    }
}

void MPDConnection::search(const QString &field, const QString &value, int id)
{
    QList<Song> songs;
    Response response=sendCommand("search "+field.toLatin1()+" "+encodeName(value));
    if (response.ok) {
        songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Library);
        qSort(songs);
    }
    emit searchResponse(id, songs);
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
        send += quote(moveItems.at(i));
        send += " ";
        send += quote(size - 1);
        send += "\n";
    }
    //now move all of them to the destination position
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        send += cmd;
        send += quote(size - 1 - i);
        send += " ";
        send += quote(pos - posOffset);
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

bool MPDConnection::listDirInfo(const QString &dir, MusicLibraryItemRoot *root)
{
    bool topLevel="/"==dir;
    Response response=sendCommand(topLevel ? "lsinfo" : ("lsinfo "+encodeName(dir)));
    if (response.ok) {
        QSet<QString> childDirs;
        MPDParseUtils::parseLibraryItems(response.data, details.dir, ver, mopidy, root, !topLevel, &childDirs);
        foreach (const QString &child, childDirs) {
            if (!listDirInfo(child, root)) {
                return false;
            }
        }
        return true;
    } else {
        return false;
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
