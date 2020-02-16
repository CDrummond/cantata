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

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QFile>
#include "online/onlineservice.h"
#include "online/podcastservice.h"
#include "mpdparseutils.h"
#include "mpdstatus.h"
#include "mpdstats.h"
#include "playlist.h"
#include "song.h"
#include "output.h"
#ifdef ENABLE_HTTP_SERVER
#include "http/httpserver.h"
#endif
#include "support/utils.h"
#include "cuefile.h"
#include "mpdconnection.h"
#include <algorithm>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "MPDParseUtils"
void MPDParseUtils::enableDebug()
{
    debugEnabled=true;
}

static const QByteArray constTimeKey("Time: ");
static const QByteArray constAlbumKey("Album: ");
static const QByteArray constArtistKey("Artist: ");
static const QByteArray constAlbumArtistKey("AlbumArtist: ");
static const QByteArray constAlbumSortKey("AlbumSort: ");
static const QByteArray constArtistSortKey("ArtistSort: ");
static const QByteArray constAlbumArtistSortKey("AlbumArtistSort: ");
static const QByteArray constComposerKey("Composer: ");
static const QByteArray constPerformerKey("Performer: ");
static const QByteArray constCommentKey("Comment: ");
static const QByteArray constTitleKey("Title: ");
static const QByteArray constTrackKey("Track: ");
static const QByteArray constIdKey("Id: ");
static const QByteArray constDiscKey("Disc: ");
static const QByteArray constDateKey("Date: ");
static const QByteArray constOriginalDateKey("OriginalDate: ");
static const QByteArray constGenreKey("Genre: ");
static const QByteArray constNameKey("Name: ");
static const QByteArray constPriorityKey("Prio: ");
static const QByteArray constAlbumId("MUSICBRAINZ_ALBUMID: ");
static const QByteArray constFileKey("file: ");
static const QByteArray constPlaylistKey("playlist: ");
static const QByteArray constDirectoryKey("directory: ");
static const QByteArray constOutputIdKey("outputid: ");
static const QByteArray constOutputNameKey("outputname: ");
static const QByteArray constOutputEnabledKey("outputenabled: ");
static const QByteArray constChangePosKey("cpos");
static const QByteArray constChangeIdKey("Id");
static const QByteArray constLastModifiedKey("Last-Modified: ");
static const QByteArray constStatsArtistsKey("artists: ");
static const QByteArray constStatsAlbumsKey("albums: ");
static const QByteArray constStatsSongsKey("songs: ");
static const QByteArray constStatsUptimeKey("uptime: ");
static const QByteArray constStatsPlaytimeKey("playtime: ");
static const QByteArray constStatsDbPlaytimeKey("db_playtime: ");
static const QByteArray constStatsDbUpdateKey("db_update: ");

static const QByteArray constStatusVolumeKey("volume: ");
static const QByteArray constStatusConsumeKey("consume: ");
static const QByteArray constStatusRepeatKey("repeat: ");
static const QByteArray constStatusSingleKey("single: ");
static const QByteArray constStatusRandomKey("random: ");
static const QByteArray constStatusPlaylistKey("playlist: ");
static const QByteArray constStatusPlaylistLengthKey("playlistlength: ");
static const QByteArray constStatusCrossfadeKey("xfade: ");
static const QByteArray constStatusStateKey("state: ");
static const QByteArray constStatusSongKey("song: ");
static const QByteArray constStatusSongIdKey("songid: ");
static const QByteArray constStatusNextSongKey("nextsong: ");
static const QByteArray constStatusNextSongIdKey("nextsongid: ");
static const QByteArray constStatusTimeKey("time: ");
static const QByteArray constStatusBitrateKey("bitrate: ");
static const QByteArray constStatusAudioKey("audio: ");
static const QByteArray constStatusUpdatingDbKey("updating_db: ");
static const QByteArray constStatusErrorKey("error: ");
static const QByteArray constChannel("channel: ");
static const QByteArray constMessage("message: ");

static const QByteArray constOkValue("OK");
static const QByteArray constSetValue("1");
static const QByteArray constPlayValue("play");
static const QByteArray constStopValue("stop");

