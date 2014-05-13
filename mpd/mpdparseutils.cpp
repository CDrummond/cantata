/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "models/dirviewitemroot.h"
#include "models/dirviewitemdir.h"
#include "models/dirviewitemfile.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "models/musiclibraryitemsong.h"
#include "models/musiclibraryitemroot.h"
#include "mpdparseutils.h"
#include "mpdstatus.h"
#include "mpdstats.h"
#include "playlist.h"
#include "song.h"
#include "output.h"
#include "gui/covers.h"
#ifdef ENABLE_HTTP_SERVER
#include "http/httpserver.h"
#endif
#include "support/utils.h"
#include "cuefile.h"
#include "mpdconnection.h"
#ifdef ENABLE_ONLINE_SERVICES
#include "online/onlineservice.h"
#endif

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
static const QByteArray constComposerKey("Composer: ");
static const QByteArray constTitleKey("Title: ");
static const QByteArray constTrackKey("Track: ");
static const QByteArray constIdKey("Id: ");
static const QByteArray constDiscKey("Disc: ");
static const QByteArray constDateKey("Date: ");
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

static const QByteArray constOkValue("OK");
static const QByteArray constSetValue("1");
static const QByteArray constPlayValue("play");
static const QByteArray constStopValue("stop");

static const QString constProtocol=QLatin1String("://");
static const QString constHttpProtocol=QLatin1String("http://");

static inline bool toBool(const QByteArray &v) { return v==constSetValue; }

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

    foreach (const QByteArray &line, lines) {
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
            v.dbUpdate.setTime_t(line.mid(constStatsDbUpdateKey.length()).toUInt());
        }
    }
    return v;
}

