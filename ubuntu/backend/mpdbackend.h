/*
 * Cantata
 *
 * Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
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

#ifndef MPDBACKEND_H
#define MPDBACKEND_H

#include <QObject>
#include <QString>
#include <QTimer>

#include "mpdconnection.h"
#include "playqueuemodel.h"
#include "playqueueproxymodel.h"
#include "albumsproxymodel.h"

class MPDBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ getIsConnected NOTIFY onConnectedChanged)
    Q_PROPERTY(bool isPlaying READ getIsPlaying NOTIFY onPlayingStatusChanged)
    Q_PROPERTY(bool isStopped READ getIsStopped NOTIFY onPlayingStatusChanged)
    Q_PROPERTY(bool isPaused READ getIsPaused NOTIFY onPlayingStatusChanged)


    //Information about current song
    Q_PROPERTY(QString currentSongMainText READ getCurrentSongMainText NOTIFY onCurrentSongChanged)
    Q_PROPERTY(QString currentSongSubText READ getCurrentSongSubText NOTIFY onCurrentSongChanged)
    Q_PROPERTY(quint32 currentSongPlayqueuePosition READ getCurrentSongPlayqueuePosition NOTIFY onCurrentSongChanged)

    Q_PROPERTY(quint8 mpdVolume READ getMpdVolume WRITE setMpdVolume NOTIFY onMpdVolumeChanged)
    Q_PROPERTY(bool playQueueEmpty READ isPlayQueueEmpty NOTIFY onPlayQueueChanged)
    Q_PROPERTY(bool albumsFound READ getAlbumsFound NOTIFY onAlbumsModelChanged)

public:
    explicit MPDBackend(QObject *parent = 0);

    Q_INVOKABLE void connectTo(QString hostname, quint16 port, QString password);
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void nextSong();
    Q_INVOKABLE void previousSong();
    Q_INVOKABLE bool getIsConnected() { return MPDConnection::self()->isConnected(); }
    Q_INVOKABLE void addAlbum(int index, bool replace);
    Q_INVOKABLE void removeFromPlayQueue(int index);
    Q_INVOKABLE bool getIsPlaying() { return MPDStatus::self()->state() == MPDState_Playing; }
    Q_INVOKABLE bool getIsStopped() { return MPDStatus::self()->state() == MPDState_Stopped; }
    Q_INVOKABLE bool getIsPaused() { return MPDStatus::self()->state() == MPDState_Paused; }
    Q_INVOKABLE void startPlayingSongAtPos(int index);
    Q_INVOKABLE QString getCurrentSongMainText() { return mainText; }
    Q_INVOKABLE QString getCurrentSongSubText() { return subText; }
    Q_INVOKABLE quint32 getCurrentSongPlayqueuePosition() { return playQueueModel.currentSongRow(); }
    Q_INVOKABLE quint8 getMpdVolume() { return MPDStatus::self()->volume(); }
    Q_INVOKABLE void setMpdVolume(quint8 volume);
    Q_INVOKABLE bool isPlayQueueEmpty() { return playQueueModel.rowCount() == 0; }
    Q_INVOKABLE bool getAlbumsFound() { return AlbumsModel::self()->rowCount() != 0; }


signals:
    void onConnectedChanged();
    void onPlayingStatusChanged();
    void onCurrentSongChanged();
    void onMpdVolumeChanged();
    void onPlayQueueChanged();
    void onAlbumsModelChanged();

public Q_SLOTS:
    void onConnected(bool connected);
    void updateCurrentSong(const Song &song);
    void updateStats();
    void updateStatus();
    void updatePlayQueue(const QList<Song> &songs);
    void mpdConnectionStateChanged(bool connected);
    void albumsUpdated();

private:
    MPDState lastState;
    qint32 lastSongId;
    enum { CS_Init, CS_Connected, CS_Disconnected } connectedState;
    bool stopAfterCurrent;

    Song current;
    QString mainText;
    QString subText;
    QTimer *statusTimer;

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void setDetails(const MPDConnectionDetails &det);
    void pause(bool p);
    void play();
    void stop(bool afterCurrent=false);
    void getStatus();
    void playListInfo();
    void currentSong();
    void startPlayingSongId(qint32);
    void goToNextSong();
    void goToPreviousSong();
    void updateLibrary();
    void add(const QStringList &files, bool replace, quint8 priorty); //Album
    void setVolume(int volume);

private:
    void updateStatus(MPDStatus * const status);

public: // WTF???
    PlayQueueModel playQueueModel;
    PlayQueueProxyModel playQueueProxyModel;
    AlbumsProxyModel albumsProxyModel;
};

#endif // MPDBACKEND_H