static const QString constProtocol=QLatin1String("://");
static const QString constHttpProtocol=QLatin1String("http://");

static inline bool toBool(const QByteArray &v) { return v==constSetValue; }

static QSet<QString> singleTracksFolders;
static MPDParseUtils::CueSupport cueSupport=MPDParseUtils::Cue_Parse;

MPDParseUtils::CueSupport MPDParseUtils::toCueSupport(const QString &str)
{
    for (int i=0; i<Cue_Count; ++i) {
        if (toStr((MPDParseUtils::CueSupport)i)==str) {
            return (MPDParseUtils::CueSupport)i;
        }
    }
    return MPDParseUtils::Cue_Parse;
}

QString MPDParseUtils::toStr(MPDParseUtils::CueSupport cs)
{
    switch (cs) {
    default:
    case MPDParseUtils::Cue_Parse:            return QLatin1String("parse");
    case MPDParseUtils::Cue_ListButDontParse: return QLatin1String("list");
    case MPDParseUtils::Cue_Ignore:           return QLatin1String("ignore");
    }
}

void MPDParseUtils::setCueFileSupport(CueSupport cs)
{
    cueSupport=cs;
}

MPDParseUtils::CueSupport MPDParseUtils::cueFileSupport()
{
    return cueSupport;
}

void MPDParseUtils::setSingleTracksFolders(const QSet<QString> &folders)
{
    singleTracksFolders=folders;
}

QList<Playlist> MPDParseUtils::parsePlaylists(const QByteArray &data)
{
    QList<Playlist> playlists;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        const QByteArray &line=lines.at(i);

        if (line.startsWith(constPlaylistKey)) {
            Playlist playlist;
            playlist.name = QString::fromUtf8(line.mid(constPlaylistKey.length()));
            i++;

            if (i < amountOfLines) {
                const QByteArray &line=lines.at(i);
                if (line.startsWith(constLastModifiedKey)) {
                    playlist.lastModified=QDateTime::fromString(QString::fromUtf8(line.mid(constLastModifiedKey.length())), Qt::ISODate);
                    playlists.append(playlist);
                }
            }
        }
    }

    return playlists;
}

MPDStatsValues MPDParseUtils::parseStats(const QByteArray &data)
{
    MPDStatsValues v;
    QList<QByteArray> lines = data.split('\n');

    for (const QByteArray &line: lines) {
        if (line.startsWith(constStatsArtistsKey)) {
            v.artists=line.mid(constStatsArtistsKey.length()).toUInt();
        } else if (line.startsWith(constStatsAlbumsKey)) {
            v.albums=line.mid(constStatsAlbumsKey.length()).toUInt();
        } else if (line.startsWith(constStatsSongsKey)) {
            v.songs=line.mid(constStatsSongsKey.length()).toUInt();
        } else if (line.startsWith(constStatsUptimeKey)) {
            v.uptime=line.mid(constStatsUptimeKey.length()).toUInt();
        } else if (line.startsWith(constStatsPlaytimeKey)) {
            v.playtime=line.mid(constStatsPlaytimeKey.length()).toUInt();
        } else if (line.startsWith(constStatsDbPlaytimeKey)) {
            v.dbPlaytime=line.mid(constStatsDbPlaytimeKey.length()).toUInt();
        } else if (line.startsWith(constStatsDbUpdateKey)) {
            v.dbUpdate=line.mid(constStatsDbUpdateKey.length()).toUInt();
        }
    }
    return v;
}

