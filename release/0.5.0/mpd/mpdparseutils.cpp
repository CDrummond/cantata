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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "dirviewitemroot.h"
#include "dirviewitemdir.h"
#include "dirviewitemfile.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "playlist.h"
#include "song.h"
#include "output.h"
#include "covers.h"
#include "httpserver.h"
#include "debugtimer.h"

QString MPDParseUtils::fixPath(const QString &f)
{
    QString d(f);
    if (!d.isEmpty()) {
        d.replace(QLatin1String("//"), QChar('/'));
    }
    if (!d.isEmpty() && !d.endsWith('/')) {
        d+='/';
    }
    return d;
}

QString MPDParseUtils::getDir(const QString &f)
{
    QString d(f);

    int slashPos(d.lastIndexOf('/'));

    if(slashPos!=-1) {
        d.remove(slashPos+1, d.length());
    }

    return fixPath(d);
}

QList<Playlist> MPDParseUtils::parsePlaylists(const QByteArray &data)
{
    TF_DEBUG
    QList<Playlist> playlists;
    QList<QByteArray> lines = data.split('\n');
    QList<QByteArray> tokens;

    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        tokens = lines.at(i).split(':');

        if (tokens.at(0) == "playlist") {
            Playlist playlist;
            playlist.name = tokens.at(1).simplified();
            i++;
            tokens = lines.at(i).split(':');

            if (tokens.at(0) == "Last-Modified") {
                QByteArray lastModified(tokens.at(1));
                lastModified += tokens.at(2);
                lastModified += tokens.at(3);
                playlist.lastModified.fromString(lastModified, Qt::ISODate);

                playlists.append(playlist);
            }
        }
    }

    return playlists;
}

void MPDParseUtils::parseStats(const QByteArray &data)
{
    TF_DEBUG
    MPDStats::Values v;
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
    MPDStats::set(v);
}

void MPDParseUtils::parseStatus(const QByteArray &data)
{
    TF_DEBUG
    MPDStatus * const status = MPDStatus::self();
    QList<QByteArray> lines = data.split('\n');
    QList<QByteArray> tokens;

    status->acquireWriteLock();

    int amountOfLines = lines.size();

    for (int i = 0; i < amountOfLines; i++) {
        tokens = lines.at(i).split(':');

        if (tokens.at(0) == "volume") {
            status->setVolume(tokens.at(1).toUInt());
        } else if (tokens.at(0) == "consume") {
            if (tokens.at(1).trimmed() == "1") {
                status->setConsume(true);
            } else {
                status->setConsume(false);
            }
        } else if (tokens.at(0) == "repeat") {
            if (tokens.at(1).trimmed() == "1") {
                status->setRepeat(true);
            } else {
                status->setRepeat(false);
            }
        } else if (tokens.at(0) == "random") {
            if (tokens.at(1).trimmed() == "1") {
                status->setRandom(true);
            } else {
                status->setRandom(false);
            }
        } else if (tokens.at(0) == "playlist") {
            status->setPlaylist(tokens.at(1).toUInt());
        } else if (tokens.at(0) == "playlistlength") {
            status->setPlaylistLength(tokens.at(1).toInt());
        } else if (tokens.at(0) == "playlistqueue") {
            status->setPlaylistQueue(tokens.at(1).toInt());
        } else if (tokens.at(0) == "xfade") {
            status->setCrossFade(tokens.at(1).toInt());
        } else if (tokens.at(0) == "state") {
            if (tokens.at(1).contains("play")) {
                status->setState(MPDStatus::State_Playing);
            } else if (tokens.at(1).contains("stop")) {
                status->setState(MPDStatus::State_Stopped);
            } else {
                status->setState(MPDStatus::State_Paused);
            }
        } else if (tokens.at(0) == "song") {
            status->setSong(tokens.at(1).toInt());
        } else if (tokens.at(0) == "songid") {
            status->setSongId(tokens.at(1).toInt());
        } else if (tokens.at(0) == "time") {
            status->setTimeElapsed(tokens.at(1).toInt());
            status->setTimeTotal(tokens.at(2).toInt());
        } else if (tokens.at(0) == "bitrate") {
            status->setBitrate(tokens.at(1).toUInt());
        } else if (tokens.at(0) == "audio") {
        } else if (tokens.at(0) == "updating_db") {
            status->setUpdatingDb(tokens.at(1).toInt());
        } else if (tokens.at(0) == "error") {
            status->setError(tokens.at(1));
        }
    }

    status->releaseWriteLock();
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
            song.file.replace("\"", "\\\"");
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
        } else if (element == QLatin1String("Pos")) {
            song.pos = value.toInt();
        } else if (element == QLatin1String("Id")) {
            song.id = value.toUInt();
        } else if (element == QLatin1String("Disc")) {
            song.disc = value.split("/").at(0).toUInt();
        } else if (element == QLatin1String("Date")) {
            song.year = value.toUInt();
        } else if (element == QLatin1String("Genre")) {
            song.genre = value;
        }  else if (element == QLatin1String("Name")) {
            song.name = value;
        }
    }

    if (song.genre.isEmpty()) {
        #ifdef ENABLE_KDE_SUPPORT
        song.genre = i18n("Unknown");
        #else
        song.genre = QObject::tr("Unknown");
        #endif
    }

    if (!albumartist.isEmpty() && albumartist != song.artist) {
        song.modifiedtitle = song.artist + " - " + song.title;
    }
    return song;
}

