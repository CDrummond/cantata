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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KGlobal>
#endif
#include <QtGui/QApplication>
#include "debugtimer.h"

//#undef qDebug
//#define qDebug qWarning

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(MPDConnection, conn)
#endif

MPDConnection * MPDConnection::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return conn;
    #else
    static MPDConnection *conn=0;;
    if(!conn) {
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
            qDebug() << (void *)(&socket) << "Waiting for read data.";
            if (socket.waitForReadyRead(5000)) {
                break;
            }
        }

        data.append(socket.readAll());

        if (data.endsWith("OK\n") || data.startsWith("OK") || data.startsWith("ACK")) {
            break;
        }
    }
    if(data.size()>256) {
        qDebug() << (void *)(&socket) << "Read (bytes):" << data.size();
    } else {
        qDebug() << (void *)(&socket) << "Read:" << data;
    }

    return data;
}

MPDConnection::Response readReply(MpdSocket &socket)
{
    QByteArray data = readFromSocket(socket);
    return MPDConnection::Response(data.endsWith("OK\n"), data);
}

MPDConnection::MPDConnection()
    : ver(0)
    , ui(0)
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
}

MPDConnection::~MPDConnection()
{
    disconnect(&sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    disconnect(&idleSocket, SIGNAL(readyRead()), this, SLOT(idleDataReady()));
    sock.disconnectFromHost();
    idleSocket.disconnectFromHost();
}

bool MPDConnection::connectToMPD(MpdSocket &socket, bool enableIdle)
{
    if (socket.state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Connecting" << enableIdle << (enableIdle ? "(idle)" : "(std)") << (void *)(&socket);
        if (hostname.isEmpty() || port == 0) {
            qDebug("MPDConnection: no hostname and/or port supplied.");
            return false;
        }

        socket.connectToHost(hostname, port);
        if (socket.waitForConnected(5000)) {
            qDebug("MPDConnection established");
            QByteArray recvdata = readFromSocket(socket);

            if (recvdata.startsWith("OK MPD")) {
                qDebug("Received identification string");
            }

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
                qDebug("MPDConnection: setting password...");
                QByteArray senddata = "password ";
                senddata += password.toUtf8();
                senddata += "\n";
                socket.write(senddata);
                socket.waitForBytesWritten(5000);
                if (readReply(socket).ok) {
                    qDebug("MPDConnection: password accepted");
                } else {
                    qDebug("MPDConnection: password rejected");
                    socket.close();
                    return false;
                }
            }

            if(enableIdle) {
                connect(&socket, SIGNAL(readyRead()), this, SLOT(idleDataReady()));
                qDebug() << "Enabling idle";
                socket.write("idle\n");
                socket.waitForBytesWritten();
            }
            connect(&socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
            return true;
        } else {
            qDebug("Couldn't connect");
            return false;
        }
    }

//     qDebug() << "Already connected" << (enableIdle ? "(idle)" : "(std)");
    return true;
}

bool MPDConnection::connectToMPD()
{
    State old=state;
    if (connectToMPD(sock) && connectToMPD(idleSocket, true)) {
        state=State_Connected;
    } else {
        state=State_Disconnected;
    }

    if (state!=old) {
        emit stateChanged(State_Connected==state);
    }
    return State_Connected==state;
}

void MPDConnection::disconnectFromMPD()
{
    qDebug() << "disconnectFromMPD" << QThread::currentThreadId();
    disconnect(&sock, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
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
    if (State_Connected==state) {
        emit stateChanged(false);
    }
    state=State_Disconnected;
}

void MPDConnection::setDetails(const QString &host, quint16 p, const QString &pass)
{
    if (hostname!=host || (!sock.isLocal() && port!=p) || password!=pass) {
        qDebug() << "setDetails" << host << p << (pass.isEmpty() ? false : true);
        disconnectFromMPD();
        hostname=host;
        port=p;
        password=pass;
        qDebug() << "call connectToMPD";
        connectToMPD();
    }
}

MPDConnection::Response MPDConnection::sendCommand(const QByteArray &command)
{
    if (!connectToMPD()) {
        return Response(false);
    }

    qDebug() << (void *)(&sock) << "command: " << command;

    sock.write(command);
    sock.write("\n");
    sock.waitForBytesWritten(5000);
    Response response=readReply(sock);

    if (!response.ok) {
        qDebug() << command << "failed";
    }
    return response;
}

/*
 * Playlist commands
 */

void MPDConnection::add(const QStringList &files)
{
    QByteArray send = "command_list_begin\n";

    for (int i = 0; i < files.size(); i++) {
        send += "add \"";
        send += files.at(i).toUtf8();
        send += "\"\n";
    }

    send += "command_list_end";

    if (sendCommand(send).ok){
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
void MPDConnection::addid(const QStringList &files, quint32 pos, quint32 size)
{
    QByteArray send = "command_list_begin\n";
    int cur_size = size;
    for (int i = 0; i < files.size(); i++) {
        send += "add \"";
        send += files.at(i).toUtf8();
        send += "\"";
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
    sendCommand("clear");
}

void MPDConnection::removeSongs(const QList<qint32> &items)
{
    QByteArray send = "command_list_begin\n";

    for (int i = 0; i < items.size(); i++) {
        send += "deleteid ";
        send += QByteArray::number(items.at(i));
        send += "\n";
    }

    send += "command_list_end";

    sendCommand(send);
}

void MPDConnection::move(quint32 from, quint32 to)
{
    QByteArray send = "move " + QByteArray::number(from) + " " + QByteArray::number(to);

    qDebug() << send;
    sendCommand(send);
}

void MPDConnection::move(const QList<quint32> &items, quint32 pos, quint32 size)
{
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
}

void MPDConnection::shuffle()
{
    sendCommand("shuffle");
}

void MPDConnection::shuffle(quint32 from, quint32 to)
{
    QByteArray command = "shuffle ";
    command += QByteArray::number(from);
    command += ":";
    command += QByteArray::number(to + 1);

    sendCommand(command);
}

void MPDConnection::currentSong()
{
    Response response=sendCommand("currentsong");
    if(response.ok) {
        emit currentSongUpdated(MPDParseUtils::parseSong(response.data));
    }
}

void MPDConnection::playListInfo()
{
    Response response=sendCommand("playlistinfo");
    if(response.ok) {
        emit playlistUpdated(MPDParseUtils::parseSongs(response.data));
    }
}

/*
 * Playback commands
 */
void MPDConnection::setCrossFade(int secs)
{
    QByteArray data = "crossfade ";
    data += QByteArray::number(secs);
    sendCommand(data);
}

void MPDConnection::setReplayGain(const QString &v)
{
    QByteArray data = "replay_gain_mode ";
    data += v.toLatin1();
    sendCommand(data);
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

void MPDConnection::setPause(bool toggle)
{
    QByteArray data = "pause ";
    if (toggle == true)
        data += "1";
    else
        data += "0";

    sendCommand(data);
}

void MPDConnection::startPlayingSong(quint32 song)
{
    QByteArray data = "play ";
    data += QByteArray::number(song);
    sendCommand(data);
}

void MPDConnection::startPlayingSongId(quint32 song_id)
{
    QByteArray data = "playid ";
    data += QByteArray::number(song_id);
    sendCommand(data);
}

void MPDConnection::goToPrevious()
{
    sendCommand("previous");
}

void MPDConnection::setConsume(bool toggle)
{
    QByteArray data = "consume ";
    if (toggle == true)
        data += "1";
    else
        data += "0";

    sendCommand(data);
}

void MPDConnection::setRandom(bool toggle)
{
    QByteArray data = "random ";
    if (toggle == true)
        data += "1";
    else
        data += "0";

    sendCommand(data);
}

void MPDConnection::setRepeat(bool toggle)
{
    QByteArray data = "repeat ";
    if (toggle == true)
        data += "1";
    else
        data += "0";

    sendCommand(data);
}

void MPDConnection::setSeek(quint32 song, quint32 time)
{
    QByteArray data = "seek ";
    data += QByteArray::number(song);
    data += " ";
    data += QByteArray::number(time);

    sendCommand(data);
}

void MPDConnection::setSeekId(quint32 song_id, quint32 time)
{
    QByteArray data = "seekid ";
    data += QByteArray::number(song_id);
    data += " ";
    data += QByteArray::number(time);
    sendCommand(data);
}

void MPDConnection::setVolume(int vol)
{
    QByteArray data = "setvol ";
    data += QByteArray::number(vol);
    sendCommand(data);
}

void MPDConnection::stopPlaying()
{
    sendCommand("stop");
}

void MPDConnection::getStats()
{
    Response response=sendCommand("stats");
    if(response.ok) {
        MPDParseUtils::parseStats(response.data);
        emit statsUpdated();
    }
}

void MPDConnection::getStatus()
{
    Response response=sendCommand("status");
    if(response.ok) {
        MPDParseUtils::parseStatus(response.data);
        emit statusUpdated();
    }
}

/*
 * Data is written during idle.
 * Retrieve it and parse it
 */
void MPDConnection::idleDataReady()
{
    qDebug() << "idleDataReady";
    QByteArray data;

    if (idleSocket.bytesAvailable() == 0) {
        return;
    }

    parseIdleReturn(readFromSocket(idleSocket));
    qDebug() << "write idle";
    idleSocket.write("idle\n");
    idleSocket.waitForBytesWritten();
}

/*
 * Socket state has changed, currently we only use this to gracefully
 * handle disconnects.
 */
void MPDConnection::onSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    if (socketState == QAbstractSocket::ClosingState){
        bool wasConnected=State_Connected==state;
        qDebug() << "onSocketStateChanged";
        disconnectFromMPD();
        if (wasConnected) {
            connectToMPD();
        }
    }
}

/*
 * Parse data returned by the mpd deamon on an idle commond.
 */
void MPDConnection::parseIdleReturn(const QByteArray &data)
{
    qDebug() << "parseIdleReturn:" << data;
    QList<QByteArray> lines = data.split('\n');

    QByteArray line;

    /*
     * See http://www.musicpd.org/doc/protocol/ch02.html
     */
    foreach(line, lines) {
        if (line == "changed: database") {
            /*
             * Temp solution
             */
            getStats();
        } else if (line == "changed: update") {
            emit databaseUpdated();
        } else if (line == "changed: stored_playlist") {
            emit storedPlayListUpdated();
        } else if (line == "changed: playlist") {
            playListInfo();
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
            qDebug() << "Unknown command in idle return: " << line;
        }
    }
}

void MPDConnection::outputs()
{
    Response response=sendCommand("outputs");
    if(response.ok) {
        emit outputsUpdated(MPDParseUtils::parseOuputs(response.data));
    }
}

void MPDConnection::enableOutput(int id)
{
    QByteArray data = "enableoutput ";
    data += QByteArray::number(id);
    sendCommand(data);
}

void MPDConnection::disableOutput(int id)
{
    QByteArray data = "disableoutput ";
    data += QByteArray::number(id);
    sendCommand(data);
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
    if(response.ok) {
        qDebug() << "parseLibraryItems:" << response.data;
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
    if(response.ok) {
        emit dirViewUpdated(MPDParseUtils::parseDirViewItems(response.data));
    }
    emit updatedFileList();
}

/*
 * Playlists commands
 */
void MPDConnection::listPlaylist()
{
    sendCommand("listplaylist");
}

void MPDConnection::listPlaylists()
{
    TF_DEBUG
    Response response=sendCommand("listplaylists");
    if(response.ok) {
        emit playlistsRetrieved(MPDParseUtils::parsePlaylists(response.data));
    }
}

void MPDConnection::playlistInfo(const QString &name)
{
    TF_DEBUG
    QByteArray data = "listplaylistinfo ";
    data += encodeName(name);
    Response response=sendCommand(data);
    if (response.ok) {
        emit playlistInfoRetrieved(name, MPDParseUtils::parseSongs(response.data));
    }
}

void MPDConnection::loadPlaylist(QString name)
{
    QByteArray data("load ");
    data += encodeName(name);

    if (sendCommand(data).ok) {
        emit playlistLoaded(name);
    }
}

void MPDConnection::renamePlaylist(const QString oldName, const QString newName)
{
    QByteArray data("rename ");
    data += encodeName(oldName);
    data += " ";
    data += encodeName(newName);

    if (!sendCommand(data).ok && ui) {
        #ifdef ENABLE_KDE_SUPPORT
        QString message=i18n("Sorry, failed to rename <b>%1</b> to <b>%2</b>").arg(oldName).arg(newName);
        #else
        QString message=tr("Sorry, failed to rename <b>%1</b> to <b>%2</b>").arg(oldName).arg(newName);
        #endif
        QMetaObject::invokeMethod(ui, "showError", Qt::QueuedConnection, Q_ARG(QString, message));
    }
}

void MPDConnection::removePlaylist(QString name)
{
    QByteArray data("rm ");
    data += encodeName(name);

    sendCommand(data);
}

void MPDConnection::savePlaylist(QString name)
{
    QByteArray data("save ");
    data += encodeName(name);

    if (!sendCommand(data).ok && ui) {
        #ifdef ENABLE_KDE_SUPPORT
        QString message=i18n("Sorry, failed to save <b>%1</b>").arg(name);
        #else
        QString message=tr("Sorry, failed to save <b>%1</b>").arg(name);
        #endif
        QMetaObject::invokeMethod(ui, "showError", Qt::QueuedConnection, Q_ARG(QString, message));
    }
}

void MPDConnection::addToPlaylist(const QString &name, const QStringList &songs)
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
        playlistInfo(name);
    }
}

void MPDConnection::removeFromPlaylist(const QString &name, const QList<int> &positions)
{
    if (positions.isEmpty()) {
        return;
    }

    QByteArray encodedName=encodeName(name);
    QList<int> sorted=positions;

    qSort(sorted);
    QList<int> fixedPositions;
    QMap<int, int> map;

    for (int i=0; i<sorted.count(); ++i) {
        fixedPositions << (sorted.at(i)-i);
    }
    QList<int> removed;
    for (int i=0; i<fixedPositions.count(); ++i) {
        QByteArray data = "playlistdelete ";
        data += encodedName;
        data += " ";
        data += QByteArray::number(fixedPositions.at(i));
        if (sendCommand(data).ok) {
            removed << sorted.at(i);
        } else {
            break;
        }
    }

    if (!removed.isEmpty()) {
        emit removedFromPlaylist(name, removed);
    }
}

void MPDConnection::moveInPlaylist(const QString &name, int from, int to)
{
    QByteArray data = "playlistmove ";
    data += encodeName(name);
    data += " ";
    data += QByteArray::number(from);
    data += " ";
    data += QByteArray::number(to);
    if (sendCommand(data).ok) {
        emit movedInPlaylist(name, from, to);
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
