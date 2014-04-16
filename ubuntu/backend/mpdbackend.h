/*
 * Cantata
 *
 * Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
 * Copyright (c) 2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "albumsmodel.h"
#include "musiclibrarymodel.h"
#include "playlistsmodel.h"
#include "playqueuemodel.h"
#include "playqueueproxymodel.h"
#include "musiclibraryproxymodel.h"
#include "albumsproxymodel.h"
#include "playlistsproxymodel.h"
#include "dirviewmodel.h"
#include "dirviewproxymodel.h"

class MPDBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ getIsConnected NOTIFY onConnectedChanged)
    Q_PROPERTY(bool isPlaying READ getIsPlaying NOTIFY onPlayingStatusChanged)
    Q_PROPERTY(bool isStopped READ getIsStopped NOTIFY onPlayingStatusChanged)
    Q_PROPERTY(bool isPaused READ getIsPaused NOTIFY onPlayingStatusChanged)
    Q_PROPERTY(int crossfade READ getCrossfade NOTIFY onPlayingStatusChanged)
    Q_PROPERTY(QStringList outputs READ getOutputs NOTIFY outputsChanged)
    Q_PROPERTY(QString replayGain READ getReplayGain NOTIFY replayGainChanged)

    //Information about current song
    Q_PROPERTY(QString currentSongMainText READ getCurrentSongMainText NOTIFY onCurrentSongChanged)
    Q_PROPERTY(QString currentSongSubText READ getCurrentSongSubText NOTIFY onCurrentSongChanged)
    Q_PROPERTY(quint32 currentSongPlayqueuePosition READ getCurrentSongPlayqueuePosition NOTIFY onCurrentSongChanged)

    Q_PROPERTY(bool isRandomOrder READ getIsRandomOrder WRITE setIsRandomOrder NOTIFY onMpdStatusChanged)
    Q_PROPERTY(quint8 mpdVolume READ getMpdVolume WRITE setMpdVolume NOTIFY onMpdStatusChanged)

    Q_PROPERTY(bool playQueueEmpty READ isPlayQueueEmpty NOTIFY onPlayQueueChanged)
    Q_PROPERTY(QString playQueueStatus READ getPlayQueueStatus NOTIFY onPlayQueueChanged)
    Q_PROPERTY(bool artistsFound READ getArtistsFound NOTIFY onArtistsModelChanged)
    Q_PROPERTY(bool albumsFound READ getAlbumsFound NOTIFY onAlbumsModelChanged)
    Q_PROPERTY(bool foldersFound READ getFoldersFound NOTIFY onFoldersModelChanged)
    Q_PROPERTY(bool playlistsFound READ getPlaylistsFound NOTIFY onPlaylistsModelChanged)

public:
    explicit MPDBackend(QObject *parent = 0);

    Q_INVOKABLE void connectTo(QString hostname, quint16 port, QString password, QString folder);
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void nextSong();
    Q_INVOKABLE void previousSong();
    Q_INVOKABLE void clearPlayQueue() { emit clear(); }
    Q_INVOKABLE bool getIsConnected() { return MPDConnection::self()->isConnected(); }
    Q_INVOKABLE void add(const QString &modelName, const QVariant &rows, bool replace);
    Q_INVOKABLE void loadPlaylist(int index, bool replace);
    Q_INVOKABLE void removeFromPlayQueue(int index);
    Q_INVOKABLE bool getIsPlaying() { return MPDStatus::self()->state() == MPDState_Playing; }
    Q_INVOKABLE bool getIsStopped() { return MPDStatus::self()->state() == MPDState_Stopped; }
    Q_INVOKABLE bool getIsPaused() { return MPDStatus::self()->state() == MPDState_Paused; }
    Q_INVOKABLE void startPlayingSongAtPos(int index);
    Q_INVOKABLE QString getCurrentSongMainText() { return mainText; }
    Q_INVOKABLE QString getCurrentSongSubText() { return subText; }
    Q_INVOKABLE quint32 getCurrentSongPlayqueuePosition() { return playQueueModel.currentSongRow(); }
    Q_INVOKABLE quint8 getMpdVolume() { return MPDStatus::self()->volume(); }
    Q_INVOKABLE QString getPlayQueueStatus() { return playQueueStatus; }
    Q_INVOKABLE void setMpdVolume(quint8 volume);
    Q_INVOKABLE bool isPlayQueueEmpty() { return playQueueModel.rowCount() == 0; }
    Q_INVOKABLE bool getArtistsFound() { return MusicLibraryModel::self()->rowCount()>0; }
    Q_INVOKABLE bool getAlbumsFound() { return AlbumsModel::self()->rowCount()>0; }
    Q_INVOKABLE bool getFoldersFound() { return DirViewModel::self()->rowCount()>0; }
    Q_INVOKABLE bool getPlaylistsFound() { return PlaylistsModel::self()->rowCount()>0; }
    Q_INVOKABLE bool getIsRandomOrder() { return MPDStatus::self()->random(); }
    Q_INVOKABLE void setIsRandomOrder(bool random);
    Q_INVOKABLE void getPlaybackSettings();
    Q_INVOKABLE void setPlaybackSettings(const QString &replayGain, int crossfade, const QStringList &outputs);
    Q_INVOKABLE int getCrossfade() { return MPDStatus::self()->crossFade(); }
    Q_INVOKABLE QStringList getOutputs () { return outputSettings; }
    Q_INVOKABLE QString getReplayGain () { return replayGainSetting; }

    PlayQueueProxyModel * getPlayQueueProxyModel() { return &playQueueProxyModel; }
    MusicLibraryProxyModel * getArtistsProxyModel() { return &artistsProxyModel; }
    AlbumsProxyModel * getAlbumsProxyModel() { return &albumsProxyModel; }
    DirViewProxyModel * getFoldersProxyModel() { return &foldersProxyModel; }
    PlaylistsProxyModel * getPlaylistsProxyModel() { return &playlistsProxyModel; }

Q_SIGNALS:
    void onConnectedChanged();
    void onPlayingStatusChanged();
    void onCurrentSongChanged();
    void onPlayQueueChanged();
    void onArtistsModelChanged();
    void onAlbumsModelChanged();
    void onFoldersModelChanged();
    void onPlaylistsModelChanged();
    void onMpdStatusChanged();
    void outputsChanged();
    void replayGainChanged();

public Q_SLOTS:
    void onConnected(bool connected);
    void updateCurrentSong(const Song &song);
    void updateStats();
    void updateStatus();
    void updatePlayQueue(const QList<Song> &songs);
    void mpdConnectionStateChanged(bool connected);
    void artistsUpdated();
    void albumsUpdated();
    void foldersUpdated();
    void playlistsUpdated();

private Q_SLOTS:
    void outputsUpdated(const QList<Output> &outputs);
    void replayGainUpdated(const QString &v);
    
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
    void loadFolders();
    void loadLibrary();
    void add(const QStringList &files, bool replace, quint8 priorty); // Songs
    void loadPlaylist(const QString &name, bool replace);
    void setVolume(int volume);
    void clear();
    void setRandomOrder(bool random);
    void getReplayGainSetting();
    void setReplayGain(const QString &);
    void setCrossFade(int secs);
    void getOutputSetting();
    void enableOutput(int id, bool);

private:
    void updateStatus(MPDStatus * const status);

private:
    MPDState lastState;
    qint32 lastSongId;
    enum { CS_Init, CS_Connected, CS_Disconnected } connectedState;
    bool stopAfterCurrent;

    Song current;
    QString mainText;
    QString subText;
    QString playQueueStatus;
    QStringList outputSettings;
    QString replayGainSetting;
    QTimer *statusTimer;
    PlayQueueModel playQueueModel;
    PlayQueueProxyModel playQueueProxyModel;
    MusicLibraryProxyModel artistsProxyModel;
    AlbumsProxyModel albumsProxyModel;
    DirViewProxyModel foldersProxyModel;
    PlaylistsProxyModel playlistsProxyModel;
};

#endif // MPDBACKEND_H
