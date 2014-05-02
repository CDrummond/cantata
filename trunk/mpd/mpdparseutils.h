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

#ifndef MPD_PARSE_UTILS_H
#define MPD_PARSE_UTILS_H

#include <QString>
#include <QSet>

struct Song;
struct Playlist;
struct Output;
struct MPDStatsValues;
struct MPDStatusValues;
class DirViewItemRoot;
class MusicLibraryItemRoot;

namespace MPDParseUtils
{
    extern void enableDebug();

    struct IdPos {
        IdPos(qint32 i, quint32 p)
            : id(i)
            , pos(p) {
        }
        qint32 id;
        quint32 pos;
    };

    enum Location {
        Loc_Library,
        Loc_Playlists,
        Loc_PlayQueue,
        Loc_Streams
    };

    extern QList<Playlist> parsePlaylists(const QByteArray &data);
    extern MPDStatsValues parseStats(const QByteArray &data);
    extern MPDStatusValues parseStatus(const QByteArray &data);
    extern Song parseSong(const QList<QByteArray> &lines, Location location);
    inline Song parseSong(const QByteArray &data, Location location) { return parseSong(data.split('\n'), location); }
    extern QList<Song> parseSongs(const QByteArray &data, Location location);
    extern QList<IdPos> parseChanges(const QByteArray &data);
    extern QStringList parseList(const QByteArray &data, const QByteArray &key);
    extern bool groupSingle();
    extern void setGroupSingle(bool g);
    extern void parseLibraryItems(const QByteArray &data, const QString &mpdDir, long mpdVersion,
                                  bool isMopidy, MusicLibraryItemRoot *rootItem, bool parsePlaylists=true,
                                  QSet<QString> *childDirs=0);
    extern DirViewItemRoot * parseDirViewItems(const QByteArray &data, bool isMopidy);
    extern QList<Output> parseOuputs(const QByteArray &data);
    extern QString addStreamName(const QString &url, const QString &name);
    extern QString getStreamName(const QString &url);
    extern QString getAndRemoveStreamName(QString &url);
};

#endif