MPDStatusValues MPDParseUtils::parseStatus(const QByteArray &data)
{
    MPDStatusValues v;
    QList<QByteArray> lines = data.split('\n');

    for (const QByteArray &line: lines) {
        if (line.startsWith(constStatusVolumeKey)) {
            v.volume=line.mid(constStatusVolumeKey.length()).toInt();
        } else if (line.startsWith(constStatusConsumeKey)) {
            v.consume=toBool(line.mid(constStatusConsumeKey.length()));
        } else if (line.startsWith(constStatusRepeatKey)) {
            v.repeat=toBool(line.mid(constStatusRepeatKey.length()));
        } else if (line.startsWith(constStatusSingleKey)) {
            v.single=toBool(line.mid(constStatusSingleKey.length()));
        } else if (line.startsWith(constStatusRandomKey)) {
            v.random=toBool(line.mid(constStatusRandomKey.length()));
        } else if (line.startsWith(constStatusPlaylistKey)) {
            v.playlist=line.mid(constStatusPlaylistKey.length()).toUInt();
        } else if (line.startsWith(constStatusPlaylistLengthKey)) {
            v.playlistLength=line.mid(constStatusPlaylistLengthKey.length()).toInt();
        } else if (line.startsWith(constStatusCrossfadeKey)) {
            v.crossFade=line.mid(constStatusCrossfadeKey.length()).toInt();
        } else if (line.startsWith(constStatusStateKey)) {
            QByteArray value=line.mid(constStatusStateKey.length());
            if (constPlayValue==value) {
                v.state=MPDState_Playing;
            } else if (constStopValue==value) {
                v.state=MPDState_Stopped;
            } else {
                v.state=MPDState_Paused;
            }
        } else if (line.startsWith(constStatusSongKey)) {
            v.song=line.mid(constStatusSongKey.length()).toInt();
        } else if (line.startsWith(constStatusSongIdKey)) {
            v.songId=line.mid(constStatusSongIdKey.length()).toInt();
        } else if (line.startsWith(constStatusNextSongKey)) {
            v.nextSong=line.mid(constStatusNextSongKey.length()).toInt();
        } else if (line.startsWith(constStatusNextSongIdKey)) {
            v.nextSongId=line.mid(constStatusNextSongIdKey.length()).toInt();
        } else if (line.startsWith(constStatusTimeKey)) {
            QList<QByteArray> values=line.mid(constStatusTimeKey.length()).split(':');
            if (values.length()>1) {
                v.timeElapsed=values.at(0).toInt();
                v.timeTotal=values.at(1).toInt();
            }
        } else if (line.startsWith(constStatusBitrateKey)) {
            v.bitrate=line.mid(constStatusBitrateKey.length()).toUInt();
        } else if (line.startsWith(constStatusAudioKey)) {
            QList<QByteArray> values=line.mid(constStatusAudioKey.length()).split(':');
            if (3==values.length()) {
                v.samplerate=values.at(0).toUInt();
                v.bits=values.at(1).toUInt();
                v.channels=values.at(2).toUInt();
            }
        } else if (line.startsWith(constStatusUpdatingDbKey)) {
            v.updatingDb=line.mid(constStatusUpdatingDbKey.length()).toInt();
        } else if (line.startsWith(constStatusErrorKey)) {
            v.error=QString::fromUtf8(line.mid(constStatusErrorKey.length()));
            // If we are reporting a stream error, remove any stream name added by Cantata...
            int start=v.error.indexOf(constHttpProtocol);
            if (start>0) {
                int end=v.error.indexOf(QChar('\"'), start+6);
                int pos=v.error.indexOf(QChar('#'), start+6);
                if (pos>start && pos<end) {
                    v.error=v.error.left(pos)+v.error.mid(end);
                }
            }
        }
    }
    return v;
}

static QSet<QString> constStdProtocols=QSet<QString>() << constHttpProtocol
                                                       << QLatin1String("https://")
                                                       << QLatin1String("mms://")
                                                       << QLatin1String("mmsh://")
                                                       << QLatin1String("mmst://")
                                                       << QLatin1String("mmsu://")
                                                       << QLatin1String("gopher://")
                                                       << QLatin1String("rtp://")
                                                       << QLatin1String("rtsp://")
                                                       << QLatin1String("rtmp://")
                                                       << QLatin1String("rtmpt://")
                                                       << QLatin1String("rtmps://");

