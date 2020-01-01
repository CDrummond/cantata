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

#ifndef MPDCONNECTION_H
#define MPDCONNECTION_H

#include <QTcpSocket>
#include <QLocalSocket>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QStringList>
#include <QSet>
#include "mpdstats.h"
#include "mpdstatus.h"
#include "song.h"
#include "output.h"
#include "playlist.h"
#include "stream.h"
#include "config.h"
#include <time.h>

class QTimer;
class Thread;
class QPropertyAnimation;

class MpdSocket : public QObject
{
    Q_OBJECT
public:
    MpdSocket(QObject *parent);
    ~MpdSocket() override;

    void connectToHost(const QString &hostName, quint16 port, QIODevice::OpenMode mode = QIODevice::ReadWrite);
    void disconnectFromHost() {
        if (tcp) {
            tcp->disconnectFromHost();
        } else if(local) {
            local->disconnectFromServer();
        }
    }
    void close() {
        if (tcp) {
            tcp->close();
        } else if(local) {
            local->close();
        }
    }
    int write(const QByteArray &data) {
        if (tcp) {
            return tcp->write(data);
        } else if(local) {
            return local->write(data);
        }
        return 0;
    }
    void waitForBytesWritten(int msecs = 30000) {
        if (tcp) {
            tcp->waitForBytesWritten(msecs);
        } else if(local) {
            local->waitForBytesWritten(msecs);
        }
    }
    bool waitForReadyRead(int msecs = 30000) {
        return tcp ? tcp->waitForReadyRead(msecs)
                   : local
                        ? local->waitForReadyRead(msecs)
                        : false;
    }
    bool waitForConnected(int msecs = 30000) {
        return tcp ? tcp->waitForConnected(msecs)
                   : local
                        ? local->waitForConnected(msecs)
                        : false;
    }
    qint64 bytesAvailable() {
        return tcp ? tcp->bytesAvailable()
                   : local
                        ? local->bytesAvailable()
                        : 0;
    }
    QByteArray readAll() {
        return tcp ? tcp->readAll()
                   : local
                        ? local->readAll()
                        : QByteArray();
    }
    QAbstractSocket::SocketState state() const {
        return tcp ? tcp->state()
                   : local
                        ? (QAbstractSocket::SocketState)local->state()
                        : QAbstractSocket::UnconnectedState;
    }
    QNetworkProxy::ProxyType proxyType() const { return tcp ? tcp->proxy().type() : QNetworkProxy::NoProxy; }
    bool isLocal() const { return nullptr!=local; }
    QString address() const { return tcp ? tcp->peerAddress().toString() : QString(); }
    QString errorString() const { return tcp ? tcp->errorString() : local ? local->errorString() : QLatin1String("No socket object?"); }
    QAbstractSocket::SocketError error() const {
        return tcp ? tcp->error()
                   : local
                        ? (QAbstractSocket::SocketError)local->error()
                        : QAbstractSocket::UnknownSocketError;
    }
Q_SIGNALS:
    void stateChanged(QAbstractSocket::SocketState state);
    void readyRead();

private Q_SLOTS:
    void localStateChanged(QLocalSocket::LocalSocketState state);

private:
    void deleteTcp();
    void deleteLocal();

private:
    QTcpSocket *tcp;
    QLocalSocket *local;
};

struct MPDConnectionDetails {
    MPDConnectionDetails();
    MPDConnectionDetails(const MPDConnectionDetails &o) { *this=o; }
    QString getName() const;
    QString description() const;
    bool isLocal() const { return hostname.startsWith('/') /*|| hostname.startsWith('@')*/; }
    bool isEmpty() const { return hostname.isEmpty() || (!isLocal() && 0==port); }
    MPDConnectionDetails & operator=(const MPDConnectionDetails &o);
    bool operator==(const MPDConnectionDetails &o) const { return hostname==o.hostname && isLocal()==o.isLocal() && (isLocal() || port==o.port) && password==o.password; }
    bool operator!=(const MPDConnectionDetails &o) const { return !(*this==o); }
    bool operator<(const MPDConnectionDetails &o) const { return name.localeAwareCompare(o.name)<0; }
    static QString configGroupName(const QString &n=QString()) { return n.isEmpty() ? "Connection" : ("Connection-"+n); }
    void setDirReadable();

