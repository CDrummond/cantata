/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "models/streamsmodel.h"
#ifdef ENABLE_SIMPLE_MPD_SUPPORT
#include "mpduser.h"
#endif
#include "gui/settings.h"
#include "support/globalstatic.h"
#include "support/configuration.h"
#include <QStringList>
#include <QTimer>
#include <QDir>
#include <QHostInfo>
#include <QDateTime>
#include <QPropertyAnimation>
#include <QCoreApplication>
#include <QUdpSocket>
#include <complex>
#include "support/thread.h"
#include "cuefile.h"
#if defined Q_OS_LINUX && defined QT_QTDBUS_FOUND
#include "dbus/powermanagement.h"
#elif defined Q_OS_MAC && defined IOKIT_FOUND
#include "mac/powermanagement.h"
#endif
#include <algorithm>
#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "MPDConnection" << QThread::currentThreadId()
void MPDConnection::enableDebug()
{
    debugEnabled=true;
}

// Uncomment the following to report error strings in MPDStatus to the UI
// ...disabled, as stickers (for ratings) can cause lots of errors to be reported - and these all need clearing, etc.
// #define REPORT_MPD_ERRORS
static const int constSocketCommsTimeout=2000;
static const int constMaxReadAttempts=4;
static const int constMaxFilesPerAddCommand=2000;
static const int constConnTimer=5000;

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
static const QByteArray constIdleStickerValue("sticker");
static const QByteArray constIdleSubscriptionValue("subscription");
static const QByteArray constIdleMessageValue("message");
static const QByteArray constDynamicIn("cantata-dynamic-in");
static const QByteArray constDynamicOut("cantata-dynamic-out");
static const QByteArray constRatingSticker("rating");

static inline int socketTimeout(int dataSize)
{
    static const int constDataBlock=256;
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

const QString MPDConnection::constModifiedSince=QLatin1String("modified-since");
const int MPDConnection::constMaxPqChanges=1000;
const QString MPDConnection::constStreamsPlayListName=QLatin1String("[Radio Streams]");
const QString MPDConnection::constPlaylistPrefix=QLatin1String("playlist:");
const QString MPDConnection::constDirPrefix=QLatin1String("dir:");

QByteArray MPDConnection::quote(int val)
{
    return '\"'+QByteArray::number(val)+'\"';
}

QByteArray MPDConnection::encodeName(const QString &name)
{
    return '\"'+name.toUtf8().replace("\\", "\\\\").replace("\"", "\\\"")+'\"';
}

static QByteArray readFromSocket(MpdSocket &socket, int timeout=constSocketCommsTimeout)
{
    QByteArray data;
    int attempt=0;
    while (QAbstractSocket::ConnectedState==socket.state()) {
        while (0==socket.bytesAvailable() && QAbstractSocket::ConnectedState==socket.state()) {
            DBUG << (void *)(&socket) << "Waiting for read data, attempt" << attempt;
            if (socket.waitForReadyRead(timeout)) {
                break;
            }
            DBUG << (void *)(&socket) << "Wait for read failed - " << socket.errorString();
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

static MPDConnection::Response readReply(MpdSocket &socket, int timeout=constSocketCommsTimeout)
{
    QByteArray data = readFromSocket(socket, timeout);
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
            return tr("Unknown")+QLatin1String(" (")+command+QLatin1Char(')');
        } else {
            cmdEnd+=2;
            QString rv=data.mid(cmdEnd, data.length()-(data.endsWith('\n') ? cmdEnd+1 : cmdEnd));
            if (data.contains("{listplaylists}")) {
                // NOTE: NOT translated, as refers to config item
                return QLatin1String("playlist_directory - ")+rv;
            }

            // If we are reporting a stream error, remove any stream name added by Cantata...
            int start=rv.indexOf(QLatin1String("http://"));
            if (start>0) {
                int pos=rv.indexOf(QChar('#'), start+6);
                if (-1!=pos) {
                    rv=rv.left(pos);
                }
            }

            return rv;
        }
    }
    return data;
}

MPDConnectionDetails::MPDConnectionDetails()
    : port(6600)
    , dirReadable(false)
    , applyReplayGain(true)
    , allowLocalStreaming(true)
    , autoUpdate(false)
{
}

QString MPDConnectionDetails::getName() const
{
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    return name.isEmpty() ? QObject::tr("Default") : (name==MPDUser::constName ? MPDUser::translatedName() : name);
    #else
    return name.isEmpty() ? QObject::tr("Default") : name;
    #endif
}

QString MPDConnectionDetails::description() const
{
    if (hostname.isEmpty()) {
        return getName();
    } else if (hostname.startsWith('/') || hostname.startsWith('~')) {
        return getName();
    } else {
        return QObject::tr("\"%1\" (%2:%3)", "name (host:port)").arg(getName()).arg(hostname).arg(QString::number(port));
    }
}

MPDConnectionDetails & MPDConnectionDetails::operator=(const MPDConnectionDetails &o)
{
    name=o.name;
    hostname=o.hostname;
    port=o.port;
    password=o.password;
    dir=o.dir;
    dirReadable=o.dirReadable;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamUrl=o.streamUrl;
    #endif
    replayGain=o.replayGain;
    applyReplayGain=o.applyReplayGain;
    allowLocalStreaming=o.allowLocalStreaming;
    autoUpdate=o.autoUpdate;
    return *this;
}

void MPDConnectionDetails::setDirReadable()
{
    dirReadable=Utils::isDirReadable(dir);
}

MPDConnection::MPDConnection()
    : isInitialConnect(true)
    , thread(nullptr)
    , ver(0)
    , canUseStickers(false)
    , sock(this)
    , idleSocket(this)
    , lastStatusPlayQueueVersion(0)
    , lastUpdatePlayQueueVersion(0)
    , state(State_Blank)
    , isListingMusic(false)
    , reconnectTimer(nullptr)
    , reconnectStart(0)
    , stopAfterCurrent(false)
    , currentSongId(-1)
    , songPos(0)
    , unmuteVol(-1)
    , isUpdatingDb(false)
    , volumeFade(nullptr)
    , fadeDuration(0)
    , restoreVolume(-1)
{
    qRegisterMetaType<time_t>("time_t");
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
    qRegisterMetaType<QMap<qint32, quint8> >("QMap<qint32, quint8>");
    qRegisterMetaType<Stream>("Stream");
    qRegisterMetaType<QList<Stream> >("QList<Stream>");
    #if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
    connect(PowerManagement::self(), SIGNAL(resuming()), this, SLOT(reconnect()));
    #endif
    MPDParseUtils::setSingleTracksFolders(Configuration().get("singleTracksFolders", QStringList()).toSet());
}

MPDConnection::~MPDConnection()
{
    if (State_Connected==state && fadingVolume()) {
        sendCommand("stop");
        stopVolumeFade();
    }
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
        connTimer=thread->createTimer(this);
        connTimer->setSingleShot(false);
        moveToThread(thread);
        connect(thread, SIGNAL(finished()), connTimer, SLOT(stop()));
        connect(connTimer, SIGNAL(timeout()), SLOT(getStatus()));
        thread->start();
    }
}

void MPDConnection::stop()
{
    stopPlaying();
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    if (details.name==MPDUser::constName && Settings::self()->stopOnExit()) {
        MPDUser::self()->stop();
    }
    #endif

    if (thread) {
        thread->deleteTimer(connTimer);
        connTimer=nullptr;
        thread->stop();
        thread=nullptr;
    }
}

bool MPDConnection::localFilePlaybackSupported() const
{
    return details.isLocal() ||
           (ver>=CANTATA_MAKE_VERSION(0, 19, 0) && /*handlers.contains(QLatin1String("file")) &&*/
           (QLatin1String("127.0.0.1")==details.hostname || QLatin1String("localhost")==details.hostname));
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
                socket.write("password "+details.password.toUtf8()+'\n');
                socket.waitForBytesWritten(constSocketCommsTimeout);
                if (!readReply(socket).ok) {
                    DBUG << (void *)(&socket) << "password rejected";
                    socket.close();
                    return IncorrectPassword;
                }
            }

            if (enableIdle) {
                dynamicId.clear();
                setupRemoteDynamic();
                connect(&socket, SIGNAL(readyRead()), this, SLOT(idleDataReady()), Qt::QueuedConnection);
                connect(&socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)), Qt::QueuedConnection);
                DBUG << (void *)(&socket) << "Enabling idle";
                socket.write("idle\n");
                socket.waitForBytesWritten();
            }
            return Success;
        } else {
            DBUG << (void *)(&socket) << "Couldn't connect - " << socket.errorString() << socket.error();
            return convertSocketCode(socket);
        }
    }