Song MPDParseUtils::parseSong(const QList<QByteArray> &lines, Location location)
{
    Song song;
    for (const QByteArray &line: lines) {
        if (line.startsWith(constFileKey)) {
            song.file = QString::fromUtf8(line.mid(constFileKey.length()));
        } else if (line.startsWith(constTimeKey) ){
            song.time = line.mid(constTimeKey.length()).toUInt();
        } else if (line.startsWith(constAlbumKey)) {
            song.album = QString::fromUtf8(line.mid(constAlbumKey.length()));
        } else if (line.startsWith(constArtistKey)) {
            song.artist = QString::fromUtf8(line.mid(constArtistKey.length()));
        } else if (line.startsWith(constAlbumArtistKey)) {
            song.albumartist = QString::fromUtf8(line.mid(constAlbumArtistKey.length()));
        } else if (line.startsWith(constComposerKey)) {
            song.setComposer(QString::fromUtf8(line.mid(constComposerKey.length())));
        } else if (line.startsWith(constTitleKey)) {
            song.title =QString::fromUtf8(line.mid(constTitleKey.length()));
        } else if (line.startsWith(constTrackKey)) {
            int v=line.mid(constTrackKey.length()).split('/').at(0).toInt();
            song.track = v<0 ? 0 : v;
        } else if (Loc_Library!=location && Loc_Search!=location && line.startsWith(constIdKey)) {
            song.id = line.mid(constIdKey.length()).toUInt();
        } else if (line.startsWith(constDiscKey)) {
            int v = line.mid(constDiscKey.length()).split('/').at(0).toInt();
            song.disc = v<0 ? 0 : v;
        } else if (line.startsWith(constDateKey)) {
            QByteArray value=line.mid(constDateKey.length());
            int v=value.length()>4 ? value.left(4).toUInt() : value.toUInt();
            song.year=v<0 ? 0 : v;
        } else if (line.startsWith(constOriginalDateKey)) {
            QByteArray value=line.mid(constOriginalDateKey.length());
            int v=value.length()>4 ? value.left(4).toUInt() : value.toUInt();
            song.origYear=v<0 ? 0 : v;
        } else if (line.startsWith(constGenreKey)) {
            song.addGenre(QString::fromUtf8(line.mid(constGenreKey.length())));
        } else if (line.startsWith(constNameKey)) {
            song.setName(QString::fromUtf8(line.mid(constNameKey.length())));
        } else if (line.startsWith(constPlaylistKey)) {
            song.file = QString::fromUtf8(line.mid(constPlaylistKey.length()));
            song.title=Utils::getFile(song.file);
            song.type=Song::Playlist;
        }  else if (line.startsWith(constAlbumId)) {
            song.setMbAlbumId(line.mid(constAlbumId.length()));
        } else if ((Loc_Search==location || Loc_Library==location) && line.startsWith(constLastModifiedKey)) {
            song.lastModified=QDateTime::fromString(QString::fromUtf8(line.mid(constLastModifiedKey.length())), Qt::ISODate).toTime_t();
        } else if ((Loc_Search==location || Loc_Playlists==location || Loc_PlayQueue==location) && line.startsWith(constPerformerKey)) {
            if (song.hasPerformer()) {
                song.setPerformer(song.performer()+QLatin1String(", ")+QString::fromUtf8(line.mid(constPerformerKey.length())));
            } else {
                song.setPerformer(QString::fromUtf8(line.mid(constPerformerKey.length())));
            }
        } else if (Loc_PlayQueue==location) {
            if (line.startsWith(constPriorityKey)) {
                song.priority = line.mid(constPriorityKey.length()).toUInt();
            } else if (line.startsWith(constCommentKey)) {
                song.setComment(QString::fromUtf8(line.mid(constCommentKey.length())));
            }
        } else if (Loc_Library==location) {
            if (line.startsWith(constAlbumSortKey)) {
                song.setAlbumSort(QString::fromUtf8(line.mid(constAlbumSortKey.length())));
            } else if (line.startsWith(constArtistSortKey)) {
                song.setArtistSort(QString::fromUtf8(line.mid(constArtistSortKey.length())));
            } else if (line.startsWith(constAlbumArtistSortKey)) {
                song.setAlbumArtistSort(QString::fromUtf8(line.mid(constAlbumArtistSortKey.length())));
            }
        }
    }

    if (Song::Playlist!=song.type && song.genres[0].isEmpty()) {
        song.addGenre(Song::unknown());
    }

    if (Loc_Library==location) {
        song.guessTags();
        song.fillEmptyFields();
    } else if (Loc_Streams==location) {
        song.setName(getAndRemoveStreamName(song.file, true));
    } else {
        QString origFile=song.file;
        bool modifiedFile=false;

        #ifdef ENABLE_HTTP_SERVER
        if (!song.file.isEmpty() && song.file.startsWith(constHttpProtocol) && HttpServer::self()->isOurs(song.file)) {
            song.type=Song::CantataStream;
            Song mod=HttpServer::self()->decodeUrl(song.file);
            mod.priority=song.priority;
            if (!mod.title.isEmpty()) {
                mod.id=song.id;
                song=mod;
                song.setLocalPath(mod.file);
            }
            modifiedFile=true;
        } else
        #endif
            if (song.file.contains(Song::constCddaProtocol)) {
                song.type=Song::Cdda;
            } else if (song.file.contains(constProtocol)) {
                for (const QString &protocol: constStdProtocols) {
                    if (song.file.startsWith(protocol)) {
                        song.type=Song::Stream;
                        break;
                    }
                }
            }

        if (!song.file.isEmpty()) {
            if (song.isStream()) {
                if (song.isCantataStream()) {
                    if (song.title.isEmpty()) {
                        QStringList parts=QUrl(song.file).path().split('/');
                        if (!parts.isEmpty()) {
                            song.title=parts.last();
                            song.fillEmptyFields();
                        }
                    }
                } else {
                    if (OnlineService::decode(song)) {
                        modifiedFile=true;
                    } else {
                        QString name=getAndRemoveStreamName(song.file);
                        if (!name.isEmpty()) {
                            song.setName(name);
                        }
                        if (song.title.isEmpty() && song.name().isEmpty()) {
                            song.title=Utils::getFile(QUrl(song.file).path());
                        }
                    }
                }
            } else if (Loc_PlayQueue==location && Song::Standard==song.type && !singleTracksFolders.isEmpty() && singleTracksFolders.contains(Utils::getDir(song.file, false))) {
                song.setFromSingleTracks();
                song.fillEmptyFields();
            } else {
                song.guessTags();
                song.fillEmptyFields();
            }
        }
        // HTTP server, and OnlineServices, modify the path. But this then messes up
        // undo/restore of playqueue. Therefore, set path back to original value...
        if (modifiedFile) {
            song.setDecodedPath(song.file);
        }
        song.file=origFile;
        song.setKey(location);

        // Check for downloaded podcasts played via local file playback
        if (Song::OnlineSvrTrack!=song.type && PodcastService::isPodcastFile(song.file)) {
            song.setIsFromOnlineService(PodcastService::constName);
            song.albumartist=song.artist=PodcastService::constName;
        }
    }
    return song;
}