    QString name;
    QString hostname;
    quint16 port;
    QString password;
    QString dir;
    bool dirReadable;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    QString streamUrl;
    #endif
    QString replayGain;
    bool applyReplayGain;
    bool allowLocalStreaming;
    bool autoUpdate;
};

class MPDServerInfo {
public:
    enum ServerType {
        Undetermined,
        Mpd,
        Mopidy,
        ForkedDaapd,
        Unknown
    };

    MPDServerInfo() {
        reset();
        lsinfoCommand = "lsinfo \"mpd-client://cantata/";
        lsinfoCommand += PACKAGE_VERSION_STRING;
        lsinfoCommand += "\"";
    };

    void reset();
    void detect();

    ServerType getServerType() const { return serverType;}
    bool isUndetermined() const { return serverType == Undetermined; }
    bool isMpd() const { return serverType == Mpd; }
    bool isMopidy() const { return serverType == Mopidy; }
    bool isForkedDaapd() const { return serverType == ForkedDaapd; }
    bool isPlayQueueIdValid() const { return serverType != ForkedDaapd; }

    const QString &getServerName() const { return serverName;}
    const QByteArray &getTopLevelLsinfo() const { return topLevelLsinfo; }

private:
    void setServerType(ServerType newServerType) { serverType = newServerType; }

    ServerType serverType;
    QString serverName;
    QByteArray topLevelLsinfo;

    struct ResponseParameter {
        QByteArray response;
        bool isSubstring;
        ServerType serverType;
        QString name;
    };

    QByteArray lsinfoCommand;
    static ResponseParameter lsinfoResponseParameters[];
};

class MPDConnection : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int volume READ getVolume WRITE setVolume)

public:
    enum AddAction
    {
        Append,
        Replace,
        ReplaceAndplay,
        AppendAndPlay,
        AddAndPlay,
        AddAfterCurrent
    };

    enum VolumeFade
    {
        MinFade     = 400,
        MaxFade     = 4000,
        DefaultFade = MinFade // disable volume fade by default. prev:2000
    };

    static const QString constModifiedSince;
    static const int constMaxPqChanges;
    static const QString constStreamsPlayListName;
    static const QString constPlaylistPrefix;
    static const QString constDirPrefix;

    static MPDConnection * self();
    static QByteArray quote(int val);
    static QByteArray encodeName(const QString &name);

    struct Response {
        Response(bool o=true, const QByteArray &d=QByteArray());
        QString getError(const QByteArray &command);
        bool ok;
        QByteArray data;
    };

    static void enableDebug();

    MPDConnection();
    ~MPDConnection() override;

    void start();
    const MPDConnectionDetails & getDetails() const { return details; }
    void setDirReadable() { details.setDirReadable(); }
    bool isConnected() const { return State_Connected==state; }
    bool canUsePriority() const { return ver>=CANTATA_MAKE_VERSION(0, 17, 0) && isMpd(); }
    const QSet<QString> & urlHandlers() const { return handlers; }
    const QSet<QString> & tags() const { return tagTypes; }
    bool composerTagSupported() const { return tagTypes.contains(QLatin1String("Composer")); }
    bool commentTagSupported() const { return tagTypes.contains(QLatin1String("Comment")); }
    bool performerTagSupported() const { return tagTypes.contains(QLatin1String("Performer")); }
    bool originalDateTagSupported() const { return tagTypes.contains(QLatin1String("OriginalDate")); }
    bool modifiedFindSupported() const { return ver>=CANTATA_MAKE_VERSION(0, 19, 0); }
    bool replaygainSupported() const { return ver>=CANTATA_MAKE_VERSION(0, 16, 0); }
    bool supportsCoverDownload() const { return ver>=CANTATA_MAKE_VERSION(0, 21, 0) && isMpd(); }
    bool localFilePlaybackSupported() const;
    bool stickersSupported() const { return canUseStickers; }

    long version() const { return ver; }
    static bool isPlaylist(const QString &file);
    int unmuteVolume() { return unmuteVol; }
    bool isMuted() { return -1!=unmuteVol; }
    bool isMpd() const { return serverInfo.isMpd(); }
    bool isMopidy() const { return serverInfo.isMopidy(); }
    bool isForkedDaapd() const { return serverInfo.isForkedDaapd(); }
    bool isPlayQueueIdValid() const { return serverInfo.isPlayQueueIdValid(); }
    void setVolumeFadeDuration(int f) { fadeDuration=f; }
    QString ipAddress() const { return details.isLocal() ? QString() : sock.address(); }