//     DBUG << "Already connected" << (enableIdle ? "(idle)" : "(std)");
    return Success;
}

MPDConnection::ConnectionReturn MPDConnection::convertSocketCode(MpdSocket &socket)
{
    switch (socket.error()) {
    case QAbstractSocket::ProxyAuthenticationRequiredError:
    case QAbstractSocket::ProxyConnectionRefusedError:
    case QAbstractSocket::ProxyConnectionClosedError:
    case QAbstractSocket::ProxyConnectionTimeoutError:
    case QAbstractSocket::ProxyNotFoundError:
    case QAbstractSocket::ProxyProtocolError:
        return MPDConnection::ProxyError;
    default:
        if (QNetworkProxy::DefaultProxy!=socket.proxyType() && QNetworkProxy::NoProxy!=socket.proxyType()) {
            return MPDConnection::ProxyError;
        }
        if (socket.errorString().contains(QLatin1String("proxy"), Qt::CaseInsensitive)) {
            return MPDConnection::ProxyError;
        }
        return MPDConnection::Failed;
    }
}

QString MPDConnection::errorString(ConnectionReturn status) const
{
    switch (status) {
    default:
    case Success:           return QString();
    case Failed:            return tr("Connection to %1 failed").arg(details.description());
    case ProxyError:        return tr("Connection to %1 failed - please check your proxy settings").arg(details.description());
    case IncorrectPassword: return tr("Connection to %1 failed - incorrect password").arg(details.description());
    }
}

MPDConnection::ConnectionReturn MPDConnection::connectToMPD()
{
    connTimer->stop();
    if (State_Connected==state && (QAbstractSocket::ConnectedState!=sock.state() || QAbstractSocket::ConnectedState!=idleSocket.state())) {
        DBUG << "Something has gone wrong with sockets, so disconnect";
        disconnectFromMPD();
    }
    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    int maxConnAttempts=MPDUser::constName==details.name ? 2 : 1;
    #else
    int maxConnAttempts=1;
    #endif
    ConnectionReturn status=Failed;
    for (int connAttempt=0; connAttempt<maxConnAttempts; ++connAttempt) {
        if (Success==(status=connectToMPD(sock)) && Success==(status=connectToMPD(idleSocket, true))) {
            state=State_Connected;
            emit socketAddress(sock.address());
        } else {
            disconnectFromMPD();
            state=State_Disconnected;
            #ifdef ENABLE_SIMPLE_MPD_SUPPORT
            if (0==connAttempt && MPDUser::constName==details.name) {
                DBUG << "Restart personal mpd";
                MPDUser::self()->stop();
                MPDUser::self()->start();
            }
            #endif
        }
    }
    connTimer->start(constConnTimer);
    return status;
}

void MPDConnection::disconnectFromMPD()
{
    DBUG << "disconnectFromMPD";
    connTimer->stop();
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
    playQueueIds.clear();
    streamIds.clear();
    lastStatusPlayQueueVersion=0;
    lastUpdatePlayQueueVersion=0;
    currentSongId=0;
    songPos=0;
    serverInfo.reset();
    isUpdatingDb=false;
    emit socketAddress(QString());
}

// This function is mainly intended to be called after the computer (laptop) has been 'resumed'
void MPDConnection::reconnect()
{
    if (reconnectTimer && reconnectTimer->isActive()) {
        return;
    }
    if (0==reconnectStart && isConnected()) {
        disconnectFromMPD();
    }

    if (isConnected()) { // Perhaps the user pressed a button which caused the reconnect???
        reconnectStart=0;
        return;
    }
    time_t now=time(nullptr);
    ConnectionReturn status=connectToMPD();
    switch (status) {
    case Success:        
        // Issue #1041 - MPD does not seem to persist user/client made replaygain changes, so use the values read from Cantata's config.
        if (replaygainSupported() && details.applyReplayGain && !details.replayGain.isEmpty()) {
            sendCommand("replay_gain_mode "+details.replayGain.toLatin1());
        }
        getStatus();
        getStats();
        getUrlHandlers();
        getTagTypes();
        getStickerSupport();
        playListInfo();
        outputs();
        reconnectStart=0;
        determineIfaceIp();
        emit stateChanged(true);
        break;
    case Failed:
        if (0==reconnectStart || std::abs(now-reconnectStart)<15) {
            if (0==reconnectStart) {
                reconnectStart=now;
            }
            if (!reconnectTimer) {
                reconnectTimer=new QTimer(this);
                reconnectTimer->setSingleShot(true);
                connect(reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnect()), Qt::QueuedConnection);
            }
            if (std::abs(now-reconnectStart)>1) {
                emit info(tr("Connecting to %1").arg(details.description()));
            }
            reconnectTimer->start(500);
        } else {
            emit stateChanged(false);
            emit error(errorString(Failed), true);
            reconnectStart=0;
        }
        break;
    default:
        emit stateChanged(false);
        emit error(errorString(status), true);
        reconnectStart=0;
        break;
    }
}

