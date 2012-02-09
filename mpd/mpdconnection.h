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

#ifndef MPDCONNECTION_H
#define MPDCONNECTION_H

#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QLocalSocket>
#include <QtCore/QDateTime>
#include "mpdstats.h"
#include "mpdstatus.h"
#include "song.h"
#include "output.h"
#include "playlist.h"

class MusicLibraryItemArtist;
class DirViewItemRoot;
class MusicLibraryItemRoot;

class MpdSocket : public QObject
{
    Q_OBJECT
public:
    MpdSocket(QObject *parent);
    virtual ~MpdSocket();

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
    void write(const QByteArray &data) {
        if (tcp) {
            tcp->write(data);
        } else if(local) {
            local->write(data);
        }
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

    bool isLocal() const { return 0!=local; }

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

class MPDConnection : public QObject
{
    Q_OBJECT

public:
    static MPDConnection * self();

    struct Response {
        Response(bool o=true, const QByteArray &d=QByteArray());
        bool ok;
        QByteArray data;
    };

    MPDConnection();
    ~MPDConnection();

    bool isLocal() const { return hostname.startsWith('/'); }

public Q_SLOTS:
    void setDetails(const QString &host, quint16 port, const QString &pass);
    // Current Playlist
    void add(const QStringList &files);
    void addid(const QStringList &files, quint32 pos, quint32 size);
    void currentSong();
    void playListInfo();
    void removeSongs(const QList<qint32> &items);
    void move(quint32 from, quint32 to);
    void move(const QList<quint32> &items, quint32 pos, quint32 size);
    void shuffle(quint32 from, quint32 to);
    void clear();
    void shuffle();

    // Playback
    void setCrossFade(int secs);
    void setReplayGain(const QString &v);
    void getReplayGain();
    void goToNext();
    void setPause(bool toggle);
    void startPlayingSong(quint32 song = 0);
    void startPlayingSongId(quint32 songId = 0);
    void goToPrevious();
    void setConsume(bool toggle);
    void setRandom(bool toggle);
    void setRepeat(bool toggle);
    void setSeek(quint32 song, quint32 time);
    void setSeekId(quint32 songId, quint32 time);
    void setVolume(int vol);
    void stopPlaying();

    // Output
    void outputs();
    void enableOutput(int id);
    void disableOutput(int id);

    // Miscellaneous
    void getStats();
    void getStatus();
    void getUrlHandlers();

    // Database
    void listAllInfo(const QDateTime &dbUpdate);
    void listAll();

    // Admin
    void update();

    // Playlists
    void listPlaylist();
    void listPlaylists();
    void playlistInfo(const QString &name);
    void loadPlaylist(QString name);
    void renamePlaylist(const QString oldName, const QString newName);
    void removePlaylist(QString name);
    void savePlaylist(QString name);
    void addToPlaylist(const QString &name, const QStringList &songs);
    void removeFromPlaylist(const QString &name, const QList<int> &positions);
    void moveInPlaylist(const QString &name, int id, int pos);

Q_SIGNALS:
    void stateChanged(bool connected);
    void currentSongUpdated(const Song &song);
    void playlistUpdated(const QList<Song> &songs);
    void statsUpdated();
    void statusUpdated();
    void storedPlayListUpdated();
    void outputsUpdated(const QList<Output> &outputs);
    void musicLibraryUpdated(MusicLibraryItemRoot * root, QDateTime db_update);
    void dirViewUpdated(DirViewItemRoot * root);
    void playlistsRetrieved(const QList<Playlist> &data);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);
    void removedFromPlaylist(const QString &name, const QList<int> &positions);
    void movedInPlaylist(const QString &name, int from, int to);
    void databaseUpdated();
    void playlistLoaded(const QString &playlist);
    void added(const QStringList &files);
    void replayGain(const QString &);
    void version(long);
    void updatingLibrary();
    void updatedLibrary();
    void updatingFileList();
    void updatedFileList();
    void error(const QString &err);
    void urlHandlers(const QStringList &handlers);

private Q_SLOTS:
    void idleDataReady();
    void onSocketStateChanged(QAbstractSocket::SocketState socketState);

private:
    bool connectToMPD();
    void disconnectFromMPD();
    bool connectToMPD(MpdSocket &socket, bool enableIdle=false);
    Response sendCommand(const QByteArray &command, bool emitErrors=true);
    void initialize();
    void parseIdleReturn(const QByteArray &data);

private:
    long ver;
    QString hostname;
    quint16 port;
    QString password;
    // Use 2 sockets, 1 for commands and 1 to receive MPD idle events.
    // Cant use 1, as we could write a command just as an idle event is ready to read
    MpdSocket sock;
    MpdSocket idleSocket;

    enum State
    {
        State_Blank,
        State_Connected,
        State_Disconnected
    };
    State state;
};

#endif