QList<Song> MPDParseUtils::parseSongs(const QByteArray &data)
{
    TF_DEBUG
    QList<Song> songs;
    QByteArray line;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    QString album;
    QString artist;
    quint16 key=0;

    for (int i = 0; i < amountOfLines; i++) {
        line += lines.at(i);
        // Skip the "OK" line, this is NOT a song!!!
        if ("OK"==line) {
            continue;
        }
        line += "\n";
        if (i == lines.size() - 1 || lines.at(i + 1).startsWith("file:")) {
            Song song=parseSong(line);

            if (song.file.startsWith("http") && HttpServer::self()->isOurs(song.file)) {
                Song mod=HttpServer::self()->decodeUrl(song.file);
                if (!mod.title.isEmpty()) {
                    mod.id=song.id;
                    song=mod;
                }
            }

            if (song.album!=album || song.albumArtist()!=artist) {
                key++;
                album=song.album;
                artist=song.albumArtist();
            }
            song.key=key;
            songs.append(song);
            line.clear();
        }
    }

    return songs;
}

QStringList MPDParseUtils::parseUrlHandlers(const QByteArray &data)
{
    TF_DEBUG
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

bool MPDParseUtils::groupSingle()
{
    return groupSingleTracks;
}

void MPDParseUtils::setGroupSingle(bool g)
{
    groupSingleTracks=g;
}

MusicLibraryItemRoot * MPDParseUtils::parseLibraryItems(const QByteArray &data)
{
    TF_DEBUG
    MusicLibraryItemRoot * const rootItem = new MusicLibraryItemRoot;
    QByteArray currentItem;
    QList<QByteArray> lines = data.split('\n');
    int amountOfLines = lines.size();
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;

    for (int i = 0; i < amountOfLines; i++) {
        currentItem += lines.at(i);
        currentItem += "\n";
        if (i == lines.size() - 1 || lines.at(i + 1).startsWith("file:")) {
            Song currentSong = parseSong(currentItem);
            currentItem.clear();

            if (currentSong.isEmpty()) {
                continue;
            }

            currentSong.fillEmptyFields();
            if (!artistItem || currentSong.albumArtist()!=artistItem->data()) {
                artistItem = rootItem->artist(currentSong);
            }
            if (!albumItem || albumItem->parent()!=artistItem || currentSong.album!=albumItem->data()) {
                albumItem = artistItem->album(currentSong);
            }

            MusicLibraryItemSong *songItem = new MusicLibraryItemSong(currentSong, albumItem);

            albumItem->append(songItem);
            albumItem->addGenre(currentSong.genre);
            artistItem->addGenre(currentSong.genre);
            songItem->addGenre(currentSong.genre);
            rootItem->addGenre(currentSong.genre);
        }
    }

    if (groupSingleTracks) {
        rootItem->groupSingleTracks();
    }

    return rootItem;
}

DirViewItemRoot * MPDParseUtils::parseDirViewItems(const QByteArray &data)
{
    TF_DEBUG
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

            if (currentDir->type() == DirViewItem::Type_Root) {
                static_cast<DirViewItemRoot *>(currentDir)->insertFile(parts.at(parts.size() - 1));
            } else {
                static_cast<DirViewItemDir *>(currentDir)->insertFile(parts.at(parts.size() - 1));
            }
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

            if (currentDir->type() == DirViewItem::Type_Root) {
                currentDir = static_cast<DirViewItemRoot *>(currentDir)->createDirectory(parts.at(parts.size() - 1));
            } else {
                currentDir = static_cast<DirViewItemDir *>(currentDir)->createDirectory(parts.at(parts.size() - 1));
            }

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
            : 1==days
                ? QObject::tr("1 day %1").arg(time.toString("h:mm:ss"))
                : QObject::tr("%1 days %2").arg(days).arg(time.toString("h:mm:ss"));
    #endif
}