MPDStatusValues MPDParseUtils::parseStatus(const QByteArray &data)
{
    MPDStatusValues v;
    QList<QByteArray> lines = data.split('\n');

    foreach (const QByteArray &line, lines) {
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

static QSet<QString> constStdProtocols=QSet<QString>() << QLatin1String("http://")
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
    foreach (const QByteArray &line, lines) {
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
            song.composer = QString::fromUtf8(line.mid(constComposerKey.length()));
        } else if (line.startsWith(constTitleKey)) {
            song.title =QString::fromUtf8(line.mid(constTitleKey.length()));
        } else if (line.startsWith(constTrackKey)) {
            song.track = line.mid(constTrackKey.length()).split('/').at(0).toInt();
        } else if (Loc_Library!=location && line.startsWith(constIdKey)) {
            song.id = line.mid(constIdKey.length()).toUInt();
        } else if (line.startsWith(constDiscKey)) {
            song.disc = line.mid(constDiscKey.length()).split('/').at(0).toInt();
        } else if (line.startsWith(constDateKey)) {
            QByteArray value=line.mid(constDateKey.length());
            if (value.length()>4) {
                song.year = value.left(4).toUInt();
            } else {
                song.year = value.toUInt();
            }
        } else if (line.startsWith(constGenreKey)) {
            song.addGenre(QString::fromUtf8(line.mid(constGenreKey.length())));
        }  else if (line.startsWith(constNameKey)) {
            song.name = QString::fromUtf8(line.mid(constNameKey.length()));
        } else if (line.startsWith(constPlaylistKey)) {
            song.file = QString::fromUtf8(line.mid(constPlaylistKey.length()));
            song.title=Utils::getFile(song.file);
            song.type=Song::Playlist;
        } else if (Loc_PlayQueue==location && line.startsWith(constPriorityKey)) {
            song.priority = line.mid(constPriorityKey.length()).toUInt();
        } else if (line.startsWith(constAlbumId)) {
            song.mbAlbumId = line.mid(constAlbumId.length());
        }
    }

    if (Song::Playlist!=song.type && song.genre.isEmpty()) {
        song.genre = Song::unknown();
    }

    if (Loc_Library==location) {
        song.guessTags();
        song.fillEmptyFields();
    } else if (Loc_Streams==location) {
        song.name=getAndRemoveStreamName(song.file);
    } else {
        QString origFile=song.file;

        #ifdef ENABLE_HTTP_SERVER
        if (!song.file.isEmpty() && song.file.startsWith(constHttpProtocol) && HttpServer::self()->isOurs(song.file)) {
            song.type=Song::CantataStream;
            Song mod=HttpServer::self()->decodeUrl(song.file);
            if (!mod.title.isEmpty()) {
                mod.id=song.id;
                song=mod;
            }
        } else
        #endif
            if (song.file.contains(Song::constCddaProtocol)) {
                song.type=Song::Cdda;
            } else if (song.file.contains(constProtocol)) {
                foreach (const QString &protocol, constStdProtocols) {
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
                    #ifdef ENABLE_ONLINE_SERVICES
                    if (!OnlineService::decode(song))
                    #endif
                    {
                        QString name=getAndRemoveStreamName(song.file);
                        if (!name.isEmpty()) {
                            song.name=name;
                        }
                        if (song.title.isEmpty() && song.name.isEmpty()) {
                            song.title=Utils::getFile(QUrl(song.file).path());
                        }
                    }
                }
            } else {
                song.guessTags();
                song.fillEmptyFields();
            }
        }
        // HTTP server, and OnlineServices, modify the path. But this then messes up
        // undo/restore of playqueue. Therefore, set path back to original value...
        song.file=origFile;
        song.setKey(location);
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

    foreach (const QByteArray &line, lines) {
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

static bool groupSingleTracks=false;

bool MPDParseUtils::groupSingle()
{
    return groupSingleTracks;
}

void MPDParseUtils::setGroupSingle(bool g)
{
    groupSingleTracks=g;
}

void MPDParseUtils::parseLibraryItems(const QByteArray &data, const QString &mpdDir, long mpdVersion,
                                      bool isMopidy, MusicLibraryItemRoot *rootItem, bool parsePlaylists,
                                      QSet<QString> *childDirs)
{
    bool canSplitCue=mpdVersion>=MPD_MAKE_VERSION(0,17,0);
    QList<QByteArray> currentItem;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;

    for (int i = 0; i < amountOfLines; i++) {
        const QByteArray &line=lines.at(i);
        if (childDirs && line.startsWith(constDirectoryKey)) {
            childDirs->insert(QString::fromUtf8(line.mid(constDirectoryKey.length())));
        }
        currentItem.append(line);
        if (i == amountOfLines - 1 || lines.at(i + 1).startsWith(constFileKey) || lines.at(i + 1).startsWith(constPlaylistKey)) {
            Song currentSong = parseSong(currentItem, Loc_Library);
            currentItem.clear();
            if (currentSong.file.isEmpty() || (isMopidy && !currentSong.file.startsWith(Song::constMopidyLocal))) {
                continue;
            }

            if (Song::Playlist==currentSong.type) {
                // lsinfo / will return all stored playlists - but this is deprecated.
                if (!parsePlaylists) {
                    continue;
                }

                MusicLibraryItemAlbum *prevAlbum=albumItem;
                QString prevSongFile=songItem ? songItem->file() : QString();
                QList<Song> cueSongs; // List of songs from cue file
                QSet<QString> cueFiles; // List of source (flac, mp3, etc) files referenced in cue file

                DBUG << "Got playlist item" << currentSong.file << "prevFile:" << prevSongFile;

                bool parseCue=canSplitCue && currentSong.isCueFile() && !mpdDir.startsWith("http://") && QFile::exists(mpdDir+currentSong.file);
                bool cueParseStatus=false;
                if (parseCue) {
                    DBUG << "Parsing cue file:" << currentSong.file << "mpdDir:" << mpdDir;
                    cueParseStatus=CueFile::parse(currentSong.file, mpdDir, cueSongs, cueFiles);
                    if (!cueParseStatus) {
                        DBUG << "Failed to parse cue file!";
                        continue;
                    } else DBUG << "Parsed cue file, songs:" << cueSongs.count() << "files:" << cueFiles;
                }
                if (cueParseStatus &&
                    (cueFiles.count()<cueSongs.count() || (albumItem && albumItem->data()==Song::unknown() && albumItem->parentItem()->data()==Song::unknown()))) {

                    bool canUseThisCueFile=true;
                    foreach (const Song &s, cueSongs) {
                        if (!QFile::exists(mpdDir+s.name)) {
                            DBUG << QString(mpdDir+s.name) << "is referenced in cue file, but does not exist in MPD folder";
                            canUseThisCueFile=false;
                            break;
                        }
                    }

                    if (!canUseThisCueFile) {
                        continue;
                    }

                    bool canUseCueFileTracks=false;
                    QList<Song> fixedCueSongs; // Songs taken from cueSongs that have been updated...

                    if (albumItem) {
                        QMap<QString, Song> origFiles=albumItem->getSongs(cueFiles);
                        DBUG << "Original files:" << origFiles.keys();
                        if (origFiles.size()==cueFiles.size()) {
                            // We have a previous album, if any of the details of the songs from the cue are empty,
                            // use those from the album...
                            bool setTimeFromSource=origFiles.size()==cueSongs.size();
                            quint32 albumTime=1==cueFiles.size() ? albumItem->totalTime() : 0;
                            quint32 usedAlbumTime=0;
                            foreach (const Song &orig, cueSongs) {
                                Song s=orig;
                                Song albumSong=origFiles[s.name];
                                s.name=QString(); // CueFile has placed source file name here!
                                if (s.artist.isEmpty() && !albumSong.artist.isEmpty()) {
                                    s.artist=albumSong.artist;
                                    DBUG << "Get artist from album" << albumSong.artist;
                                }
                                if (s.composer.isEmpty() && !albumSong.composer.isEmpty()) {
                                    s.composer=albumSong.composer;
                                    DBUG << "Get composer from album" << albumSong.composer;
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
                                } else if (0!=albumTime) {
                                    // Try to set duration of last track by subtracting previous track durations from album duration...
                                    if (0==s.time) {
                                        s.time=albumTime-usedAlbumTime;
                                    } else {
                                        usedAlbumTime+=s.time;
                                    }
                                }
                                fixedCueSongs.append(s);
                            }
                            canUseCueFileTracks=true;
                        } else DBUG << "ERROR: file count mismatch" << origFiles.size() << cueFiles.size();
                    } else DBUG << "ERROR: No album???";

                    if (!canUseCueFileTracks) {
                        // No revious album, or album had a different number of source files to the CUE file. If so, then we need to ensure
                        // all tracks have meta data - otherwise just fallback to listing file + cue
                        foreach (const Song &orig, cueSongs) {
                            Song s=orig;
                            s.name=QString(); // CueFile has placed source file name here!
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
                        QSet<MusicLibraryItemAlbum *> updatedAlbums;
                        updatedAlbums.insert(albumItem);
                        foreach (Song s, fixedCueSongs) {
                            s.fillEmptyFields();
                            if (!artistItem || s.albumArtist()!=artistItem->data()) {
                                artistItem = rootItem->artist(s);
                            }
                            if (!albumItem || s.year!=albumItem->year() || albumItem->parentItem()!=artistItem || s.album!=albumItem->data()) {
                                albumItem = artistItem->album(s);
                            }
                            DBUG << "Create new track from cue" << s.file << s.title << s.artist << s.albumartist << s.album;
                            songItem=new MusicLibraryItemSong(s, albumItem);
                            const QSet<QString> &songGenres=songItem->allGenres();
                            albumItem->append(songItem);
                            albumItem->addGenres(songGenres);
                            artistItem->addGenres(songGenres);
                            rootItem->addGenres(songGenres);
                            updatedAlbums.insert(albumItem);
                        }

                        // For each album that was updated/created, remove any source files referenced in cue file...
                        foreach (MusicLibraryItemAlbum *al, updatedAlbums) {
                            al->removeAll(cueFiles);
                            if (prevAlbum && al!=prevAlbum) {
                                DBUG << "Removing" << cueFiles.count() << " files from " << prevAlbum->data();
                                prevAlbum->removeAll(cueFiles);
                            }
                        }

                        // Remove any artist/album that was created and is now empty.
                        // This will happen if the source file (e.g. the flac file) does not have any metadata...
                        if (prevAlbum && 0==prevAlbum->childCount()) {
                            DBUG << "Removing empty previous album" << prevAlbum->data();
                            MusicLibraryItemArtist *ar=static_cast<MusicLibraryItemArtist *>(prevAlbum->parentItem());
                            ar->remove(prevAlbum);
                            if (0==ar->childCount()) {
                                rootItem->remove(ar);
                            }
                        }
                    }
                }

                // Add playlist file (or cue file) to current album, if it has the same path!
                // This assumes that MPD always send playlists as the last file...
                if (!prevSongFile.isEmpty() && Utils::getDir(prevSongFile)==Utils::getDir(currentSong.file)) {
                    currentSong.albumartist=currentSong.artist=artistItem->data();
                    currentSong.album=albumItem->data();
                    currentSong.time=albumItem->totalTime();
                    DBUG << "Adding playlist file to" << albumItem->parentItem()->data() << albumItem->data() << (void *)albumItem;
                    songItem = new MusicLibraryItemSong(currentSong, albumItem);
                    albumItem->append(songItem);
                }
                continue;
            }
//            if (currentSong.isEmpty()) {
//                continue;
//            }

            currentSong.fillEmptyFields();
            if (!artistItem || currentSong.artistOrComposer()!=artistItem->data()) {
                artistItem = rootItem->artist(currentSong);
                DBUG << "New artist item for " << currentSong.file << artistItem->data() << (void *)artistItem;
            }
            if (!albumItem || currentSong.year!=albumItem->year() || albumItem->parentItem()!=artistItem || currentSong.albumId()!=albumItem->albumId()) {
                albumItem = artistItem->album(currentSong);
                DBUG << "New album item for " << currentSong.file << artistItem->data() << albumItem->data() << (void *)albumItem;
            }           
            songItem=new MusicLibraryItemSong(currentSong, albumItem);
            const QSet<QString> &songGenres=songItem->allGenres();
            albumItem->append(songItem);
            albumItem->addGenres(songGenres);
            artistItem->addGenres(songGenres);
            rootItem->addGenres(songGenres);
        }/* else if (childDirs) {
        }*/
    }
}

DirViewItemRoot * MPDParseUtils::parseDirViewItems(const QByteArray &data, bool isMopidy)
{
    QList<QByteArray> lines = data.split('\n');
    DirViewItemRoot *rootItem = new DirViewItemRoot;
    DirViewItemDir *dir=rootItem;
    QString currentDirPath;

    foreach (const QByteArray &line, lines) {
        QString path;

        if (line.startsWith(constFileKey)) {
            path=QString::fromUtf8(line.mid(constFileKey.length()));
        } else if (line.startsWith(constPlaylistKey)) {
            path=QString::fromUtf8(line.mid(constPlaylistKey.length()));
        }
        if (!path.isEmpty() && (!isMopidy || path.startsWith(Song::constMopidyLocal))) {
            QString mopidyPath;
            if (isMopidy) {
                mopidyPath=path;
                path=Song::decodePath(path);
            }
            int last=path.lastIndexOf(Utils::constDirSep);
            QString dirPath=-1==last ? QString() : path.left(last);
            QStringList parts=path.split("/");

            if (dirPath!=currentDirPath) {
                currentDirPath=dirPath;
                dir=rootItem;
                if (parts.length()>1) {
                    for (int i=0; i<parts.length()-1; ++i) {
                        dir=dir->getDirectory(parts.at(i), true);
                    }
                }
            }
            dir->insertFile(parts.last(), mopidyPath);
        }
    }

    return rootItem;
}

QList<Output> MPDParseUtils::parseOuputs(const QByteArray &data)
{
    QList<Output> outputs;
    QList<QByteArray> lines = data.split('\n');
    Output output;

    foreach (const QByteArray &line, lines) {
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

static const QString constHashReplacement=QLatin1String("${hash}");

QString MPDParseUtils::addStreamName(const QString &url, const QString &name)
{
    if (name.isEmpty()) {
        return url;
    }

    QUrl u(url);
    QString n(name);
    while (n.contains("#")) {
        n=n.replace("#", constHashReplacement);
    }
    if (u.path().isEmpty()) {
        return url+"/#"+n;
    }
    return url+"#"+n;
}

QString MPDParseUtils::getStreamName(const QString &url)
{
    int idx=url.lastIndexOf('#');
    QString name=-1==idx ? QString() : url.mid(idx+1);
    while (name.contains(constHashReplacement)) {
        name.replace(constHashReplacement, "#");
    }
    return name;
}

QString MPDParseUtils::getAndRemoveStreamName(QString &url)
{
    int idx=url.lastIndexOf('#');
    if (-1==idx) {
        return QString();
    }
    QString name=url.mid(idx+1);
    while (name.contains(constHashReplacement)) {
        name.replace(constHashReplacement, "#");
    }
    url=url.left(idx);
    return name;
}
