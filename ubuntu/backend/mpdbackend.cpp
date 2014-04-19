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

#include "mpdbackend.h"
#include "mpdconnection.h"
#include "currentcover.h"
#include "covers.h"
#include "localize.h"
#include "plurals.h"
#include <QString>
#include <QModelIndex>

MPDBackend::MPDBackend(QObject *parent) : QObject(parent)
    , lastState(MPDState_Inactive)
    , lastSongId(-1)
    , statusTimer(0)
    , connectedState(CS_Init)
    , stopAfterCurrent(false)
{
    connect(this, SIGNAL(goToNextSong()), MPDConnection::self(), SLOT(goToNext()));
    connect(this, SIGNAL(goToPreviousSong()), MPDConnection::self(), SLOT(goToPrevious()));
    connect(this, SIGNAL(loadLibrary()), MPDConnection::self(), SLOT(loadLibrary()));
    connect(this, SIGNAL(loadFolders()), MPDConnection::self(), SLOT(loadFolders()));

    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(onConnected(bool)));

    // TODO: These need to be confiurable via QML UI!
    AlbumsModel::self()->setEnabled(true);
    DirViewModel::self()->setEnabled(true);
    PlaylistsModel::self()->setEnabled(true);

    playQueueProxyModel.setSourceModel(&playQueueModel);
    artistsProxyModel.setSourceModel(MusicLibraryModel::self());
    albumsProxyModel.setSourceModel(AlbumsModel::self());
    foldersProxyModel.setSourceModel(DirViewModel::self());
    playlistsProxyModel.setSourceModel(PlaylistsModel::self());


//    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
//    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(pause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(play()), MPDConnection::self(), SLOT(play()));
    connect(this, SIGNAL(stop(bool)), MPDConnection::self(), SLOT(stopPlaying(bool)));
    connect(this, SIGNAL(getStatus()), MPDConnection::self(), SLOT(getStatus()));
    connect(this, SIGNAL(playListInfo()), MPDConnection::self(), SLOT(playListInfo()));
    connect(this, SIGNAL(currentSong()), MPDConnection::self(), SLOT(currentSong()));
//    connect(this, SIGNAL(setSeekId(qint32, quint32)), MPDConnection::self(), SLOT(setSeekId(qint32, quint32)));
    connect(this, SIGNAL(startPlayingSongId(qint32)), MPDConnection::self(), SLOT(startPlayingSongId(qint32)));
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
    connect(this, SIGNAL(getOutputSetting()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(setReplayGain(const QString &)), MPDConnection::self(), SLOT(setReplayGain(const QString &)));
    connect(this, SIGNAL(setCrossFade(int)), MPDConnection::self(), SLOT(setCrossFade(int)));
    connect(this, SIGNAL(getReplayGainSetting()), MPDConnection::self(), SLOT(getReplayGain()));

//    connect(this, SIGNAL(setPriority(const QList<qint32> &, quint8 )), MPDConnection::self(), SLOT(setPriority(const QList<qint32> &, quint8)));
//    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));

//    connect(&playQueueModel, SIGNAL(statsUpdated(int, quint32)), this, SLOT(updatePlayQueueStats(int, quint32))); //Just UI
    connect(&playQueueModel, SIGNAL(updateCurrent(Song)), SLOT(updateCurrentSong(Song)));

    connect(MPDStats::self(), SIGNAL(updated()), this, SLOT(updateStats()));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));

    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlayQueue(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(replayGain(QString)), SLOT(replayGainUpdated(QString)));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), SLOT(outputsUpdated(const QList<Output> &)));

//    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
//    connect(MPDConnection::self(), SIGNAL(info(const QString &)), SLOT(showInformation(const QString &)));

//    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));

//    connect(context, SIGNAL(playSong(QString)), &playQueueModel, SLOT(playSong(QString)));

