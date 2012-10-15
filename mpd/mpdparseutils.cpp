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

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
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

Song MPDParseUtils::parseSong(const QByteArray &data)
{
    Song song;
    QString tmpData = QString::fromUtf8(data.constData());
    QStringList lines = tmpData.split('\n');
    QStringList tokens;
    QString element;
    QString value;
    QString albumartist;

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
            Song song=parseSong(line);

            #ifdef TAGLIB_FOUND
            if (song.isCantataStream()) {
                Song mod=HttpServer::self()->decodeUrl(song.file);
                if (!mod.title.isEmpty()) {
                    mod.id=song.id;
                    song=mod;
                }
            }
            #endif

            song.setKey();
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

MusicLibraryItemRoot * MPDParseUtils::parseLibraryItems(const QByteArray &data)
{
    MusicLibraryItemRoot * const rootItem = new MusicLibraryItemRoot;
    QByteArray currentItem;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;

    for (int i = 0; i < amountOfLines; i++) {
        currentItem += lines.at(i);
        currentItem += "\n";
        if (i == lines.size() - 1 || lines.at(i + 1).startsWith("file:") || lines.at(i + 1).startsWith("playlist:")) {
            Song currentSong = parseSong(currentItem);
            currentItem.clear();

            if (Song::Playlist==currentSong.type) {
                if (songItem && Utils::getDir(songItem->file())==Utils::getDir(currentSong.file)) {
                    currentSong.albumartist=currentSong.artist=artistItem->data();
                    currentSong.album=albumItem->data();
                    songItem = new MusicLibraryItemSong(currentSong, albumItem);
                    albumItem->append(songItem);
                }
                continue;
            }
            if (currentSong.isEmpty()) {
                continue;
            }

            currentSong.fillEmptyFields();
            if (!artistItem || currentSong.albumArtist()!=artistItem->data()) {
                artistItem = rootItem->artist(currentSong);
            }
            if (!albumItem || currentSong.year!=albumItem->year() || albumItem->parentItem()!=artistItem || currentSong.album!=albumItem->data()) {
                albumItem = artistItem->album(currentSong);
            }

            songItem = new MusicLibraryItemSong(currentSong, albumItem);
            albumItem->append(songItem);
            albumItem->addGenre(currentSong.genre);
            artistItem->addGenre(currentSong.genre);
            rootItem->addGenre(currentSong.genre);
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