void MPDConnection::setDetails(const MPDConnectionDetails &d)
{
    // Can't change connection whilst listing music collection...
    if (isListingMusic) {
        emit connectionNotChanged(details.name);
        return;
    }

    #ifdef ENABLE_SIMPLE_MPD_SUPPORT
    bool isUser=d.name==MPDUser::constName;
    const MPDConnectionDetails &det=isUser ? MPDUser::self()->details(true) : d;
    #else
    const MPDConnectionDetails &det=d;
    #endif
    bool changedDir=det.dir!=details.dir;
    bool diffName=det.name!=details.name;
    bool diffDetails=det!=details;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool diffStreamUrl=det.streamUrl!=details.streamUrl;
    #endif

    details=det;
    // Issue #1041 - If this is a user MPD, then the call to MPDUser::self()->details() will clear the replayGain setting
    // We can safely use that of the passed in details.
    details.replayGain=d.replayGain;

    if (diffDetails || State_Connected!=state) {
        emit connectionChanged(details);
        DBUG << "setDetails" << details.hostname << details.port << (details.password.isEmpty() ? false : true);
        if (State_Connected==state && fadingVolume()) {
            sendCommand("stop");
            stopVolumeFade();
        }
        disconnectFromMPD();
        DBUG << "call connectToMPD";
        unmuteVol=-1;
        toggleStopAfterCurrent(false);
        #ifdef ENABLE_SIMPLE_MPD_SUPPORT
        if (isUser) {
            MPDUser::self()->start();
        }
        #endif
        if (isUpdatingDb) {
            isUpdatingDb=false;
            emit updatedDatabase(); // Stop any spinners...
        }
        ConnectionReturn status=connectToMPD();
        switch (status) {
        case Success:
            // Issue #1041 - MPD does not seem to persist user/client made replaygain changes, so use the values read from Cantata's config.
            if (replaygainSupported() && details.applyReplayGain && !details.replayGain.isEmpty()) {
                sendCommand("replay_gain_mode "+details.replayGain.toLatin1());
            }
            serverInfo.detect();
            getStatus();
            getStats();
            getUrlHandlers();
            getTagTypes();
            getStickerSupport();
            playListInfo();
            outputs();
            determineIfaceIp();
            emit stateChanged(true);
            break;
        default:
            emit stateChanged(false);
            emit error(errorString(status), true);
            if (isInitialConnect) {
                reconnect();
            }
        }
    } else if (diffName) {
         emit stateChanged(true);
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    if (diffStreamUrl) {
        emit streamUrl(details.streamUrl);
    }
    #endif
    if (changedDir) {
        emit dirChanged();
    }
    isInitialConnect = false;
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
    connTimer->stop();
    static bool reconnected=false; // If we reconnect, and send playlistinfo - dont want that call causing reconnects, and recursion!
    DBUG << (void *)(&sock) << "sendCommand:" << log(command) << emitErrors << retry;

    if (!isConnected()) {
        if ("stop"!=command) {
            emit error(tr("Failed to send command to %1 - not connected").arg(details.description()), true);
        }
        return Response(false);
    }

    if (QAbstractSocket::ConnectedState!=sock.state() && !reconnected) {
        DBUG << (void *)(&sock) << "Socket (state:" << sock.state() << ") need to reconnect";
        sock.close();
        ConnectionReturn status=connectToMPD();
        if (Success!=status) {
            disconnectFromMPD();
            emit stateChanged(false);
            emit error(errorString(status), true);
            return Response(false);
        } else {
            // Refresh playqueue...
            reconnected=true;
            playListInfo();
            getStatus();
            reconnected=false;
        }
    }

    Response response;
    if (-1==sock.write(command+'\n')) {
        DBUG << "Failed to write";
        // If we fail to write, dont wait for bytes to be written!!
        response=Response(false);
        sock.close();
    } else {
        int timeout=socketTimeout(command.length());
        DBUG << "Timeout (ms):" << timeout;
        sock.waitForBytesWritten(timeout);
        DBUG << "Socket state after write:" << (int)sock.state();
        response=readReply(sock, timeout);
    }

    if (!response.ok) {
        DBUG << log(command) << "failed";
        if (response.data.isEmpty() && retry && QAbstractSocket::ConnectedState!=sock.state() && !reconnected) {
            // Try one more time...
            // This scenario, where socket seems to be closed during/after 'write' seems to occur more often
            // when dynamizer is running. However, simply reconnecting seems to resolve the issue.
            return sendCommand(command, emitErrors, false);
        }
        clearError();
        if (emitErrors) {
            bool emitError=true;
            // Mopidy returns "incorrect arguments" for commands it does not support. The docs state that crossfade and replaygain mode
            // setting commands are not supported. So, if we get this error then just ignore it.
            if (!isMpd() && (command.startsWith("crossfade ") || command.startsWith("replay_gain_mode "))) {
                emitError=false;
            } else if (isMpd() && command.startsWith("albumart ")) {
                // MPD will report a generic "file not found" error if it can't find album art; this can happen
                // several times in a large playlist so hide this from the GUI (but report it using DBUG here).
                emitError=false;
                const auto start = command.indexOf(' ');
                const auto end = command.lastIndexOf(' ') - start;
                if (start > 0 && (end > 0 && (start + end) < command.length())) {
                    const QString filename = command.mid(start, end);
                    DBUG << "MPD reported no album art for" << filename;
                } else {
                    DBUG << "MPD albumart command was malformed:" << command;
                }
            }
            if (emitError) {
                if ((command.startsWith("add ") || command.startsWith("command_list_begin\nadd ")) && -1!=command.indexOf("\"file:///")) {
                    if (details.isLocal() && -1!=response.data.indexOf("Permission denied")) {
                        emit error(tr("Failed to load. Please check user \"mpd\" has read permission."));
                    } else if (!details.isLocal() && -1!=response.data.indexOf("Access denied")) {
                        emit error(tr("Failed to load. MPD can only play local files if connected via a local socket."));
                    } else if (!response.getError(command).isEmpty()) {
                        emit error(tr("MPD reported the following error: %1").arg(response.getError(command)));
                    } else {
                        disconnectFromMPD();
                        emit stateChanged(false);
                        emit error(tr("Failed to send command. Disconnected from %1").arg(details.description()), true);
                    }
                } else if (!response.getError(command).isEmpty()) {
                    emit error(tr("MPD reported the following error: %1").arg(response.getError(command)));
                } /*else if ("listallinfo"==command && ver>=CANTATA_MAKE_VERSION(0,18,0)) {
                    disconnectFromMPD();
                    emit stateChanged(false);
                    emit error(tr("Failed to load library. Please increase \"max_output_buffer_size\" in MPD's config file."));
                } */ else {
                    disconnectFromMPD();
                    emit stateChanged(false);
                    emit error(tr("Failed to send command. Disconnected from %1").arg(details.description()), true);
                }
            }
        }
    }
    DBUG << (void *)(&sock) << "sendCommand - sent";
    if (QAbstractSocket::ConnectedState==sock.state()) {
        connTimer->start(constConnTimer);
    } else {
        connTimer->stop();
    }
    return response;
}

/*
 * Playlist commands
 */
bool MPDConnection::isPlaylist(const QString &file)
{
    static QSet<QString> extensions = QSet<QString>() << QLatin1String("asx") << QLatin1String("cue")
                                                      << QLatin1String("m3u") << QLatin1String("m3u8")
                                                      << QLatin1String("pls") << QLatin1String("xspf");

    int pos=file.lastIndexOf('.');
    return pos>0 ? extensions.contains(file.mid(pos+1).toLower()) : false;
}

void MPDConnection::add(const QStringList &files, int action, quint8 priority, bool decreasePriority)
{
    add(files, 0, 0, action, priority, decreasePriority);
}

void MPDConnection::add(const QStringList &files, quint32 pos, quint32 size, int action, quint8 priority, bool decreasePriority)
{
    QList<quint8> prioList;
    if (priority>0) {
        prioList << priority;
    }
    add(files, pos, size, action, prioList, decreasePriority);
}

void MPDConnection::add(const QStringList &origList, quint32 pos, quint32 size, int action, const QList<quint8> &priority)
{
    add(origList, pos, size, action, priority, false);
}

void MPDConnection::add(const QStringList &origList, quint32 pos, quint32 size, int action, QList<quint8> priority, bool decreasePriority)
{
    quint32 playPos=0;
    if (0==pos && 0==size && (AddAfterCurrent==action || AppendAndPlay==action || AddAndPlay==action)) {
        Response response=sendCommand("status");
        if (response.ok) {
            MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
            if (AppendAndPlay==action) {
                playPos=sv.playlistLength;
            } else if (AddAndPlay==action) {
                pos=0;
                size=sv.playlistLength;
            } else {
                pos=sv.song+1;
                size=sv.playlistLength;
            }
        }
    }
    toggleStopAfterCurrent(false);
    if (Replace==action || ReplaceAndplay==action) {
        clear();
        getStatus();
    }

    QStringList files;
    for (const QString &file: origList) {
        if (file.startsWith(constDirPrefix)) {
            files+=getAllFiles(file.mid(constDirPrefix.length()));
        } else if (file.startsWith(constPlaylistPrefix)) {
            files+=getPlaylistFiles(file.mid(constPlaylistPrefix.length()));
        } else {
            files.append(file);
        }
    }

    QList<QStringList> fileLists;
    if (priority.count()<=1 && files.count()>constMaxFilesPerAddCommand) {
        int numChunks=(files.count()/constMaxFilesPerAddCommand)+(files.count()%constMaxFilesPerAddCommand ? 1 : 0);
        for (int i=0; i<numChunks; ++i) {
            fileLists.append(files.mid(i*constMaxFilesPerAddCommand, constMaxFilesPerAddCommand));
        }
    } else {
        fileLists.append(files);
    }

    int curSize = size;
    int curPos = pos;
    //    bool addedFile=false;
    bool havePlaylist=false;

    if (1==priority.count() && decreasePriority) {
        quint8 prio=priority.at(0);
        priority.clear();
        for (int i=0; i<files.count(); ++i) {
            priority.append(prio);
            if (prio>1) {
                prio--;
            }
        }
    }

    bool usePrio=!priority.isEmpty() && canUsePriority() && (1==priority.count() || priority.count()==files.count());
    quint8 singlePrio=usePrio && 1==priority.count() ? priority.at(0) : 0;
    QStringList cStreamFiles;
    bool sentOk=false;

    if (usePrio && Append==action && 0==curPos) {
        curPos=playQueueIds.size();
    }

    for (const QStringList &files: fileLists) {
        QByteArray send = "command_list_begin\n";

        for (int i = 0; i < files.size(); i++) {
            QString fileName=files.at(i);
            if (fileName.startsWith(QLatin1String("http://")) && fileName.contains(QLatin1String("cantata=song"))) {
                cStreamFiles.append(fileName);
            }
            if (CueFile::isCue(fileName)) {
                send += "load "+CueFile::getLoadLine(fileName)+'\n';
            } else {
                if (isPlaylist(fileName)) {
                    send+="load ";
                    havePlaylist=true;
                } else {
                    //                addedFile=true;
                    send += "add ";
                }
                send += encodeName(fileName)+'\n';
            }
            if (!havePlaylist) {
                if (0!=size) {
                    send += "move "+quote(curSize)+" "+quote(curPos)+'\n';
                }
                if (usePrio && !havePlaylist) {
                    send += "prio "+quote(singlePrio || i>=priority.count() ? singlePrio : priority.at(i))+" "+quote(curPos)+" "+quote(curPos)+'\n';
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

        if ((ReplaceAndplay==action || AddAndPlay==action) /*&& addedFile */&& !files.isEmpty()) {
            // Dont emit error if play fails, might be that playlist was not loaded...
            playFirstTrack(false);
        }

        if (AppendAndPlay==action) {
            startPlayingSong(playPos);
        }
        emit added(files);
    }
}

void MPDConnection::populate(const QStringList &files, const QList<quint8> &priority)
{
    add(files, 0, 0, Replace, priority);
}

void MPDConnection::addAndPlay(const QString &file)
{
    toggleStopAfterCurrent(false);
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        QByteArray send = "command_list_begin\n";
        if (CueFile::isCue(file)) {
            send += "load "+CueFile::getLoadLine(file)+'\n';
        } else {
            send+="add "+encodeName(file)+'\n';
        }
        send+="play "+quote(sv.playlistLength)+'\n';
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
    for (qint32 i: items) {
        send += "deleteid "+quote(i)+'\n';
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
    std::sort(moveItems.begin(), moveItems.end());

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
        send += '\n';
    }
    //now move all of them to the destination position
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        send += "move ";
        send += quote(size - 1 - i);
        send += " ";
        send += quote(pos - posOffset);
        send += '\n';
    }

    send += "command_list_end";
    sendCommand(send);
    #endif
}

void MPDConnection::setOrder(const QList<quint32> &items)
{
    QByteArray cmd("move ");
    QByteArray send;
    QList<qint32> positions;
    quint32 numChanges=0;
    for (qint32 i=0; i<items.count(); ++i) {
        positions.append(i);
    }

    for (qint32 to=0; to<items.count(); ++to) {
        qint32 from=positions.indexOf(items.at(to));
        if (from!=to) {
            if (send.isEmpty()) {
                send = "command_list_begin\n";
            }
            send += cmd;
            send += quote(from);
            send += " ";
            send += quote(to);
            send += '\n';
            positions.move(from, to);
            numChanges++;
        }
    }

    if (!send.isEmpty()) {
        send += "command_list_end";
        // If there are more than X changes made to the playqueue, then doing partial updates
        // can hang the UI. Therefore, set lastUpdatePlayQueueVersion to 0 to cause playlistInfo
        // to be used to do a complete update.
        if (sendCommand(send).ok && numChanges>constMaxPqChanges) {
            lastUpdatePlayQueueVersion=0;
        }
    }
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
    DBUG << "playListChanges" << lastUpdatePlayQueueVersion << playQueueIds.size();
    if (0==lastUpdatePlayQueueVersion || 0==playQueueIds.size()) {
        playListInfo();
        return;
    }

    QByteArray data = "plchangesposid "+quote(lastUpdatePlayQueueVersion);
    Response status=sendCommand("status"); // We need an updated status so as to detect deletes at end of list...
    Response response=sendCommand(data, false);
    if (response.ok && status.ok && isPlayQueueIdValid()) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
        lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=sv.playlist;
        emitStatusUpdated(sv);
        QList<MPDParseUtils::IdPos> changes=MPDParseUtils::parseChanges(response.data);
        if (!changes.isEmpty()) {
            if (changes.count()>constMaxPqChanges) {
                playListInfo();
                return;
            }
            bool first=true;
            quint32 firstPos=0;
            QList<Song> songs;
            QList<Song> newCantataStreams;
            QList<qint32> ids;
            QSet<qint32> prevIds=playQueueIds.toSet();
            QSet<qint32> strmIds;

            for (const MPDParseUtils::IdPos &idp: changes) {
                if (first) {
                    first=false;
                    firstPos=idp.pos;
                    if (idp.pos!=0) {
                        for (quint32 i=0; i<idp.pos; ++i) {
                            Song s;
                            if (i>=(unsigned int)playQueueIds.count()) { // Just for safety...
                                playListInfo();
                                return;
                            }
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
            emit playlistUpdated(songs, false);
            if (songs.isEmpty()) {
                stopVolumeFade();
            }
            return;
        }
    }

    playListInfo();
}

void MPDConnection::playListInfo()
{
    Response response=sendCommand("playlistinfo");
    QList<Song> songs;
    if (response.ok) {
        lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion;
        songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_PlayQueue);
        playQueueIds.clear();
        streamIds.clear();

        QList<Song> cStreams;
        for (const Song &s: songs) {
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
        if (songs.isEmpty()) {
            stopVolumeFade();
        }
        Response status=sendCommand("status");
        if (status.ok) {
            MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
            lastUpdatePlayQueueVersion=lastStatusPlayQueueVersion=sv.playlist;
            emitStatusUpdated(sv);
        }
    }
    emit playlistUpdated(songs, true);
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
    if (replaygainSupported()) {
        // Issue #1041 - MPD does not seem to persist user/client made replaygain changes, so store in Cantata's config file.
        Settings::self()->saveReplayGain(details.name, v);
        sendCommand("replay_gain_mode "+v.toLatin1());
    }
}

void MPDConnection::getReplayGain()
{
    if (replaygainSupported()) {
        QStringList lines=QString(sendCommand("replay_gain_status").data).split('\n', QString::SkipEmptyParts);

        if (2==lines.count() && "OK"==lines[1] && lines[0].startsWith(QLatin1String("replay_gain_mode: "))) {
            QString mode=lines[0].mid(18);
            // Issue #1041 - MPD does not seem to persist user/client made replaygain changes, so store in Cantata's config file.
            Settings::self()->saveReplayGain(details.name, mode);
            emit replayGain(mode);
        } else {
            emit replayGain(QString());
        }
    }
}

void MPDConnection::goToNext()
{
    toggleStopAfterCurrent(false);
    stopVolumeFade();
    sendCommand("next");
}

static inline QByteArray value(bool b)
{
    return MPDConnection::quote(b ? 1 : 0);
}

void MPDConnection::setPause(bool toggle)
{
    toggleStopAfterCurrent(false);
    stopVolumeFade();
    sendCommand("pause "+value(toggle));
}

void MPDConnection::play()
{
    playFirstTrack(true);
}

void MPDConnection::playFirstTrack(bool emitErrors)
{
    toggleStopAfterCurrent(false);
    stopVolumeFade();
    sendCommand("play 0", emitErrors);
}

void MPDConnection::seek(qint32 offset)
{
    if (0==offset) {
        QObject *s=sender();
        offset=s ? s->property("offset").toInt() : 0;
        if (0==offset) {
            return;
        }
    }
    toggleStopAfterCurrent(false);
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        if (-1==sv.songId) {
            return;
        }
        if (offset>0) {
            if (sv.timeElapsed+offset<sv.timeTotal) {
                setSeek(sv.song, sv.timeElapsed+offset);
            } else {
                goToNext();
            }
        } else {
            if (sv.timeElapsed+offset>=0) {
                setSeek(sv.song, sv.timeElapsed+offset);
            } else {
                // Not sure about this!!!
                /*goToPrevious();*/
                setSeek(sv.song, 0);
            }
        }
    }
}

void MPDConnection::startPlayingSong(quint32 song)
{
    toggleStopAfterCurrent(false);
    sendCommand("play "+quote(song));
}

void MPDConnection::startPlayingSongId(qint32 songId)
{
    toggleStopAfterCurrent(false);
    stopVolumeFade();
    sendCommand("playid "+quote(songId));
}

void MPDConnection::goToPrevious()
{
    toggleStopAfterCurrent(false);
    stopVolumeFade();
    Response status=sendCommand("status");
    if (status.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(status.data);
        if (sv.timeElapsed>4) {
            setSeekId(sv.songId, 0);
            return;
        }
    }
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

void MPDConnection::setVolume(int vol) //Range accepted by MPD: 0-100
{
    if (-1==vol) {
        if (restoreVolume>=0) {
            sendCommand("setvol "+quote(restoreVolume), false);
        }
        if (volumeFade) {
            sendCommand("stop");
        }
        restoreVolume=-1;
    } else if (vol>=0) {
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
        if (!startVolumeFade()) {
            sendCommand("stop");
        }
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
        if (isMopidy()) {
            // Set version to 1 so that SQL cache is updated - it uses 0 as intial value
            dbUpdate=stats.dbUpdate=1;
        }
        emit statsUpdated(stats);
    }
}

void MPDConnection::getStatus()
{
   Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        lastStatusPlayQueueVersion=sv.playlist;
        if (currentSongId!=sv.songId) {
            stopVolumeFade();
        }
        if (stopAfterCurrent && (currentSongId!=sv.songId || (songPos>0 && sv.timeElapsed<(qint32)songPos))) {
            stopVolumeFade();
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
        emitStatusUpdated(sv);

        // If playlist length does not match number of IDs, then refresh
        if (sv.playlistLength!=playQueueIds.length()) {
            playListInfo();
        }
    }
}

void MPDConnection::getUrlHandlers()
{
    Response response=sendCommand("urlhandlers");
    if (response.ok) {
        handlers=MPDParseUtils::parseList(response.data, QByteArray("handler: ")).toSet();
        DBUG << handlers;
    }
}

void MPDConnection::getTagTypes()
{
    Response response=sendCommand("tagtypes");
    if (response.ok) {
        tagTypes=MPDParseUtils::parseList(response.data, QByteArray("tagtype: ")).toSet();
    }
}

void MPDConnection::getCover(const Song &song)
{
    int dataToRead = -1;
    int imageSize = 0;
    QByteArray imageData;
    bool firstRun = true;
    QString path=Utils::getDir(song.file);
    while (dataToRead != 0) {
        Response response=sendCommand("albumart "+encodeName(path)+" "+QByteArray::number(firstRun ? 0 : (imageSize - dataToRead)));
        if (!response.ok) {
            DBUG << "albumart query failed";
            break;
        }

        static const QByteArray constSize("size: ");
        static const QByteArray constBinary("binary: ");

        auto sizeStart = strstr(response.data.constData(), constSize.constData());
        if (!sizeStart) {
            DBUG << "Failed to get size start";
            break;
        }
        auto sizeEnd = strchr(sizeStart, '\n');
        if (!sizeEnd) {
            DBUG << "Failed to get size end";
            break;
        }

        auto chunkSizeStart = strstr(sizeEnd, constBinary.constData());
        if (!chunkSizeStart) {
            DBUG << "Failed to get chunk size start";
            break;
        }
        auto chunkSizeEnd = strchr(chunkSizeStart, '\n');
        if (!chunkSizeEnd) {
            DBUG << "Failed to chunk size end";
            break;
        }

        if (firstRun) {
            imageSize = QByteArray(sizeStart+constSize.length(), sizeEnd-(sizeStart+constSize.length())).toUInt();
            imageData.reserve(imageSize);
            dataToRead = imageSize;
            firstRun = false;
            DBUG << "image size" << imageSize;
        }

        int chunkSize = QByteArray(chunkSizeStart+constBinary.length(), chunkSizeEnd-(chunkSizeStart+constBinary.length())).toUInt();
        DBUG << "chunk size" << chunkSize;

        int startOfChunk=(chunkSizeEnd+1)-response.data.constData();
        if (startOfChunk+chunkSize > response.data.length()) {
            DBUG << "Invalid chunk size";
            break;
        }

        imageData.append(chunkSizeEnd+1, chunkSize);
        dataToRead -= chunkSize;
    }

    DBUG << dataToRead << imageData.size();
    emit albumArt(song, 0==dataToRead ? imageData : QByteArray());
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
        ConnectionReturn status=Success;
        if (wasConnected && Success!=(status=connectToMPD(idleSocket, true))) {
            // Failed to connect idle socket - so close *both*
            disconnectFromMPD();
            emit stateChanged(false);
            emit error(errorString(status), true);
        }
        if (QAbstractSocket::ConnectedState==idleSocket.state()) {
            connect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)), Qt::QueuedConnection);
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
    if (!response.ok) {
        DBUG << "idle failed? reconnect";
        disconnect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
        if (QAbstractSocket::ConnectedState==idleSocket.state()) {
            idleSocket.disconnectFromHost();
        }
        idleSocket.close();
        ConnectionReturn status=connectToMPD(idleSocket, true);
        if (Success!=status) {
            // Failed to connect idle socket - so close *both*
            disconnectFromMPD();
            emit stateChanged(false);
            emit error(errorString(status), true);
        }
        return;
    }

    QList<QByteArray> lines = data.split('\n');

    /*
     * See http://www.musicpd.org/doc/protocol/ch02.html
     */
    bool playListUpdated=false;
    bool statusUpdated=false;
    for (const QByteArray &line: lines) {
        if (line.startsWith(constIdleChangedKey)) {
            QByteArray value=line.mid(constIdleChangedKey.length());
            if (constIdleDbValue==value) {
                getStats();
                getStatus();
                playListInfo();
                playListUpdated=true;
            } else if (constIdleUpdateValue==value) {
                getStats();
                getStatus();
            } else if (constIdleStoredPlaylistValue==value) {
                listPlaylists();
                listStreams();
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
            } else if (constIdleStickerValue==value) {
                emit stickerDbChanged();
            } else if (constIdleSubscriptionValue==value) {
                //if (dynamicId.isEmpty()) {
                    setupRemoteDynamic();
                //}
            } else if (constIdleMessageValue==value) {
                readRemoteDynamicMessages();
            }
        }
    }

    DBUG << (void *)(&idleSocket) << "write idle";
    idleSocket.write("idle\n");
    idleSocket.waitForBytesWritten();
}

void MPDConnection::outputs()
{
    Response response=sendCommand("outputs");
    if (response.ok) {
        emit outputsUpdated(MPDParseUtils::parseOuputs(response.data));
    }
}

void MPDConnection::enableOutput(quint32 id, bool enable)
{
    if (sendCommand((enable ? "enableoutput " : "disableoutput ")+quote(id)).ok) {
        outputs();
    }
}

/*
 * Admin commands
 */
void MPDConnection::updateMaybe()
{
    if (!details.autoUpdate) {
        update();
    }
}

void MPDConnection::update()
{
    if (isMopidy()) {
        // Mopidy does not support MPD's update command. So, when user presses update DB, what we
        // just reload the library.
        loadLibrary();
    } else {
        sendCommand("update");
    }
}

/*
 * Database commands
 */
void MPDConnection::loadLibrary()
{
    DBUG << "loadLibrary";
    isListingMusic=true;
    emit updatingLibrary(dbUpdate);
    QList<Song> songs;
    recursivelyListDir("/", songs);
    emit updatedLibrary();
    isListingMusic=false;
}

void MPDConnection::listFolder(const QString &folder)
{
    DBUG << "listFolder" << folder;
    bool topLevel="/"==folder || ""==folder;
    Response response=sendCommand(topLevel ? "lsinfo" : ("lsinfo "+encodeName(folder)));
    QStringList subFolders;
    QList<Song> songs;
    if (response.ok) {
        MPDParseUtils::parseDirItems(response.data, QString(), ver, songs, folder, subFolders, MPDParseUtils::Loc_Browse);
    }
    emit folderContents(folder, subFolders, songs);
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
    // Don't report errors here. If user has disabled playlists, then MPD will report an error
    // Issues #1090 #1284
    Response response=sendCommand("listplaylists", false);
    if (response.ok) {
        QList<Playlist> playlists=MPDParseUtils::parsePlaylists(response.data);
        playlists.removeAll((Playlist(constStreamsPlayListName)));
        emit playlistsRetrieved(playlists);
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
        if (replace) {
            playFirstTrack(false);
        }
        emit playlistLoaded(name);
    }
}

void MPDConnection::renamePlaylist(const QString oldName, const QString newName)
{
    if (sendCommand("rename "+encodeName(oldName)+' '+encodeName(newName), false).ok) {
        emit playlistRenamed(oldName, newName);
    } else {
        emit error(tr("Failed to rename <b>%1</b> to <b>%2</b>").arg(oldName).arg(newName));
    }
}

void MPDConnection::removePlaylist(const QString &name)
{
    sendCommand("rm "+encodeName(name));
}

void MPDConnection::savePlaylist(const QString &name, bool overwrite)
{
    if (overwrite) {
        sendCommand("rm "+encodeName(name), false);
    }
    if (!sendCommand("save "+encodeName(name), false).ok) {
        emit error(tr("Failed to save <b>%1</b>").arg(name));
    }
}

void MPDConnection::addToPlaylist(const QString &name, const QStringList &songs, quint32 pos, quint32 size)
{
    if (songs.isEmpty()) {
        return;
    }

    if (!name.isEmpty()) {
        for (const QString &song: songs) {
            if (CueFile::isCue(song)) {
                emit error(tr("You cannot add parts of a cue sheet to a playlist!")+QLatin1String(" (")+song+QLatin1Char(')'));
                return;
            } else if (isPlaylist(song)) {
                emit error(tr("You cannot add a playlist to another playlist!")+QLatin1String(" (")+song+QLatin1Char(')'));
                return;
            }
        }
    }

    QStringList files;
    for (const QString &file: songs) {
        if (file.startsWith(constDirPrefix)) {
            files+=getAllFiles(file.mid(constDirPrefix.length()));
        } else if (file.startsWith(constPlaylistPrefix)) {
            files+=getPlaylistFiles(file.mid(constPlaylistPrefix.length()));
        } else {
            files.append(file);
        }
    }

    QByteArray encodedName=encodeName(name);
    QByteArray send = "command_list_begin\n";
    for (const QString &s: files) {
        send += "playlistadd "+encodedName+" "+encodeName(s)+'\n';
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

    std::sort(sorted.begin(), sorted.end());

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

void MPDConnection::setPriority(const QList<qint32> &ids, quint8 priority, bool decreasePriority)
{
    if (canUsePriority()) {
        QMap<qint32, quint8> tracks;
        QByteArray send = "command_list_begin\n";

        for (quint32 id: ids) {
            tracks.insert(id, priority);
            send += "prioid "+quote(priority)+" "+quote(id)+'\n';
            if (decreasePriority && priority>0) {
                priority--;
            }
        }

        send += "command_list_end";
        if (sendCommand(send).ok) {
            emit prioritySet(tracks);
        }
    }
}

void MPDConnection::search(const QString &field, const QString &value, int id)
{
    QList<Song> songs;
    QByteArray cmd;

    if (field==constModifiedSince) {
        time_t v=0;
        if (QRegExp("\\d*").exactMatch(value)) {
            v=QDateTime(QDateTime::currentDateTime().date()).toTime_t()-(value.toInt()*24*60*60);
        } else if (QRegExp("^((19|20)\\d\\d)[-/](0[1-9]|1[012])[-/](0[1-9]|[12][0-9]|3[01])$").exactMatch(value)) {
            QDateTime dt=QDateTime::fromString(QString(value).replace("/", "-"), Qt::ISODate);
            if (dt.isValid()) {
                v=dt.toTime_t();
            }
        }
        if (v>0) {
            cmd="find "+field.toLatin1()+" "+quote(v);
        }
    } else {
        cmd="search "+field.toLatin1()+" "+encodeName(value);
    }

    if (!cmd.isEmpty()) {
        Response response=sendCommand(cmd);
        if (response.ok) {
            songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Search);

            if (QLatin1String("any")==field) {
                // When searching on 'any' MPD ignores filename/paths! So, do another
                // search on these, and combine results.
                response=sendCommand("search file "+encodeName(value));
                if (response.ok) {
                    QList<Song> otherSongs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Search);
                    if (!otherSongs.isEmpty()) {
                        QSet<QString> fileNames;
                        for (const auto &s: songs) {
                            fileNames.insert(s.file);
                        }
                        for (const auto &s: otherSongs) {
                            if (!fileNames.contains(s.file)) {
                                songs.append(s);
                            }
                        }
                    }
                }
            }
            std::sort(songs.begin(), songs.end());
        }
    }
    emit searchResponse(id, songs);
}

void MPDConnection::search(const QByteArray &query, const QString &id)
{
    QList<Song> songs;
    if (query.isEmpty()) {
        Response response=sendCommand("list albumartist", false, false);
        if (response.ok) {
            QList<QByteArray> lines = response.data.split('\n');
            for (const QByteArray &line: lines) {
                if (line.startsWith("AlbumArtist: ")) {
                    Response resp = sendCommand("find albumartist " + encodeName(QString::fromUtf8(line.mid(13))) , false, false);
                    if (resp.ok) {
                        songs += MPDParseUtils::parseSongs(resp.data, MPDParseUtils::Loc_Search);
                    }
                }
            }
        }
    } else if (query.startsWith("RATING:")) {
        QList<QByteArray> parts = query.split(':');
        if (3==parts.length()) {
            Response response=sendCommand("sticker find song \"\" rating", false, false);
            if (response.ok) {
                int min = parts.at(1).toInt();
                int max = parts.at(2).toInt();
                QList<MPDParseUtils::Sticker> stickers=MPDParseUtils::parseStickers(response.data, constRatingSticker);
                if (!stickers.isEmpty()) {
                    for (const MPDParseUtils::Sticker &sticker: stickers) {
                        if (!sticker.file.isEmpty() && !sticker.value.isEmpty()) {
                            int val = sticker.value.toInt();
                            if (val>=min && val<=max) {
                                Response resp = sendCommand("find file " + encodeName(QString::fromUtf8(sticker.file)) , false, false);
                                if (resp.ok) {
                                    songs += MPDParseUtils::parseSong(resp.data, MPDParseUtils::Loc_Search);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        Response response=sendCommand(query);
        if (response.ok) {
            songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Search);
        }
    }
    emit searchResponse(id, songs);
}

void MPDConnection::listStreams()
{
    Response response=sendCommand("listplaylistinfo "+encodeName(constStreamsPlayListName), false);
    QList<Stream> streams;
    if (response.ok) {
        QList<Song> songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Streams);
        for (const Song &song: songs) {
            streams.append(Stream(song.file, song.name()));
        }    
    }
    clearError();
    emit streamList(streams);
}

void MPDConnection::saveStream(const QString &url, const QString &name)
{
    if (sendCommand("playlistadd "+encodeName(constStreamsPlayListName)+" "+encodeName(MPDParseUtils::addStreamName(url, name, true))).ok) {
        emit savedStream(url, name);
    }
}

void MPDConnection::removeStreams(const QList<quint32> &positions)
{
    if (positions.isEmpty()) {
        return;
    }

    QByteArray encodedName=encodeName(constStreamsPlayListName);
    QList<quint32> sorted=positions;
    QList<quint32> removed;

    std::sort(sorted.begin(), sorted.end());

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

    emit removedStreams(removed);
}

void MPDConnection::editStream(const QString &url, const QString &name, quint32 position)
{
    QByteArray encodedName=encodeName(constStreamsPlayListName);
    if (sendCommand("playlistdelete "+encodedName+" "+quote(position)).ok &&
        sendCommand("playlistadd "+encodedName+" "+encodeName(MPDParseUtils::addStreamName(url, name))).ok) {
//        emit editedStream(url, name, position);
        listStreams();
    }
}

void MPDConnection::sendClientMessage(const QString &channel, const QString &msg, const QString &clientName)
{
    if (!sendCommand("sendmessage "+channel.toUtf8()+" "+msg.toUtf8(), false).ok) {
        emit error(tr("Failed to send '%1' to %2. Please check %2 is registered with MPD.").arg(msg).arg(clientName.isEmpty() ? channel : clientName));
        emit clientMessageFailed(channel, msg);
    }
}

void MPDConnection::sendDynamicMessage(const QStringList &msg)
{
    // Check whether cantata-dynamic is still alive, by seeing if its channel is still open...
    if (1==msg.count() && QLatin1String("ping")==msg.at(0)) {
        Response response=sendCommand("channels");
        if (!response.ok || !MPDParseUtils::parseList(response.data, QByteArray("channel: ")).toSet().contains(constDynamicIn)) {
            emit dynamicSupport(false);
        }
        return;
    }

    QByteArray data;
    for (QString part: msg) {
        if (data.isEmpty()) {
            data+='\"'+part.toUtf8()+':'+dynamicId;
        } else {
            part=part.replace('\"', "{q}");
            part=part.replace("{", "{ob}");
            part=part.replace("}", "{cb}");
            part=part.replace("\n", "{n}");
            part=part.replace(":", "{c}");
            data+=':'+part.toUtf8();
        }
    }

    data+='\"';
    if (!sendCommand("sendmessage "+constDynamicIn+" "+data).ok) {
        emit dynamicSupport(false);
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

    std::sort(moveItems.begin(), moveItems.end());

    //first move all items (starting with the biggest) to the end so we don't have to deal with changing rownums
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        if (moveItems.at(i) < pos && moveItems.at(i) != size - 1) {
            // we are moving away an item that resides before the destination row, manipulate destination row
            posOffset++;
        }
        send += cmd;
        send += quote(moveItems.at(i));
        send += " ";
        send += quote(size - 1);
        send += '\n';
    }
    //now move all of them to the destination position
    for (int i = moveItems.size() - 1; i >= 0; i--) {
        send += cmd;
        send += quote(size - 1 - i);
        send += " ";
        send += quote(pos - posOffset);
        send += '\n';
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

bool MPDConnection::recursivelyListDir(const QString &dir, QList<Song> &songs)
{
    bool topLevel="/"==dir || ""==dir;

    if (topLevel && isMpd()) {
        // UPnP database backend does not list separate metadata items, so if "list genre" returns
        // empty response assume this is a UPnP backend and dont attempt to get rest of data...
        // Although we dont use "list XXX", lsinfo will return duplciate items (due to the way most
        // UPnP servers returing directories of classifications - Genre/Album/Tracks, Artist/Album/Tracks,
        // etc...
        Response response=sendCommand("list genre", false, false);
        if (!response.ok || response.data.split('\n').length()<3) { // 2 lines - OK and blank
            // ..just to be 100% sure, check no artists either...
            response=sendCommand("list artist", false, false);
            if (!response.ok || response.data.split('\n').length()<3) { // 2 lines - OK and blank
                return false;
            }
        }
    }

    Response response=sendCommand(topLevel
                                    ? serverInfo.getTopLevelLsinfo()
                                    : ("lsinfo "+encodeName(dir)));
    if (response.ok) {
        QStringList subDirs;
        MPDParseUtils::parseDirItems(response.data, details.dir, ver, songs, dir, subDirs, MPDParseUtils::Loc_Library);
        if (songs.count()>=200){
            QCoreApplication::processEvents();
            QList<Song> *copy=new QList<Song>();
            *copy << songs;
            emit librarySongs(copy);
            songs.clear();
        }
        for (const QString &sub: subDirs) {
            recursivelyListDir(sub, songs);
        }

        if (topLevel && !songs.isEmpty()) {
            QList<Song> *copy=new QList<Song>();
            *copy << songs;
            emit librarySongs(copy);
        }
        return true;
    } else {
        return false;
    }
}

QStringList MPDConnection::getPlaylistFiles(const QString &name)
{
    QStringList files;
    Response response=sendCommand("listplaylistinfo "+encodeName(name));
    if (response.ok) {
        QList<Song> songs=MPDParseUtils::parseSongs(response.data, MPDParseUtils::Loc_Playlists);
        emit playlistInfoRetrieved(name, songs);
        for (const Song &s: songs) {
            files.append(s.file);
        }    
    }
    return files;
}

QStringList MPDConnection::getAllFiles(const QString &dir)
{
    QStringList files;
    Response response=sendCommand("lsinfo "+encodeName(dir));
    if (response.ok) {
        QStringList subDirs;
        QList<Song> songs;
        MPDParseUtils::parseDirItems(response.data, details.dir, ver, songs, dir, subDirs, MPDParseUtils::Loc_Browse);
        for (const Song &song: songs) {
            if (Song::Playlist!=song.type) {
                files.append(song.file);
            }
        }
        for (const QString &sub: subDirs) {
            files+=getAllFiles(sub);
        }
    }

    return files;
}

bool MPDConnection::checkRemoteDynamicSupport()
{
    if (ver>=CANTATA_MAKE_VERSION(0,17,0)) {
        Response response;
        if (-1!=idleSocket.write("channels\n")) {
            idleSocket.waitForBytesWritten(constSocketCommsTimeout);
            response=readReply(idleSocket);
            if (response.ok) {
                return MPDParseUtils::parseList(response.data, QByteArray("channel: ")).toSet().contains(constDynamicIn);
            }
        }
    }
    return false;
}

bool MPDConnection::subscribe(const QByteArray &channel)
{
    if (-1!=idleSocket.write("subscribe \""+channel+"\"\n")) {
        idleSocket.waitForBytesWritten(constSocketCommsTimeout);
        Response response=readReply(idleSocket);
        if (response.ok || response.data.startsWith("ACK [56@0]")) { // ACK => already subscribed...
            DBUG << "Created subscription to " << channel;
            return true;
        } else {
            DBUG << "Failed to subscribe to " << channel;
        }
    } else {
        DBUG << "Failed to create subscribe to " << channel;
    }
    return false;
}

void MPDConnection::setupRemoteDynamic()
{
    if (checkRemoteDynamicSupport()) {
        DBUG << "cantata-dynamic is running";
        if (subscribe(constDynamicOut)) {
            if (dynamicId.isEmpty()) {
                dynamicId=QHostInfo::localHostName().toLatin1()+'.'+QHostInfo::localDomainName().toLatin1()+'-'+QByteArray::number(QCoreApplication::applicationPid());
                if (!subscribe(constDynamicOut+'-'+dynamicId)) {
                    dynamicId.clear();
                }
            }
        }
    } else {
        DBUG << "remote dynamic is not supported";
    }
    emit dynamicSupport(!dynamicId.isEmpty());
}

void MPDConnection::readRemoteDynamicMessages()
{
    if (-1!=idleSocket.write("readmessages\n")) {
        idleSocket.waitForBytesWritten(constSocketCommsTimeout);
        Response response=readReply(idleSocket);
        if (response.ok) {
            MPDParseUtils::MessageMap messages=MPDParseUtils::parseMessages(response.data);
            if (!messages.isEmpty()) {
                QList<QByteArray> channels=QList<QByteArray>() << constDynamicOut << constDynamicOut+'-'+dynamicId;
                for (const QByteArray &channel: channels) {
                    if (messages.contains(channel)) {
                        for (const QString &m: messages[channel]) {
                            if (!m.isEmpty()) {
                                DBUG << "Received message " << m;
                                QStringList parts=m.split(':', QString::SkipEmptyParts);
                                QStringList message;
                                for (QString part: parts) {
                                    part=part.replace("{c}", ":");
                                    part=part.replace("{n}", "\n");
                                    part=part.replace("{cb}", "}");
                                    part=part.replace("{ob}", "{");
                                    part=part.replace("{q}", "\"");
                                    message.append(part);
                                }
                                emit dynamicResponse(message);
                            }
                        }
                    }
                }
            }
        }
    }
}

int MPDConnection::getVolume()
{
    Response response=sendCommand("status");
    if (response.ok) {
        MPDStatusValues sv=MPDParseUtils::parseStatus(response.data);
        return sv.volume;
    }
    return -1;
}

void MPDConnection::setRating(const QString &file, quint8 val)
{
    if (val>Song::Rating_Max) {
        return;
    }

    if (!canUseStickers) {
        emit error(tr("Cannot store ratings, as the 'sticker' MPD command is not supported."));
        return;
    }

    bool ok=0==val
                ? sendCommand("sticker delete song "+encodeName(file)+' '+constRatingSticker, 0!=val).ok
                : sendCommand("sticker set song "+encodeName(file)+' '+constRatingSticker+' '+quote(val)).ok;

    if (!ok && 0==val) {
        clearError();
    }

    if (ok) {
        emit rating(file, val);
    } else {
        getRating(file);
    }
}

void MPDConnection::setRating(const QStringList &files, quint8 val)
{
    if (1==files.count()) {
        setRating(files.at(0), val);
        return;
    }

    if (!canUseStickers) {
        emit error(tr("Cannot store ratings, as the 'sticker' MPD command is not supported."));
        return;
    }

    QList<QStringList> fileLists;
    if (files.count()>constMaxFilesPerAddCommand) {
        int numChunks=(files.count()/constMaxFilesPerAddCommand)+(files.count()%constMaxFilesPerAddCommand ? 1 : 0);
        for (int i=0; i<numChunks; ++i) {
            fileLists.append(files.mid(i*constMaxFilesPerAddCommand, constMaxFilesPerAddCommand));
        }
    } else {
        fileLists.append(files);
    }

    bool ok=true;
    for (const QStringList &list: fileLists) {
        QByteArray cmd = "command_list_begin\n";

        for (const QString &f: list) {
            if (0==val) {
                cmd+="sticker delete song "+encodeName(f)+' '+constRatingSticker+'\n';
            } else {
                cmd+="sticker set song "+encodeName(f)+' '+constRatingSticker+' '+quote(val)+'\n';
            }
        }

        cmd += "command_list_end";
        ok=sendCommand(cmd, 0!=val).ok;
        if (!ok) {
            break;
        }
    }

    if (!ok && 0==val) {
        clearError();
    }
}

void MPDConnection::getRating(const QString &file)
{
    quint8 r=0;
    if (canUseStickers) {
        Response resp=sendCommand("sticker get song "+encodeName(file)+' '+constRatingSticker, false);
        if (resp.ok) {
            QByteArray val=MPDParseUtils::parseSticker(resp.data, constRatingSticker);
            if (!val.isEmpty()) {
                r=val.toUInt();
            }
        } else { // Ignore errors about uknown sticker...
            clearError();
        }
        if (r>Song::Rating_Max) {
            r=0;
        }
    }
    emit rating(file, r);
}

void MPDConnection::getStickerSupport()
{
    Response response=sendCommand("commands");
    canUseStickers=response.ok &&
        MPDParseUtils::parseList(response.data, QByteArray("command: ")).toSet().contains("sticker");
}

bool MPDConnection::fadingVolume()
{
    return volumeFade && QPropertyAnimation::Running==volumeFade->state();
}

bool MPDConnection::startVolumeFade()
{
    if (fadeDuration<=MinFade) {
        return false;
    }

    restoreVolume=getVolume();
    if (restoreVolume<5) {
        return false;
    }

    if (!volumeFade) {
        volumeFade = new QPropertyAnimation(this, "volume");
        volumeFade->setDuration(fadeDuration);
    }

    if (QPropertyAnimation::Running!=volumeFade->state()) {
        volumeFade->setStartValue(restoreVolume);
        volumeFade->setEndValue(-1);
        volumeFade->start();
    }
    return true;
}

void MPDConnection::stopVolumeFade()
{
    if (fadingVolume()) {
        volumeFade->stop();
        setVolume(restoreVolume);
        restoreVolume=-1;
    }
}

void MPDConnection::emitStatusUpdated(MPDStatusValues &v)
{
    if (restoreVolume>=0) {
        v.volume=restoreVolume;
    }
    #ifndef REPORT_MPD_ERRORS
    v.error=QString();
    #endif
    emit statusUpdated(v);
    if (!v.error.isEmpty()) {
        clearError();
    }
}

void MPDConnection::clearError()
{
    #ifdef REPORT_MPD_ERRORS
    if (isConnected()) {
        DBUG << __FUNCTION__;
        if (-1!=sock.write("clearerror\n")) {
            sock.waitForBytesWritten(500);
            readReply(sock);
        }
    }
    #endif
}

void MPDConnection::determineIfaceIp()
{
    static const QLatin1String ip4Local("127.0.0.1");
    if (!details.isLocal() && !details.hostname.isEmpty() && ip4Local!=details.hostname && QLatin1String("localhost")!=details.hostname) {
        QUdpSocket testSocket(this);
        testSocket.connectToHost(details.hostname, 1, QIODevice::ReadOnly);
        QString addr=testSocket.localAddress().toString();
        testSocket.close();
        if (!addr.isEmpty()) {
            DBUG << addr;
            emit ifaceIp(addr);
            return;
        }
    }
    DBUG << ip4Local;
    emit ifaceIp(ip4Local);
}

MpdSocket::MpdSocket(QObject *parent)
    : QObject(parent)
    , tcp(nullptr)
    , local(nullptr)
{
}

MpdSocket::~MpdSocket()
{
    deleteTcp();
    deleteLocal();
}

void MpdSocket::connectToHost(const QString &hostName, quint16 port, QIODevice::OpenMode mode)
{
    DBUG << "connectToHost" << hostName << port;
    if (hostName.startsWith('/') || hostName.startsWith('~')/* || hostName.startsWith('@')*/) {
        deleteTcp();
        if (!local) {
            local = new QLocalSocket(this);
            connect(local, SIGNAL(stateChanged(QLocalSocket::LocalSocketState)), this, SLOT(localStateChanged(QLocalSocket::LocalSocketState)));
            connect(local, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        }
        DBUG << "Connecting to LOCAL socket";
        QString host = Utils::tildaToHome(hostName);
        /*if ('@'==host[0]) {
            host[0]='\0';
        }*/
        local->connectToServer(host, mode);
    } else {
        deleteLocal();
        if (!tcp) {
            tcp = new QTcpSocket(this);
            connect(tcp, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SIGNAL(stateChanged(QAbstractSocket::SocketState)));
            connect(tcp, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        }
        DBUG << "Connecting to TCP socket";
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
        tcp=nullptr;
    }
}

void MpdSocket::deleteLocal()
{
    if (local) {
        disconnect(local, SIGNAL(stateChanged(QLocalSocket::LocalSocketState)), this, SLOT(localStateChanged(QLocalSocket::LocalSocketState)));
        disconnect(local, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        local->deleteLater();
        local=nullptr;
    }
}

// ONLY use this method to detect Non-MPD servers. The code which uses this will default to MPD
MPDServerInfo::ResponseParameter MPDServerInfo::lsinfoResponseParameters[] = {
    // github
    { "{lsinfo} Directory info not found for virtual-path '/", true, MPDServerInfo::ForkedDaapd, "forked-daapd" },
    // { "ACK [50@0] {lsinfo} Not found", false, MPDServerInfo::Mopidy, "Mopidy" },
    // ubuntu 16.10
    { "OK", false, MPDServerInfo::ForkedDaapd, "forked-daapd" }
};

void MPDServerInfo::detect() {
    MPDConnection *conn;

    if (!isUndetermined()) {
        return;
    }

    conn = MPDConnection::self();

    if (isUndetermined()) {
        MPDConnection::Response response=conn->sendCommand("stats");
        if (response.ok) {
            MPDStatsValues stats=MPDParseUtils::parseStats(response.data);
            if (0==stats.artists && 0==stats.albums && 0==stats.songs
                && 0==stats.uptime && 0==stats.playtime && 0==stats.dbPlaytime
                && 0==stats.dbUpdate) {
                setServerType(Mopidy);
                serverName = "Mopidy";
            }
        }
    }

    if (isUndetermined()) {
        MPDConnection::Response response=conn->sendCommand(lsinfoCommand, false, false);
        QList<QByteArray> lines = response.data.split('\n');
        bool match = false;
        int indx;
        for (const QByteArray &line: lines) {
            for (indx=0; indx<sizeof(lsinfoResponseParameters)/sizeof(ResponseParameter); ++indx) {
                ResponseParameter &rp = lsinfoResponseParameters[indx];
                if (rp.isSubstring) {
                    match = line.toLower().contains(rp.response.toLower());
                } else {
                    match = line.toLower() == rp.response.toLower();
                }
                if (match) {
                    setServerType(rp.serverType);
                    serverName = rp.name;
                    break;
                }
            }
            // first line is currently enough
            break;
        }
    }

    if (isUndetermined()) {
        // Default to MPD if cannot determine otherwise. Cantata is an *MPD* client first and foremost.
        setServerType(Mpd);
        serverName = "MPD";
    }

    DBUG << "detected serverType:" << getServerName() << "(" << getServerType() << ")";

    if (isMopidy()) {
        topLevelLsinfo = "lsinfo \"Local media\"";
    }

    if (isForkedDaapd()) {
        topLevelLsinfo = "lsinfo file:";

        QByteArray message = "sendmessage rating \"";
        message += "rating ";                           // sticker name
        message += QString().number(Song::Rating_Max);  // max rating
        message += " ";
        message += QString().number(Song::Rating_Step); // rating step (optional)
        message += "\"";
        conn->sendCommand(message, false, false);
    }
}

void MPDServerInfo::reset() {
    setServerType(MPDServerInfo::Undetermined);
    serverName = "undetermined";
    topLevelLsinfo = "lsinfo";
}

#include "moc_mpdconnection.cpp"
