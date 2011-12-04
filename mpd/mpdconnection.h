/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QMutex>
#include <QThread>
#include <QTcpSocket>
#include <QDateTime>
#include "mpdstats.h"
#include "mpdstatus.h"
#include "song.h"
#include "output.h"
#include "playlist.h"

class MusicLibraryItemArtist;
class DirViewItemRoot;
class MusicLibraryItemRoot;

class MPDConnection : public QThread
{
    Q_OBJECT

public:
    static MPDConnection * self();

    struct Response {
        Response(bool o=true, const QByteArray &d=QByteArray()) : ok(o), data(d) { }
        bool ok;
        QByteArray data;
    };

    MPDConnection();
    ~MPDConnection();

    bool connectToMPD();
    void disconnectFromMPD();
    void setDetails(const QString &host, const quint16 port, const QString &pass);
    bool isConnected();

public Q_SLOTS:
    // Playlist
    void add(const QStringList &files);
    void addid(const QStringList &files, const quint32 pos, const quint32 size);
    void currentSong();
    void playListInfo();
    void removeSongs(const QList<qint32> &items);
    void move(const quint32 from, const quint32 to);
    void move(const QList<quint32> items, const quint32 pos, const quint32 size);
    void shuffle(const quint32 from, const quint32 to);
    void clear();
    void shuffle();

    // Playback
    void setCrossFade(const quint8 secs);
    void setReplayGain(const QString &v);
    QString getReplayGain();
    void goToNext();
    void setPause(const bool toggle);
    void startPlayingSong(const quint32 song = 0);
    void startPlayingSongId(const quint32 song_id = 0);
    void goToPrevious();
    void setConsume(const bool toggle);
    void setRandom(const bool toggle);
    void setRepeat(const bool toggle);
    void setSeek(const quint32 song, const quint32 time);
    void setSeekId(const quint32 song_id, const quint32 time);
    void setVolume(const quint8 vol);
    void stopPlaying();

    // Output
    void outputs();
    void enableOutput(const quint8 id);
    void disableOutput(const quint8 id);

    // Miscellaneous
    void getStats();
    void getStatus();

    // Database
    void listAllInfo(QDateTime db_update);
    void listAll();

    // Admin
    void update();

    // Playlists
    void listPlaylist();
    void listPlaylists();
    void load(QString name);
    void rename(const QString oldName, const QString newName);
    void rm(QString name);
    void save(QString name);

Q_SIGNALS:
    void currentSongUpdated(const Song &song);
    void mpdConnectionDied();
    void playlistUpdated(const QList<Song> &songs);
    void statsUpdated();
    void statusUpdated();
    void storedPlayListUpdated();
    void outputsUpdated(const QList<Output> &outputs);
    void musicLibraryUpdated(MusicLibraryItemRoot * root, QDateTime db_update);
    void dirViewUpdated(DirViewItemRoot * root);
    void playlistsRetrieved(const QList<Playlist> &data);
    void databaseUpdated();

private Q_SLOTS:
    void idleDataReady();
    void onSocketStateChanged(QAbstractSocket::SocketState socketState);
    void onIdleSocketStateChanged(QAbstractSocket::SocketState socketState);

private:
    bool connectToMPD(QTcpSocket &socket, bool enableIdle=false);
    Response sendCommand(const QByteArray &command);
    void initialize();
    void parseIdleReturn(const QByteArray &data);

private:
    QString hostname;
    quint16 port;
    QString password;
    // Use 2 sockets, 1 for commands and 1 to receive MPD idle events.
    // Cant use 1, as we could write a command just as an idle event is ready to read
    QTcpSocket sock;
    QTcpSocket idleSocket;
    QMutex mutex;
};

#endif