QList<Song> MPDParseUtils::parseSongs(const QByteArray &data, Location location)
{
    QList<Song> songs;
    QList<QByteArray> currentItem;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        const QByteArray &line=lines.at(i);
        // Skip the "OK" line, this is NOT a song!!!
        if (constOkValue==line) {
            continue;
        }
        if (!line.isEmpty()) {
            currentItem.append(line);
        }
        if (i == lines.size() - 1 || lines.at(i + 1).startsWith(constFileKey)) {
            Song song=parseSong(currentItem, location);
            if (!song.file.isEmpty()) {
                songs.append(song);
            }
            currentItem.clear();
        }
    }

    return songs;
}

QList<MPDParseUtils::IdPos> MPDParseUtils::parseChanges(const QByteArray &data)
{
    QList<IdPos> changes;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    quint32 cpos=0;
    bool foundCpos=false;

    for (int i = 0; i < amountOfLines; i++) {
        const QByteArray &line = lines.at(i);
        // Skip the "OK" line, this is NOT a song!!!
        if (constOkValue==line || line.length()<1) {
            continue;
        }
        QList<QByteArray> tokens = line.split(':');
        if (2!=tokens.count()) {
            return QList<IdPos>();
        }
        QByteArray element = tokens.takeFirst();
        QByteArray value = tokens.takeFirst();
        if (constChangePosKey==element) {
            if (foundCpos) {
                return QList<IdPos>();
            }
            foundCpos=true;
            cpos = value.toInt();
        } else if (constChangeIdKey==element) {
            if (!foundCpos) {
                return QList<IdPos>();
            }
            foundCpos=false;
            qint32 id=value.toInt();
            changes.append(IdPos(id, cpos));
        }
    }

    return changes;
}