//    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
//    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));



    connect(this, SIGNAL(add(const QStringList &, bool, quint8)), MPDConnection::self(), SLOT(add(const QStringList &, bool, quint8)));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(this, SIGNAL(setRandomOrder(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(this, SIGNAL(loadPlaylist(const QString &, bool)), MPDConnection::self(), SLOT(loadPlaylist(const QString &, bool)));
    connect(this, SIGNAL(removePlaylist(const QString &)), MPDConnection::self(), SLOT(removePlaylist(const QString &)));
    connect(this, SIGNAL(renamePlaylist(const QString &, const QString &)), MPDConnection::self(), SLOT(renamePlaylist(const QString &, const QString &)));
    connect(this, SIGNAL(removeFromPlaylist(const QString &, const QList<quint32> &)), MPDConnection::self(), SLOT(removeFromPlaylist(const QString &, const QList<quint32> &)));


    connect(MusicLibraryModel::self(), SIGNAL(updated()), this, SLOT(artistsUpdated()));
    connect(AlbumsModel::self(), SIGNAL(updated()), this, SLOT(albumsUpdated()));
    connect(DirViewModel::self(), SIGNAL(updated()), this, SLOT(foldersUpdated()));
    connect(PlaylistsModel::self(), SIGNAL(updated()), this, SLOT(playlistsUpdated()));

    MPDConnection::self()->start();
}

void MPDBackend::connectTo(QString hostname, quint16 port, QString password, QString folder) {
    MPDConnectionDetails details;
    details.hostname = hostname;
    details.port = port;
    details.password = password;
    details.dir = folder.isEmpty() ? folder : Utils::fixPath(folder);
    emit setDetails(details);
    MusicLibraryModel::self()->clear();
    //emit loadLibrary();
}

void MPDBackend::enableAlbumsView(bool en) {
    AlbumsModel::self()->setEnabled(en);
}

void MPDBackend::enableFoldersView(bool en) {
    DirViewModel::self()->setEnabled(en);
}

void MPDBackend::enablePlaylistsView(bool en) {
    PlaylistsModel::self()->setEnabled(en);
}

void MPDBackend::setArtistSortYear(bool s) {
    MusicLibraryItemAlbum::setSortByDate(s);
}

void MPDBackend::setAlbumViewSort(int s) {
    AlbumsModel::self()->setAlbumSort(s);
}

void MPDBackend::setCoverFetch(bool s) {
    Covers::self()->setFetchCovers(s);
}
    
void MPDBackend::playPause() {
    switch (MPDStatus::self()->state()) {
    case MPDState_Playing:
        emit pause(true);
        break;
    case MPDState_Paused:
//        stopVolumeFade();
        emit pause(false);
        break;
    default:
        if (playQueueModel.rowCount()>0) {
//            stopVolumeFade();
            if (-1!=playQueueModel.currentSong() && -1!=playQueueModel.currentSongRow()) {
                emit startPlayingSongId(playQueueModel.currentSong());
            } else {
                emit play();
            }
        }
    }
}

void MPDBackend::nextSong() {
    emit goToNextSong();
}

void MPDBackend::previousSong() {
    emit goToPreviousSong();
}

void MPDBackend::startPlayingSongAtPos(int index) {
    emit startPlayingSongId(playQueueModel.getIdByRow(index));
}

void MPDBackend::onConnected(bool connected) {
    emit onConnectedChanged();
}

void MPDBackend::setMpdVolume(quint8 volume) {
    emit setVolume(volume);
}

void MPDBackend::setIsRandomOrder(bool random) {
    emit setRandomOrder(random);
}

void MPDBackend::getPlaybackSettings() {
    emit getOutputSetting();
    emit getReplayGainSetting();
}

void MPDBackend::setPlaybackSettings(const QString &replayGain, int crossfade, const QStringList &outputs) {
    if (replayGain!=replayGainSetting) {
        replayGainSetting=replayGain;
        emit setReplayGain(replayGainSetting);
    }

    if (MPDStatus::self()->crossFade()!=crossfade) {
        emit setCrossFade(crossfade);
    }

    if (outputs!=outputSettings) {
        outputSettings=outputs;
        foreach (const QString &o, outputs) {
            QStringList parts=o.split(':');
            if (parts.size()>2) {
                bool enabled=parts.at(parts.size()-1)=="1";
                int id=parts.at(parts.size()-2).toInt();
                emit enableOutput(id, enabled);
            }
        }
    }
}

void MPDBackend::outputsUpdated(const QList<Output> &outputs) {
    QStringList setting;

    foreach (const Output &o, outputs) {
        setting.append(o.name+":"+o.id+":"+(o.enabled ? 1 : 0));
    }

    if (setting!=outputSettings) {
        outputSettings=setting;
        emit outputsChanged();
    }
}

void MPDBackend::replayGainUpdated(const QString &v) {
    if (v!=replayGainSetting) {
        replayGainSetting=v;
        emit replayGainChanged();
    }
}

// TODO: Why are the sorts required below? e.g. If we don't call sort, the items are only sorted string-wise. This
// means that 'Various Artists' is not placed at the top, and "The XXX" is not sorted as "XXX"
void MPDBackend::artistsUpdated() {
    artistsProxyModel.sort();
    emit onArtistsModelChanged();
}

void MPDBackend::albumsUpdated() {
    albumsProxyModel.sort();
    emit onAlbumsModelChanged();
}

void MPDBackend::foldersUpdated() {
    foldersProxyModel.sort();
    emit onFoldersModelChanged();
}

void MPDBackend::playlistsUpdated() {
    playlistsProxyModel.sort();
    emit onPlaylistsModelChanged();
}

void MPDBackend::add(const QString &modelName, const QVariant &rows, bool replace) {
    if (QVariant::Int==rows.type() && "playlists"==modelName) {
        loadPlaylist(rows.toInt(), replace);
        return;
    }

    ProxyModel *proxy=0;
    ActionModel *model=0;
    if ("artists"==modelName) {
        proxy=&artistsProxyModel;
        model=MusicLibraryModel::self();
    } else if ("albums"==modelName) {
        proxy=&albumsProxyModel;
        model=AlbumsModel::self();
    } else if ("playlists"==modelName) {
        proxy=&playlistsProxyModel;
        model=PlaylistsModel::self();
    } else if ("folders"==modelName) {
        proxy=&foldersProxyModel;
        model=DirViewModel::self();
    }

    if (model) {
        QList<int> rowList;
        if (QVariant::Int==rows.type()) {
            rowList.append(rows.toInt());
        } else if (QVariant::List==rows.type()) {
            foreach (const QVariant &v, rows.toList()) {
                rowList.append(v.toInt());
            }
        }
        if (rowList.count()) {
            QModelIndex idx;
            foreach (int r, rowList) {
                idx=proxy->index(r, 0, idx);
                if (!idx.isValid()) {
                    return;
                }
            }
            QStringList fileNames = model->filenames(QModelIndexList() << proxy->mapToSource(idx), false);
            if (!fileNames.isEmpty()) {
                emit add(fileNames, replace, 0);
            }
        }
    }
}

void MPDBackend::remove(const QString &modelName, const QVariant &rows) {
    if ("playlists"!=modelName) {
        return;
    }
    
    if (QVariant::Int==rows.type()) {
        QModelIndex index=playlistsProxyModel.index(rows.toInt(), 0);
        if (index.isValid()) {
            index=playlistsProxyModel.mapToSource(index);
            if (index.isValid() && static_cast<PlaylistsModel::Item *>(index.internalPointer())->isPlaylist()) {
                emit removePlaylist(static_cast<PlaylistsModel::PlaylistItem*>(index.internalPointer())->name);
            }
        }
    } else if (QVariant::List==rows.type()) {
        QVariantList rowList=rows.toList();
        if (2==rowList.count()) {
            QModelIndex playListIndex=playlistsProxyModel.index(rowList.at(0).toInt(), 0);
            if (playListIndex.isValid()) {
                QModelIndex songIndex=playlistsProxyModel.index(rowList.at(1).toInt(), 0, playListIndex);
                if (songIndex.isValid()) {
                    playListIndex=playlistsProxyModel.mapToSource(playListIndex);
                    songIndex=playlistsProxyModel.mapToSource(songIndex);
                    if (playListIndex.isValid() && songIndex.isValid() &&
                        static_cast<PlaylistsModel::Item *>(playListIndex.internalPointer())->isPlaylist() &&
                        !static_cast<PlaylistsModel::Item *>(songIndex.internalPointer())->isPlaylist()) {
                        emit removeFromPlaylist(static_cast<PlaylistsModel::PlaylistItem*>(playListIndex.internalPointer())->name,
                                                QList<quint32>() << songIndex.row());
                    }
                }
            }
        }
    }
}

void MPDBackend::loadPlaylist(int index, bool replace) {
    if (index<0) {
        return;
    }
    QModelIndex idx=playlistsProxyModel.mapToSource(playlistsProxyModel.index(index, 0));
    if (idx.isValid()) {
        PlaylistsModel::Item *i=static_cast<PlaylistsModel::Item *>(idx.internalPointer());
        if (i->isPlaylist()) {
            emit loadPlaylist(static_cast<PlaylistsModel::PlaylistItem*>(i)->name, replace);
        }
    }
}

void MPDBackend::removeFromPlayQueue(int index) {
    QList<int> removeIndexList;
    removeIndexList << index;
    playQueueModel.remove(removeIndexList);
}

//Methods ported from MainWindow class:

void MPDBackend::updateCurrentSong(const Song &song)
{
//    if (fadeWhenStop() && StopState_None!=stopState) {
//        if (StopState_Stopping==stopState) {
//            emit stop();
//        }
//    }

    current=song;
    if (current.isCdda()) {
        emit getStatus();
        if (current.isUnknown()) {
            Song pqSong=playQueueModel.getSongById(current.id);
            if (!pqSong.isEmpty()) {
                current=pqSong;
            }
        }
    }

//    if (current.isCantataStream()) {
//        Song mod=HttpServer::self()->decodeUrl(current.file);
//        if (!mod.title.isEmpty()) {
//            current=mod;
//            current.id=song.id;
//        }
//    }

    if (current.isStream() && !current.isCantataStream() && !current.isCdda()) {
        mainText=current.name.isEmpty() ? Song::unknown() : current.name;
        if (current.artist.isEmpty() && current.title.isEmpty() && !current.name.isEmpty()) {
            subText=i18n("(Stream)");
        } else {
            subText=current.artist.isEmpty() ? current.title : (current.artist+QLatin1String(" - ")+current.title);
        }
    } else {
        if (current.title.isEmpty() && current.artist.isEmpty() && (!current.name.isEmpty() || !current.file.isEmpty())) {
            mainText=current.name.isEmpty() ? current.file : current.name;
        } else {
            mainText=current.title;
        }
        if (current.album.isEmpty() && current.artist.isEmpty()) {
            subText=mainText.isEmpty() ? QString() : Song::unknown();
        } else if (current.album.isEmpty()) {
            subText=current.artist;
        } else {
            subText=current.artist+QLatin1String(" - ")+current.displayAlbum();
        }
    }

//    #ifdef QT_QTDBUS_FOUND
//    mpris->updateCurrentSong(current);
//    #endif
    if (current.time<5 && MPDStatus::self()->songId()==current.id && MPDStatus::self()->timeTotal()>5) {
        current.time=MPDStatus::self()->timeTotal();
    }
//    positionSlider->setEnabled(-1!=current.id && !current.isCdda() && (!currentIsStream() || current.time>5));

// TODO: Touch is not currently displaying covers - and 'current cover' is only required for playqueue or notifications.
//       If update is called, this will cause a cove to be downloaded, and saved!
//    CurrentCover::self()->update(current);

//    trackLabel->update(current);
//    bool isPlaying=MPDState_Playing==MPDStatus::self()->state();
    playQueueModel.updateCurrentSong(current.id);
//    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
//    playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && isPlaying);
//    scrollPlayQueue();
//    context->update(current);
//    trayItem->songChanged(song, isPlaying);


    //Ubuntu specific:

    emit onCurrentSongChanged();
}

void MPDBackend::updateStats() //Does nothing right now...
{
    // Check if remote db is more recent than local one
    if (!MusicLibraryModel::self()->lastUpdate().isValid() || MPDStats::self()->dbUpdate() > MusicLibraryModel::self()->lastUpdate()) {
        if (!MusicLibraryModel::self()->lastUpdate().isValid()) {
            MusicLibraryModel::self()->clear();
//            libraryPage->clear();
//            //albumsPage->clear();
//            folderPage->clear();
//            playlistsPage->clear();
        }
        if (!MusicLibraryModel::self()->fromXML()) {
            emit loadLibrary();
        }
        if (DirViewModel::self()->isEnabled() && !DirViewModel::self()->fromXML()) {
            emit loadFolders();
        }
//        albumsPage->goTop();
//        libraryPage->refresh();
//        folderPage->refresh();
    }
}

void MPDBackend::updateStatus()
{
    updateStatus(MPDStatus::self());
}

void MPDBackend::updateStatus(MPDStatus * const status)
{
//    if (!status->error().isEmpty()) {
//        showError(i18n("MPD reported the following error: %1", status->error()));
//    }

    if (MPDState_Stopped==status->state() || MPDState_Inactive==status->state()) {
//        positionSlider->clearTimes();
        playQueueModel.clearStopAfterTrack();
        if (statusTimer) {
            statusTimer->stop();
            statusTimer->setProperty("count", 0);
        }
    } else {
//        positionSlider->setRange(0, 0==status->timeTotal() && 0!=current.time && (current.isCdda() || current.isCantataStream())
//                                    ? current.time : status->timeTotal());
//        positionSlider->setValue(status->timeElapsed());
        if (0==status->timeTotal() && 0==status->timeElapsed()) {
            if (!statusTimer) {
                statusTimer=new QTimer(this);
                statusTimer->setSingleShot(true);
                connect(statusTimer, SIGNAL(timeout()), SIGNAL(getStatus()));
            }
            QVariant id=statusTimer->property("id");
            if (!id.isValid() || id.toInt()!=current.id) {
                statusTimer->setProperty("id", current.id);
                statusTimer->setProperty("count", 0);
                statusTimer->start(250);
            } else if (statusTimer->property("count").toInt()<12) {
                statusTimer->setProperty("count", statusTimer->property("count").toInt()+1);
                statusTimer->start(250);
            }
//        } else if (!positionSlider->isEnabled()) {
//            positionSlider->setEnabled(-1!=current.id && !current.isCdda() && (!currentIsStream() || status->timeTotal()>5));
        }
    }

//    randomPlayQueueAction->setChecked(status->random());
//    repeatPlayQueueAction->setChecked(status->repeat());
//    singlePlayQueueAction->setChecked(status->single());
//    consumePlayQueueAction->setChecked(status->consume());
//    updateNextTrack(status->nextSongId());

//    if (status->timeElapsed()<172800 && (!currentIsStream() || (status->timeTotal()>0 && status->timeElapsed()<=status->timeTotal()))) {
//        if (status->state() == MPDState_Stopped || status->state() == MPDState_Inactive) {
//            positionSlider->setRange(0, 0);
//        } else {
//            positionSlider->setValue(status->timeElapsed());
//        }
//    }

    playQueueModel.setState(status->state());
    switch (status->state()) {
    case MPDState_Playing:
//        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPauseIcon);
//        StdActions::self()->playPauseTrackAction->setEnabled(0!=status->playlistLength());
//        if (StopState_Stopping!=stopState) {
//            enableStopActions(true);
//            StdActions::self()->nextTrackAction->setEnabled(status->playlistLength()>1);
//            StdActions::self()->prevTrackAction->setEnabled(status->playlistLength()>1);
//        }
//        positionSlider->startTimer();
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
//        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
//        StdActions::self()->playPauseTrackAction->setEnabled(0!=status->playlistLength());
//        enableStopActions(false);
//        StdActions::self()->nextTrackAction->setEnabled(false);
//        StdActions::self()->prevTrackAction->setEnabled(false);
//        if (!StdActions::self()->playPauseTrackAction->isEnabled()) {
            current=Song();
//            trackLabel->update(current);
            CurrentCover::self()->update(current);
//        }
        current.id=0;
//        trayItem->setToolTip("cantata", i18n("Cantata"), QLatin1String("<i>")+i18n("Playback stopped")+QLatin1String("</i>"));
//        positionSlider->stopTimer();
        break;
    case MPDState_Paused:
//        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
//        StdActions::self()->playPauseTrackAction->setEnabled(0!=status->playlistLength());
//        enableStopActions(0!=status->playlistLength());
//        StdActions::self()->nextTrackAction->setEnabled(status->playlistLength()>1);
//        StdActions::self()->prevTrackAction->setEnabled(status->playlistLength()>1);
//        positionSlider->stopTimer();
    default:
        break;
    }

    // Check if song has changed or we're playing again after being stopped, and update song info if needed
    if (MPDState_Inactive!=status->state() &&
        (MPDState_Inactive==lastState || (MPDState_Stopped==lastState && MPDState_Playing==status->state()) || lastSongId != status->songId())) {
        emit currentSong();
    }
//    if (status->state()!=lastState && (MPDState_Playing==status->state() || MPDState_Stopped==status->state())) {
//        startContextTimer();
//    }

    // Update status info
    lastState = status->state();
    lastSongId = status->songId();


    //Touch specific:

    emit onPlayingStatusChanged();
    emit onMpdStatusChanged();
}

void MPDBackend::updatePlayQueue(const QList<Song> &songs)
{
//    StdActions::self()->playPauseTrackAction->setEnabled(!songs.isEmpty());
//    StdActions::self()->nextTrackAction->setEnabled(StdActions::self()->stopPlaybackAction->isEnabled() && songs.count()>1);
//    StdActions::self()->prevTrackAction->setEnabled(StdActions::self()->stopPlaybackAction->isEnabled() && songs.count()>1);
//    StdActions::self()->savePlayQueueAction->setEnabled(!songs.isEmpty());
//    promptClearPlayQueueAction->setEnabled(!songs.isEmpty());

//    int topRow=-1;
//    QModelIndex topIndex=playQueueModel.lastCommandWasUnodOrRedo() ? playQueue->indexAt(QPoint(0, 0)) : QModelIndex();
//    if (topIndex.isValid()) {
//        topRow=playQueueProxyModel.mapToSource(topIndex).row();
//    }
//    bool wasEmpty=0==playQueueModel.rowCount();
    playQueueModel.update(songs);
//    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
//    bool scroll=autoScrollPlayQueue && playQueueProxyModel.isEmpty() && wasEmpty && MPDState_Playing==MPDStatus::self()->state();
//    playQueue->updateRows(idx.row(), current.key, scroll);
//    if (!scroll && topRow>0 && topRow<playQueueModel.rowCount()) {
//        playQueue->scrollTo(playQueueProxyModel.mapFromSource(playQueueModel.index(topRow, 0)), QAbstractItemView::PositionAtTop);
//    }

    if (songs.isEmpty()) {
        updateCurrentSong(Song());
    } else if (current.isStandardStream()) {
        // Check to see if it has been updated...
        Song pqSong=playQueueModel.getSongByRow(playQueueModel.currentSongRow());
        if (pqSong.artist!=current.artist || pqSong.album!=current.album || pqSong.name!=current.name || pqSong.title!=current.title || pqSong.year!=current.year) {
            updateCurrentSong(pqSong);
        }
    }
//    playQueueItemsSelected(playQueue->haveSelectedItems());
//    updateNextTrack(MPDStatus::self()->nextSongId());

    //Touch specific:

    if (0==songs.count()) {
        playQueueStatus=QString();
    } else if (0==playQueueModel.totalTime()) {
        playQueueStatus=Plurals::tracks(songs.count());
    } else {
       playQueueStatus=Plurals::tracksWithDuration(songs.count(), Utils::formatDuration(playQueueModel.totalTime()));
    }

    emit onPlayQueueChanged();
}

void MPDBackend::mpdConnectionStateChanged(bool connected)
{
//    serverInfoAction->setEnabled(connected && !MPDConnection::self()->isMopdidy());
//    refreshDbAction->setEnabled(connected);
//    addStreamToPlayQueueAction->setEnabled(connected);
    if (connected) {
//        messageWidget->hide();
        if (CS_Connected!=connectedState) {
            emit playListInfo();
//            emit outputs();
//            if (CS_Init!=connectedState) {
//                currentTabChanged(tabWidget->currentIndex());
//            }
            connectedState=CS_Connected;
//            StdActions::self()->addWithPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
//            setPriorityAction->setVisible(StdActions::self()->addWithPriorityAction->isVisible());
        }
//        updateWindowTitle();
    } else {
//        libraryPage->clear();
//        albumsPage->clear();
//        folderPage->clear();
//        playlistsPage->clear();
        playQueueModel.clear();
        MusicLibraryModel::self()->clear();
        //AlbumsModel::self()->clear(); //Ubuntu specific
//        searchPage->clear();
        connectedState=CS_Disconnected;
//        outputsAction->setVisible(false);
        MPDStatus dummyStatus;
        updateStatus(&dummyStatus);
    }
//    controlPlaylistActions();

    //Ubuntu specific:

    emit onCurrentSongChanged();
}
