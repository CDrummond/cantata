/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "localize.h"
#include "dirviewitemroot.h"
#include "dirviewitemdir.h"
#include "dirviewitemfile.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "mpdparseutils.h"
#include "mpdstatus.h"
#include "mpdstats.h"
#include "playlist.h"
#include "song.h"
#include "output.h"
#include "covers.h"
#ifdef TAGLIB_FOUND
#include "httpserver.h"
#endif
#include "utils.h"
#include "cuefile.h"
#include "mpdconnection.h"
#include "soundcloudservice.h"

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << "MPDParseUtils"
void MPDParseUtils::enableDebug()
{
    debugEnabled=true;
}

QList<Playlist> MPDParseUtils::parsePlaylists(const QByteArray &data)
{
    QList<Playlist> playlists;
    QList<QByteArray> lines = data.split('\n');

    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        QList<QByteArray> tokens = lines.at(i).split(':');

        if (tokens.at(0) == "playlist") {
            Playlist playlist;
            playlist.name = QString::fromUtf8(tokens.at(1)).simplified();
            i++;
            tokens = lines.at(i).split(':');

            if (tokens.at(0) == "Last-Modified") {
                playlist.lastModified=QDateTime::fromString(tokens.at(1).trimmed()+':'+tokens.at(2)+':'+tokens.at(3), Qt::ISODate);
                playlists.append(playlist);
            }
        }
    }

    return playlists;
}

MPDStatsValues MPDParseUtils::parseStats(const QByteArray &data)
{
    MPDStatsValues v;
    QList<QByteArray> lines = data.split('\n');
    QList<QByteArray> tokens;
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        tokens = lines.at(i).split(':');

        if (tokens.at(0) == "artists") {
            v.artists=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "albums") {
            v.albums=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "songs") {
            v.songs=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "uptime") {
            v.uptime=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "playtime") {
            v.playtime=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "db_playtime") {
            v.dbPlaytime=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "db_update") {
            v.dbUpdate.setTime_t(tokens.at(1).toUInt());
        }
    }
    return v;
}

MPDStatusValues MPDParseUtils::parseStatus(const QByteArray &data)
{
    MPDStatusValues v;
    QList<QByteArray> lines = data.split('\n');
    QList<QByteArray> tokens;
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        tokens = lines.at(i).split(':');

        if (tokens.at(0) == "volume") {
            v.volume=tokens.at(1).toInt();
        } else if (tokens.at(0) == "consume") {
            v.consume=tokens.at(1).trimmed() == "1";
        } else if (tokens.at(0) == "repeat") {
            v.repeat=tokens.at(1).trimmed() == "1";
        } else if (tokens.at(0) == "single") {
            v.single=tokens.at(1).trimmed() == "1";
        } else if (tokens.at(0) == "random") {
            v.random=tokens.at(1).trimmed() == "1";
        } else if (tokens.at(0) == "playlist") {
            v.playlist=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "playlistlength") {
            v.playlistLength=tokens.at(1).toInt();
        } else if (tokens.at(0) == "xfade") {
            v.crossFade=tokens.at(1).toInt();
        } else if (tokens.at(0) == "state") {
            if (tokens.at(1).contains("play")) {
                v.state=MPDState_Playing;
            } else if (tokens.at(1).contains("stop")) {
                v.state=MPDState_Stopped;
            } else {
                v.state=MPDState_Paused;
            }
        } else if (tokens.at(0) == "song") {
            v.song=tokens.at(1).toInt();
        } else if (tokens.at(0) == "songid") {
            v.songId=tokens.at(1).toInt();
        } else if (tokens.at(0) == "nextsong") {
            v.nextSong=tokens.at(1).toInt();
        } else if (tokens.at(0) == "nextsongid") {
            v.nextSongId=tokens.at(1).toInt();
        } else if (tokens.at(0) == "time") {
            v.timeElapsed=tokens.at(1).toInt();
            v.timeTotal=tokens.at(2).toInt();
        } else if (tokens.at(0) == "bitrate") {
            v.bitrate=tokens.at(1).toUInt();
        } else if (tokens.at(0) == "audio") {
        } else if (tokens.at(0) == "updating_db") {
            v.updatingDb=tokens.at(1).toInt();
        } else if (tokens.at(0) == "error") {
            v.error=tokens.at(1);
        }
    }
    return v;
}

