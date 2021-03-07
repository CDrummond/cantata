/*
 * Cantata
 *
 * Copyright (c) 2021 David Hoyes <dphoyes@gmail.com>
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

#include "macnowplaying.h"

#include "mpd-interface/mpdconnection.h"
#include "gui/currentcover.h"
#include "gui/stdactions.h"

#include <QPointer>
#import <AppKit/AppKit.h>
#import <MediaPlayer/MediaPlayer.h>

struct MacNowPlaying::impl
{
    QPointer<MPDStatus> status;
    QString currentCover;
    Song currentSong;
    NSMutableDictionary *nowPlayingInfo;

    impl()
        : nowPlayingInfo([[NSMutableDictionary alloc] init])
    {}
};

MacNowPlaying::MacNowPlaying(QObject *p)
    : QObject(p)
    , pimpl(new impl())
{
    connect(this, SIGNAL(setSeekId(qint32, quint32)), MPDConnection::self(), SLOT(setSeekId(qint32, quint32)));
    connect(CurrentCover::self(), SIGNAL(coverFile(const QString &)), this, SLOT(updateCurrentCover(const QString &)));

    MPRemoteCommandCenter *commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

    [commandCenter.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull) {
        if (!pimpl->status.isNull() && MPDState_Playing==pimpl->status->state()) {
            StdActions::self()->playPauseTrackAction->trigger();
        }
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [commandCenter.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull) {
        if (!pimpl->status.isNull() && pimpl->status->playlistLength()>0 && MPDState_Playing!=pimpl->status->state()) {
            StdActions::self()->playPauseTrackAction->trigger();
        }
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [commandCenter.stopCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull) {
        StdActions::self()->stopPlaybackAction->trigger();
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [commandCenter.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull) {
        StdActions::self()->playPauseTrackAction->trigger();
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [commandCenter.nextTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull) {
        StdActions::self()->nextTrackAction->trigger();
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [commandCenter.previousTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull) {
        StdActions::self()->prevTrackAction->trigger();
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    [commandCenter.changePlaybackPositionCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull event) {
        MPChangePlaybackPositionCommandEvent* seekEvent = static_cast<MPChangePlaybackPositionCommandEvent *>(event);
        emit setSeekId(-1, seekEvent.positionTime);
        return MPRemoteCommandHandlerStatusSuccess;
    }];

    commandCenter.changeRepeatModeCommand.enabled = NO;
    commandCenter.changeShuffleModeCommand.enabled = NO;
    commandCenter.changePlaybackRateCommand.enabled = NO;
    commandCenter.seekBackwardCommand.enabled = NO;
    commandCenter.seekForwardCommand.enabled = NO;
    commandCenter.skipBackwardCommand.enabled = NO;
    commandCenter.skipForwardCommand.enabled = NO;
    commandCenter.ratingCommand.enabled = NO;
    commandCenter.likeCommand.enabled = NO;
    commandCenter.dislikeCommand.enabled = NO;
    commandCenter.bookmarkCommand.enabled = NO;
    commandCenter.enableLanguageOptionCommand.enabled = NO;
    commandCenter.disableLanguageOptionCommand.enabled = NO;
}

MacNowPlaying::~MacNowPlaying()
{}

void MacNowPlaying::updateStatus(MPDStatus * const status)
{
    // If the current song has not yet been updated, reject this status
    // update and wait for the next unless the play queue has recently
    // been emptied or the connection to MPD has been lost ...
    if (status->songId()!=pimpl->currentSong.id && status->songId()!=-1) {
        return;
    }

    if (status!=pimpl->status) {
        pimpl->status = status;
    }

    MPNowPlayingInfoCenter *infoCenter = [MPNowPlayingInfoCenter defaultCenter];

    switch (status->state()) {
        case MPDState_Playing: {
            infoCenter.playbackState = MPNowPlayingPlaybackStatePlaying;
            [pimpl->nowPlayingInfo setObject:@(1) forKey:MPNowPlayingInfoPropertyPlaybackRate];
            break;
        }
        case MPDState_Paused: {
            infoCenter.playbackState = MPNowPlayingPlaybackStatePaused;
            [pimpl->nowPlayingInfo setObject:@(0) forKey:MPNowPlayingInfoPropertyPlaybackRate];
            break;
        }
        default:
        case MPDState_Stopped: {
            infoCenter.playbackState = MPNowPlayingPlaybackStateStopped;
            [pimpl->nowPlayingInfo setObject:@(0) forKey:MPNowPlayingInfoPropertyPlaybackRate];
            break;
        }
    }

    [pimpl->nowPlayingInfo setObject:@(status->guessedElapsed()) forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
    [pimpl->nowPlayingInfo setObject:@(status->timeTotal()) forKey:MPMediaItemPropertyPlaybackDuration];
    infoCenter.nowPlayingInfo = pimpl->nowPlayingInfo;
}

void MacNowPlaying::updateCurrentCover(const QString &fileName)
{
    if (fileName!=pimpl->currentCover) {
        pimpl->currentCover=fileName;
        NSImage *image = [[NSImage alloc] initByReferencingFile:pimpl->currentCover.toNSString()];
        MPMediaItemArtwork *artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:CGSizeMake(1000, 1000) requestHandler:^NSImage * _Nonnull(CGSize) {
            return image;
        }];
        [pimpl->nowPlayingInfo setObject:artwork forKey:MPMediaItemPropertyArtwork];
        [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = pimpl->nowPlayingInfo;
    }
}

void MacNowPlaying::updateCurrentSong(const Song &song)
{
    pimpl->currentSong = song;

    [pimpl->nowPlayingInfo setObject:@(song.id) forKey:MPMediaItemPropertyPersistentID];
    [pimpl->nowPlayingInfo setObject:@(MPMediaTypeAnyAudio) forKey:MPMediaItemPropertyMediaType];
    [pimpl->nowPlayingInfo setObject:song.title.toNSString() forKey:MPMediaItemPropertyTitle];
    [pimpl->nowPlayingInfo setObject:song.artist.toNSString() forKey:MPMediaItemPropertyArtist];
    [pimpl->nowPlayingInfo setObject:song.album.toNSString() forKey:MPMediaItemPropertyAlbumTitle];

    [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo = pimpl->nowPlayingInfo;
}

#include "moc_macnowplaying.cpp"