QStringList MPDParseUtils::parseList(const QByteArray &data, const QByteArray &key)
{
    QStringList items;
    QList<QByteArray> lines = data.split('\n');
    int keyLen=key.length();

    for (const QByteArray &line: lines) {
        // Skip the "OK" line, this is NOT a valid item!!!
        if (constOkValue==line) {
            continue;
        }
        if (line.startsWith(key)) {
            items.append(QString::fromUtf8(line.mid(keyLen).replace("://", "")));
        }
    }

    return items;
}

MPDParseUtils::MessageMap MPDParseUtils::parseMessages(const QByteArray &data)
{
    MPDParseUtils::MessageMap messages;
    QList<QByteArray> lines = data.split('\n');
    QByteArray channel;

    for (const QByteArray &line: lines) {
        // Skip the "OK" line, this is NOT a valid item!!!
        if (constOkValue==line) {
            continue;
        }
        if (line.startsWith(constChannel)) {
            channel=line.mid(constChannel.length());
        } else if (line.startsWith(constMessage)) {
            messages[channel].append(QString::fromUtf8(line.mid(constMessage.length())));
        }
    }
    return messages;
}

void MPDParseUtils::parseDirItems(const QByteArray &data, const QString &mpdDir, long mpdVersion, QList<Song> &songList, const QString &dir, QStringList &subDirs, Location loc)
{
    QList<QByteArray> currentItem;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    bool parsePlaylists="/"!=dir && ""!=dir;
    bool setSingleTracks=parsePlaylists && singleTracksFolders.contains(dir) && Loc_Browse!=loc;
    QList<Song> songs;

    for (int i = 0; i < amountOfLines; i++) {
        const QByteArray &line=lines.at(i);
        if (line.startsWith(constDirectoryKey)) {
            subDirs.append(QString::fromUtf8(line.mid(constDirectoryKey.length())));
        }
        currentItem.append(line);
        if (i == amountOfLines - 1 || lines.at(i + 1).startsWith(constFileKey) || lines.at(i + 1).startsWith(constPlaylistKey)) {
            Song currentSong = parseSong(currentItem, Loc_Library);
            currentItem.clear();
            if (currentSong.file.isEmpty()) {
                continue;
            }

            DBUG << currentSong.file;
            if (Song::Playlist==currentSong.type) {
                // lsinfo will return all stored playlists - but this is deprecated.
                if (!parsePlaylists) {
                    continue;
                }

                if (!currentSong.isCueFile()) {
                    // In Folders/Browse, we can list all playlists
                    if (Loc_Browse==loc) {
                        songs.append(currentSong);
                    }
                    // Only add CUE files to library listing...
                    continue;
                }

                switch (cueSupport) {
                case Cue_Ignore:
                    continue;
                    break;
                case Cue_Parse:
                    if (Loc_Browse==loc) {
                        songs.append(currentSong);
                    }
                    if (Loc_Library!=loc) {
                        continue;
                    }
                    break;
                case Cue_ListButDontParse:
                    if (Loc_Browse==loc) {
                        songs.append(currentSong);
                    }
                default:
                    continue;
                    break;
                }

                // No source files for CUE file..
                if (songs.isEmpty()) {
                    continue;
                }

                Song firstSong=songs.at(0);
                QList<Song> cueSongs; // List of songs from cue file
                QSet<QString> cueFiles; // List of source (flac, mp3, etc) files referenced in cue file

                DBUG << "Got playlist item" << currentSong.file;

                bool canSplitCue=mpdVersion>=CANTATA_MAKE_VERSION(0,17,0);
                bool parseCue=canSplitCue && currentSong.isCueFile() && !mpdDir.startsWith(constHttpProtocol) && QFile::exists(mpdDir+currentSong.file);
                bool cueParseStatus=false;
                double lastTrackIndex=0.0;
                if (parseCue) {
                    DBUG << "Parsing cue file:" << currentSong.file << "mpdDir:" << mpdDir;
                    cueParseStatus=CueFile::parse(currentSong.file, mpdDir, cueSongs, cueFiles, lastTrackIndex);
                    if (!cueParseStatus) {
                        DBUG << "Failed to parse cue file!";
                        continue;
                    } else DBUG << "Parsed cue file, songs:" << cueSongs.count() << "files:" << cueFiles;
                }
                if (cueParseStatus && cueSongs.count()>=songs.count() &&
                        (cueFiles.count()<cueSongs.count() || (firstSong.albumArtist().isEmpty() && firstSong.album.isEmpty()))) {

                    bool canUseThisCueFile=true;
                    for (const Song &s: cueSongs) {
                        if (!QFile::exists(mpdDir+s.name())) {
                            DBUG << QString(mpdDir+s.name()) << "is referenced in cue file, but does not exist in MPD folder";
                            canUseThisCueFile=false;
                            break;
                        }
                    }

                    if (!canUseThisCueFile) {
                        continue;
                    }

                    bool canUseCueFileTracks=false;
                    QList<Song> fixedCueSongs; // Songs taken from cueSongs that have been updated...

                    if (songs.size()==cueFiles.size()) {
                        quint32 albumTime=0;
                        QMap<QString, Song> origFiles;
                        for (const Song &s: songs) {
                            origFiles.insert(s.file, s);
                            albumTime+=s.time;
                        }
                        DBUG << "Original files:" << origFiles.keys();

                        bool setTimeFromSource=origFiles.size()==cueSongs.size();
                        DBUG << "setTimeFromSource" << setTimeFromSource << "at" << albumTime << "#c" << cueFiles.size();
                        for (const Song &orig: cueSongs) {
                            Song s=orig;
                            Song albumSong=origFiles[s.name()];
                            s.setName(QString()); // CueFile has placed source file name here!
                            if (s.artist.isEmpty() && !albumSong.artist.isEmpty()) {
                                s.artist=albumSong.artist;
                                DBUG << "Get artist from album" << albumSong.artist;
                            }
                            if (s.composer().isEmpty() && !albumSong.composer().isEmpty()) {
                                s.setComposer(albumSong.composer());
                                DBUG << "Get composer from album" << albumSong.composer();
                            }
                            if (s.album.isEmpty() && !albumSong.album.isEmpty()) {
                                s.album=albumSong.album;
                                DBUG << "Get album from album" << albumSong.album;
                            }
                            if (s.albumartist.isEmpty() && !albumSong.albumartist.isEmpty()) {
                                s.albumartist=albumSong.albumartist;
                                DBUG << "Get albumartist from album" << albumSong.albumartist;
                            }
                            if (0==s.year && 0!=albumSong.year) {
                                s.year=albumSong.year;
                                DBUG << "Get year from album" << albumSong.year;
                            }
                            if (0==s.time && setTimeFromSource) {
                                s.time=albumSong.time;
                            } else if (0==s.time && 1==cueFiles.size()) {
                                DBUG << "Set time of last track" << s.title << s.time << albumTime << (lastTrackIndex/1000.0);
                                // Try to set duration of last track by subtracting previous track durations from album duration...
                                s.time=albumTime-(lastTrackIndex/1000.0);
                            }
                            DBUG << s.title << s.time;
                            fixedCueSongs.append(s);
                        }
                        canUseCueFileTracks=true;
                    } else DBUG << "ERROR: file count mismatch" << songs.size() << cueFiles.size();

                    if (!canUseCueFileTracks) {
                        // Album had a different number of source files to the CUE file. If so, then we need to ensure
                        // all tracks have meta data - otherwise just fallback to listing file + cue
                        for (const Song &orig: cueSongs) {
                            Song s=orig;
                            s.setName(QString()); // CueFile has placed source file name here!
                            if (s.artist.isEmpty() || s.album.isEmpty()) {
                                break;
                            }
                            fixedCueSongs.append(s);
                        }

                        if (fixedCueSongs.count()==cueSongs.count()) {
                            canUseCueFileTracks=true;
                        } else DBUG << "ERROR: Not all cue tracks had meta data";
                    }

                    if (canUseCueFileTracks) {
                        songs = fixedCueSongs;
                    }
                    continue;
                }

                if (!firstSong.albumArtist().isEmpty() && !firstSong.album.isEmpty()) {
                    currentSong.albumartist=firstSong.albumArtist();
                    currentSong.album=firstSong.album;
                    songs.append(currentSong);
                }
            } else {
                if (setSingleTracks) {
                    currentSong.setFromSingleTracks();
                }
                currentSong.fillEmptyFields();
                songs.append(currentSong);
            }
        }
    }
    if (Loc_Browse==loc) {
        QList<Song> sngs;
        QList<Song> playlists;
        for (const auto &s: songs) {
            if (Song::Playlist==s.type) {
                playlists.append(s);
            } else {
                sngs.append(s);
            }
        }
        std::sort(playlists.begin(), playlists.end());
        songs=sngs;
        songs+=playlists;
    }
    songList+=songs;
}

