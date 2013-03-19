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

#ifndef MPD_PARSE_UTILS_H
#define MPD_PARSE_UTILS_H

#include <QString>

class Song;
class Playlist;
class DirViewItemRoot;
class MusicLibraryItemRoot;
class Output;
class MPDStatsValues;
class MPDStatusValues;

class MPDParseUtils
{
public:
    struct IdPos {
        IdPos(qint32 i, quint32 p)
            : id(i)
            , pos(p) {
        }
        qint32 id;
        quint32 pos;
    };
    static QList<Playlist> parsePlaylists(const QByteArray &data);
    static MPDStatsValues parseStats(const QByteArray &data);
    static MPDStatusValues parseStatus(const QByteArray &data);
    static Song parseSong(const QByteArray &data, bool isPlayQueue);
    static QList<Song> parseSongs(const QByteArray &data);
    static QList<IdPos> parseChanges(const QByteArray &data);
    static QStringList parseUrlHandlers(const QByteArray &data);
    static bool groupSingle();
    static void setGroupSingle(bool g);
    static bool groupMultiple();
    static void setGroupMultiple(bool g);
    static MusicLibraryItemRoot * parseLibraryItems(const QByteArray &data);
    static DirViewItemRoot * parseDirViewItems(const QByteArray &data);
    static QList<Output> parseOuputs(const QByteArray &data);
    static QString formatDuration(const quint32 totalseconds);
    static QString addName(const QString &url, const QString &name);
    static QString getName(const QString &url);
};

#endif