public Q_SLOTS:
    void stop();
    void reconnect();
    void setDetails(const MPDConnectionDetails &d);
//    void disconnectMpd();
    // Current Playlist
    void add(const QStringList &files, int action, quint8 priority, bool decreasePriority);
    void add(const QStringList &files, quint32 pos, quint32 size, int action, quint8 priority, bool decreasePriority);
    void add(const QStringList &files, quint32 pos, quint32 size, int action, const QList<quint8> &priority);
    void add(const QStringList &files, quint32 pos, quint32 size, int action, QList<quint8> priority, bool decreasePriority);
    void populate(const QStringList &files, const QList<quint8> &priority);
    void addAndPlay(const QString &file);
    void currentSong();
    void playListChanges();
    void playListInfo();
    void removeSongs(const QList<qint32> &items);
    void move(quint32 from, quint32 to);
    void move(const QList<quint32> &items, quint32 pos, quint32 size);
    void setOrder(const QList<quint32> &items);
    void shuffle(quint32 from, quint32 to);
    void clear();
    void shuffle();

    // Playback
    void setCrossFade(int secs);
    void setReplayGain(const QString &v);
    void getReplayGain();
    void goToNext();
    void setPause(bool toggle);
    void play();
    void startPlayingSong(quint32 song = 0);
    void startPlayingSongId(qint32 songId = 0);
    void goToPrevious();
    void setConsume(bool toggle);
    void setRandom(bool toggle);
    void setRepeat(bool toggle);
    void setSingle(bool toggle);
    void setSeek(quint32 song, quint32 time);
    void setSeekId(qint32 songId, quint32 time);
    void setVolume(int vol);
    void toggleMute();
    void stopPlaying(bool afterCurrent=false);
    void clearStopAfter();

    // Output
    void outputs();
    void enableOutput(quint32 id, bool enable);

    // Miscellaneous
    void getStats();
    void getStatus();
    void getUrlHandlers();
    void getTagTypes();
    void getCover(const Song &song);

    // Database
    void loadLibrary();
    void listFolder(const QString &folder);

    // Admin
    void updateMaybe();
    void update();

    // Playlists
//     void listPlaylist(const QString &name);
    void listPlaylists();
    void playlistInfo(const QString &name);
    void loadPlaylist(const QString &name, bool replace);
    void renamePlaylist(const QString oldName, const QString newName);
    void removePlaylist(const QString &name);
    void savePlaylist(const QString &name, bool overwrite);
    void addToPlaylist(const QString &name, const QStringList &songs) { addToPlaylist(name, songs, 0, 0); }
    void addToPlaylist(const QString &name, const QStringList &songs, quint32 pos, quint32 size);
    void removeFromPlaylist(const QString &name, const QList<quint32> &positions);
    void moveInPlaylist(const QString &name, const QList<quint32> &items, quint32 row, quint32 size);

    void setPriority(const QList<qint32> &ids, quint8 priority, bool decreasePriority);

    void search(const QString &field, const QString &value, int id);
    void search(const QByteArray &query, const QString &id);

    void listStreams();
    void saveStream(const QString &url, const QString &name);
    void removeStreams(const QList<quint32> &positions);
    void editStream(const QString &url, const QString &name, quint32 position);

    void sendClientMessage(const QString &channel, const QString &msg, const QString &clientName);
    void sendDynamicMessage(const QStringList &msg);
    int getVolume();

    void setRating(const QString &file, quint8 val);
    void setRating(const QStringList &files, quint8 val);
    void getRating(const QString &file);

    void seek(qint32 offset=0);