QList<Output> MPDParseUtils::parseOuputs(const QByteArray &data)
{
    QList<Output> outputs;
    QList<QByteArray> lines = data.split('\n');
    Output output;

    for (const QByteArray &line: lines) {
        if (constOkValue==line) {
            break;
        }

        if (line.startsWith(constOutputIdKey)) {
            if (!output.name.isEmpty()) {
                outputs << output;
                output.name=QString();
            }
            output.id=line.mid(constOutputIdKey.length()).toUInt();
        } else if (line.startsWith(constOutputNameKey)) {
            output.name=line.mid(constOutputNameKey.length());
        } else if (line.startsWith(constOutputEnabledKey)) {
            output.enabled=toBool(line.mid(constOutputEnabledKey.length()));
        }
    }

    if (!output.name.isEmpty()) {
        outputs << output;
    }

    return outputs;
}

static const QByteArray constSticker("sticker: ");
QByteArray MPDParseUtils::parseSticker(const QByteArray &data, const QByteArray &sticker)
{
    QList<QByteArray> lines = data.split('\n');
    QByteArray key=constSticker+sticker+'=';
    for (const QByteArray &line: lines) {
        if (line.startsWith(key)) {
            return line.mid(key.length());
        }
    }
    return QByteArray();
}

QList<MPDParseUtils::Sticker> MPDParseUtils::parseStickers(const QByteArray &data, const QByteArray &sticker)
{
    QList<Sticker> stickers;
    QList<QByteArray> lines = data.split('\n');
    Sticker s;
    QByteArray key=constSticker+sticker+'=';

    for (const QByteArray &line: lines) {
        if (constOkValue==line) {
            break;
        }

        if (line.startsWith(constFileKey)) {
            s.file=line.mid(constFileKey.length());
        } else if (line.startsWith(key)) {
            s.value=line.mid(key.length());
            stickers.append(s);
        }
    }

    return stickers;
}

static const QString constStreamName("StreamName");

QString MPDParseUtils::addStreamName(const QString &url, const QString &name, bool singleHash)
{
    return Utils::addHashParam(url, singleHash ? QString() : constStreamName, name);
}

QString MPDParseUtils::getStreamName(const QString &url)
{
    DBUG << url;
    QMap<QString, QString> kv = Utils::hashParams(url);
    QMap<QString, QString>::ConstIterator name = kv.constFind(constStreamName);
    if (kv.constEnd()!=name) {
        DBUG << "name" << name.value();
        return name.value();
    }
    return QString();
}

QString MPDParseUtils::getAndRemoveStreamName(QString &url, bool checkSingleHash)
{
    DBUG << url;
    QMap<QString, QString> kv = Utils::hashParams(url);
    QMap<QString, QString>::ConstIterator name = kv.constFind(constStreamName);
    if (kv.constEnd()==name && checkSingleHash) {
        DBUG << "check single";
        name = kv.find("-");
    }
    if (kv.constEnd()==name) {
        DBUG << "no name found";
        return QString();
    }
    url=Utils::removeHash(url);
    DBUG << "name" << name.value();
    return name.value();
}