Song MPDParseUtils::parseSong(const QByteArray &data, bool isPlayQueue)
{
    Song song;
    QString tmpData = QString::fromUtf8(data.constData());
    QStringList lines = tmpData.split('\n');
    QStringList tokens;
    QString element;
    QString value;
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        tokens = lines.at(i).split(':');
        element = tokens.takeFirst();
        value = tokens.join(":");
        value = value.trimmed();
        if (element == QLatin1String("file")) {
            song.file = value;
        } else if (element == QLatin1String("Time") ){
            song.time = value.toUInt();
        } else if (element == QLatin1String("Album")) {
            song.album = value;
        } else if (element == QLatin1String("Artist")) {
            song.artist = value;
        } else if (element == QLatin1String("AlbumArtist")) {
            song.albumartist = value;
        } else if (element == QLatin1String("Title")) {
            song.title = value;
        } else if (element == QLatin1String("Track")) {
            song.track = value.split("/").at(0).toInt();
//         } else if (element == QLatin1String("Pos")) {
//             song.pos = value.toInt();
        } else if (element == QLatin1String("Id")) {
            song.id = value.toUInt();
        } else if (element == QLatin1String("Disc")) {
            song.disc = value.split("/").at(0).toUInt();
        } else if (element == QLatin1String("Date")) {
            if (value.length()>4) {
                song.year = value.left(4).toUInt();
            } else {
                song.year = value.toUInt();
            }
        } else if (element == QLatin1String("Genre")) {
            song.genre = value;
        }  else if (element == QLatin1String("Name")) {
            song.name = value;
        } else if (element == QLatin1String("playlist")) {
            song.file = value;
            song.title=Utils::getFile(song.file);
            song.type=Song::Playlist;
        }  else if (element == QLatin1String("Prio")) {
            song.priority = value.toUInt();
        }
    }

    if (Song::Playlist!=song.type && song.genre.isEmpty()) {
        song.genre = i18n("Unknown");
    }

    #ifdef TAGLIB_FOUND
    if (!song.file.isEmpty() && song.file.startsWith("http") && HttpServer::self()->isOurs(song.file)) {
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
    } else if (song.file.contains("://")) {
        song.type=Song::Stream;
    }

    if (!song.file.isEmpty()) {
        if (song.isStream()) {
            if (!song.isCantataStream()) {
                if (!SoundCloudService::decode(song)) {
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
    if (isPlayQueue) {
        song.setKey();
    }
    return song;
}

QList<Song> MPDParseUtils::parseSongs(const QByteArray &data)
{
    QList<Song> songs;
    QByteArray line;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        line += lines.at(i);
        // Skip the "OK" line, this is NOT a song!!!
        if ("OK"==line) {
            continue;
        }
        line += "\n";
        if (i == lines.size() - 1 || lines.at(i + 1).startsWith("file:")) {
            Song song=parseSong(line, true);
            songs.append(song);
            line.clear();
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
        QByteArray line = lines.at(i);
        // Skip the "OK" line, this is NOT a song!!!
        if ("OK"==line || line.length()<1) {
            continue;
        }
        QList<QByteArray> tokens = line.split(':');
        if (2!=tokens.count()) {
            return QList<IdPos>();
        }
        QByteArray element = tokens.takeFirst();
        QByteArray value = tokens.takeFirst();
        if (element == "cpos") {
            if (foundCpos) {
                return QList<IdPos>();
            }
            foundCpos=true;
            cpos = value.toInt();
        } else if (element == "Id") {
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

QStringList MPDParseUtils::parseUrlHandlers(const QByteArray &data)
{
    QStringList urls;
    QByteArray song;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        QString url(lines.at(i));
        // Skip the "OK" line, this is NOT a url handler!!!
        if (QLatin1String("OK")==url) {
            continue;
        }
        if (url.startsWith(QLatin1String("handler: "))) {
            urls.append(url.mid(9).replace("://", ""));
        }
    }

    return urls;
}

static bool groupSingleTracks=false;
static bool groupMultipleArtists=false;

bool MPDParseUtils::groupSingle()
{
    return groupSingleTracks;
}

void MPDParseUtils::setGroupSingle(bool g)
{
    groupSingleTracks=g;
}

bool MPDParseUtils::groupMultiple()
{
    return groupMultipleArtists;
}

void MPDParseUtils::setGroupMultiple(bool g)
{
    groupMultipleArtists=g;
}

MusicLibraryItemRoot * MPDParseUtils::parseLibraryItems(const QByteArray &data, const QString &mpdDir, long mpdVersion)
{
    bool canSplitCue=mpdVersion>=MPD_MAKE_VERSION(0,17,0);
    MusicLibraryItemRoot * const rootItem = new MusicLibraryItemRoot;
    QByteArray currentItem;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;
    QString unknown=i18n("Unknown");
    QString lastGenre;

    for (int i = 0; i < amountOfLines; i++) {
        currentItem += lines.at(i);
        currentItem += "\n";
        if (i == lines.size() - 1 || lines.at(i + 1).startsWith("file:") || lines.at(i + 1).startsWith("playlist:")) {
            Song currentSong = parseSong(currentItem, false);
            currentItem.clear();
            if (currentSong.file.isEmpty()) {
                continue;
            }

            if (Song::Playlist==currentSong.type) {
                MusicLibraryItemAlbum *prevAlbum=albumItem;
                QString prevSongFile=songItem ? songItem->file() : QString();
                QList<Song> cueSongs; // List of songs from cue file
                QSet<QString> cueFiles; // List of source (flac, mp3, etc) files referenced in cue file

                DBUG << "Got playlist item" << currentSong.file << "prevFile:" << prevSongFile;
                if (canSplitCue && currentSong.file.endsWith(".cue", Qt::CaseInsensitive) && CueFile::parse(currentSong.file, mpdDir, cueSongs, cueFiles) &&
                        (cueFiles.count()<cueSongs.count() || (albumItem && albumItem->data()==unknown && albumItem->parentItem()->data()==unknown))) {
                    DBUG << "Parsed file, songs:" << cueSongs.count() << "files:" << cueFiles;
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
                                if (s.artist.isEmpty()) {
                                    s.artist=albumSong.artist;
                                    DBUG << "Get artist from album" << albumSong.artist;
                                }
                                if (s.album.isEmpty()) {
                                    s.album=albumSong.album;
                                    DBUG << "Get album from album" << albumSong.album;
                                }
                                if (s.albumartist.isEmpty()) {
                                    s.albumartist=albumSong.albumartist;
                                    DBUG << "Get albumartist from album" << albumSong.album;
                                }
                                if (0==s.year) {
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
                        foreach (Song s, fixedCueSongs) {
                            s.fillEmptyFields();
                            if (!artistItem || s.albumArtist()!=artistItem->data()) {
                                artistItem = rootItem->artist(s);
                            }
                            if (!albumItem || s.year!=albumItem->year() || albumItem->parentItem()!=artistItem || s.album!=albumItem->data()) {
                                albumItem = artistItem->album(s);
                            }
                            DBUG << "Create new track from cue" << s.file << s.title << s.artist << s.albumartist << s.album;
                            songItem = new MusicLibraryItemSong(s, albumItem);
                            albumItem->append(songItem);
                            updatedAlbums.insert(albumItem);

                            if (s.genre!=lastGenre) {
                                lastGenre=s.genre;
                                albumItem->addGenre(s.genre);
                                artistItem->addGenre(s.genre);
                                rootItem->addGenre(s.genre);
                            }
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
                    DBUG << "Adding playlist file to" << albumItem->parentItem()->data() << albumItem->data();
                    songItem = new MusicLibraryItemSong(currentSong, albumItem);
                    albumItem->append(songItem);
                }
                continue;
            }
//            if (currentSong.isEmpty()) {
//                continue;
//            }

            currentSong.fillEmptyFields();
            if (!artistItem || currentSong.albumArtist()!=artistItem->data()) {
                artistItem = rootItem->artist(currentSong);
            }
            if (!albumItem || currentSong.year!=albumItem->year() || albumItem->parentItem()!=artistItem || currentSong.album!=albumItem->data()) {
                albumItem = artistItem->album(currentSong);
            }
            songItem = new MusicLibraryItemSong(currentSong, albumItem);
            albumItem->append(songItem);
            if (currentSong.genre!=lastGenre) {
                lastGenre=currentSong.genre;
                albumItem->addGenre(currentSong.genre);
                artistItem->addGenre(currentSong.genre);
                rootItem->addGenre(currentSong.genre);
            }
        }
    }

    if (groupSingleTracks) {
        rootItem->groupSingleTracks();
    }
    if (groupMultipleArtists) {
        rootItem->groupMultipleArtists();
    }

    return rootItem;
}

DirViewItemRoot * MPDParseUtils::parseDirViewItems(const QByteArray &data)
{
    QList<QByteArray> lines = data.split('\n');
    DirViewItemRoot * rootItem = new DirViewItemRoot;
    DirViewItem * currentDir = rootItem;
    QStringList currentDirList;

    int amountOfLines = lines.size();
    for (int i = 0; i < amountOfLines; i++) {
        QString line = QString::fromUtf8(lines.at(i));

        if (line.startsWith("file: ")) {
            line.remove(0, 6);
            QStringList parts = line.split("/");
            static_cast<DirViewItemDir *>(currentDir)->insertFile(parts.at(parts.size() - 1));
        } else if (line.startsWith("playlist: ")) {
            line.remove(0, 10);
            QStringList parts = line.split("/");
            static_cast<DirViewItemDir *>(currentDir)->insertFile(parts.at(parts.size() - 1));
        } else if (line.startsWith("directory: ")) {
            line.remove(0, 11);
            QStringList parts = line.split("/");

            /* Check how much matches */
            int depth = 0;
            for (int j = 0; j < currentDirList.size() && j < parts.size(); j++) {
                if (currentDirList.at(j) != parts.at(j)) {
                    break;
                }
                depth++;
            }

            for (int j = currentDirList.size(); j > depth; j--) {
                currentDir = currentDir->parent();
            }

            currentDir = static_cast<DirViewItemDir *>(currentDir)->createDirectory(parts.at(parts.size() - 1));
            currentDirList = parts;
        }
    }

    return rootItem;
}

QList<Output> MPDParseUtils::parseOuputs(const QByteArray &data)
{
    QList<Output> outputs;
    QList<QByteArray> lines = data.split('\n');

    for (int i = 0; i < lines.size();) {
        if (lines.at(i) == "OK") {
            break;
        }

        quint8 id = lines.at(i).mid(10).toInt();
        QString name = QString(lines.at(i + 1).mid(12));
        bool enabled = lines.at(i + 2).mid(15).toInt() == 0 ? false : true;

        outputs << Output(id, enabled, name);

        i += 3;
    }

    return outputs;
}

QString MPDParseUtils::formatDuration(const quint32 totalseconds)
{
    //Get the days,hours,minutes and seconds out of the total seconds
    quint32 days = totalseconds / 86400;
    quint32 rest = totalseconds - (days * 86400);
    quint32 hours = rest / 3600;
    rest = rest - (hours * 3600);
    quint32 minutes = rest / 60;
    quint32 seconds = rest - (minutes * 60);

    //Convert hour,minutes and seconds to a QTime for easier parsing
    QTime time(hours, minutes, seconds);

    #ifdef ENABLE_KDE_SUPPORT
    return 0==days
            ? time.toString("h:mm:ss")
            : i18np("1 day %2", "%1 days %2", days, time.toString("h:mm:ss"));
    #else
    return 0==days
            ? time.toString("h:mm:ss")
            : QString("%1:%2").arg(days).arg(time.toString("hh:mm:ss"));
    #endif
}

QString MPDParseUtils::addStreamName(const QString &url, const QString &name)
{
    return name.isEmpty() ? url : (url+"#"+name);
}

QString MPDParseUtils::getStreamName(const QString &url)
{
    int idx=url.lastIndexOf('#');
    return -1==idx ? QString() : url.mid(idx+1);
}

QString MPDParseUtils::getAndRemoveStreamName(QString &url)
{
    int idx=url.lastIndexOf('#');
    if (-1==idx) {
        return QString();
    }
    QString name=url.mid(idx+1);
    url=url.left(idx);
    return name;
}