Q_SIGNALS:
    void connectionChanged(const MPDConnectionDetails &details);
    void connectionNotChanged(const QString &name);
    void stateChanged(bool connected);
    void passwordError();
    void currentSongUpdated(const Song &song);
    void playlistUpdated(const QList<Song> &songs, bool isComplete);
    void statsUpdated(const MPDStatsValues &stats);
    void statusUpdated(const MPDStatusValues &status);
    void outputsUpdated(const QList<Output> &outputs);
    void librarySongs(QList<Song> *songs);
    void folderContents(const QString &folder, const QStringList &subFolders, const QList<Song> &songs);
    void playlistsRetrieved(const QList<Playlist> &data);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);
    void playlistRenamed(const QString &from, const QString &to);
    void removedFromPlaylist(const QString &name, const QList<quint32> &positions);
    void movedInPlaylist(const QString &name, const QList<quint32> &items, quint32 pos);
    void updatingDatabase();
    void updatedDatabase();
    void playlistLoaded(const QString &playlist);
    void added(const QStringList &files);
    void replayGain(const QString &);
    void updatingLibrary(time_t dbUpdate);
    void updatedLibrary();
    void updatingFileList();
    void updatedFileList();
    void error(const QString &err, bool showActions=false);
    void info(const QString &msg);
    void dirChanged();
    void prioritySet(const QMap<qint32, quint8> &tracks);

    void stopAfterCurrentChanged(bool afterCurrent);
    void streamUrl(const QString &url);

    void searchResponse(int id, const QList<Song> &songs);
    void searchResponse(const QString &id, const QList<Song> &songs);

    void socketAddress(const QString &addr);
    void cantataStreams(const QStringList &files);
    void cantataStreams(const QList<Song> &songs, bool isUpdate);
    void removedIds(const QSet<qint32> &ids);

    void savedStream(const QString &url, const QString &name);
    void removedStreams(const QList<quint32> &removed);
    void editedStream(const QString &url, const QString &name, quint32 position);
    void streamList(const QList<Stream> &streams);

    void clientMessageFailed(const QString &client, const QString &msg);
    void dynamicSupport(bool e);
    void dynamicResponse(const QStringList &resp);

    void rating(const QString &file, quint8 val);
    void stickerDbChanged();

    void ifaceIp(const QString &addr);

    void albumArt(const Song &song, const QByteArray &data);

private Q_SLOTS:
    void idleDataReady();
    void onSocketStateChanged(QAbstractSocket::SocketState socketState);

private:
    enum ConnectionReturn
    {
        Success,
        Failed,
        ProxyError,
        IncorrectPassword
    };

    static ConnectionReturn convertSocketCode(MpdSocket &socket);
    QString errorString(ConnectionReturn status) const;
    ConnectionReturn connectToMPD();
    void disconnectFromMPD();
    ConnectionReturn connectToMPD(MpdSocket &socket, bool enableIdle=false);
    Response sendCommand(const QByteArray &command, bool emitErrors=true, bool retry=true);
    void initialize();
    void parseIdleReturn(const QByteArray &data);
    bool doMoveInPlaylist(const QString &name, const QList<quint32> &items, quint32 pos, quint32 size);
    void toggleStopAfterCurrent(bool afterCurrent);
    bool recursivelyListDir(const QString &dir, QList<Song> &songs);
    QStringList getPlaylistFiles(const QString &name);
    QStringList getAllFiles(const QString &dir);
    bool checkRemoteDynamicSupport();
    bool subscribe(const QByteArray &channel);
    void setupRemoteDynamic();
    void readRemoteDynamicMessages();
    bool fadingVolume();
    bool startVolumeFade();
    void stopVolumeFade();
    void emitStatusUpdated(MPDStatusValues &v);
    void clearError();
    void getRatings(QList<Song> &songs);
    void getStickerSupport();
    void playFirstTrack(bool emitErrors);
    void determineIfaceIp();

private:
    bool isInitialConnect;
    Thread *thread;
    long ver;
    QSet<QString> handlers;
    QSet<QString> tagTypes;
    bool canUseStickers;
    MPDConnectionDetails details;
    time_t dbUpdate;
    // Use 2 sockets, 1 for commands and 1 to receive MPD idle events.
    // Cant use 1, as we could write a command just as an idle event is ready to read
    MpdSocket sock;
    MpdSocket idleSocket;
    QTimer *connTimer;
    QByteArray dynamicId;

    // The three items are used so that we can do quick playqueue updates...
    QList<qint32> playQueueIds;
    QSet<qint32> streamIds;
    quint32 lastStatusPlayQueueVersion;
    quint32 lastUpdatePlayQueueVersion;

    enum State
    {
        State_Blank,
        State_Connected,
        State_Disconnected
    };
    State state;
    bool isListingMusic;
    QTimer *reconnectTimer;
    time_t reconnectStart;

    bool stopAfterCurrent;
    qint32 currentSongId;
    quint32 songPos; // USe for stop-after-current when we only have 1 songin playqueue!
    int unmuteVol;
    friend class MPDServerInfo;
    MPDServerInfo serverInfo;
    bool isUpdatingDb;

    QPropertyAnimation *volumeFade;
    int fadeDuration;
    int restoreVolume;
};

#endif
