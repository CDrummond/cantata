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

#ifndef MPD_PARSE_UTILS_H
#define MPD_PARSE_UTILS_H

#include <QString>
#include <QSet>
#include "config.h"
#include "song.h"

struct Playlist;
struct Output;
struct MPDStatsValues;
struct MPDStatusValues;

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
        Loc_Streams,
        Loc_Search,
        Loc_Browse
    };

    enum CueSupport {
        Cue_Parse,
        Cue_ListButDontParse,
        Cue_Ignore,

        Cue_Count
    };

    struct Sticker {
        QByteArray file;
        QByteArray value;
    };

    extern QString toStr(CueSupport cs);
    extern CueSupport toCueSupport(const QString &cs);
    extern void setCueFileSupport(CueSupport cs);
    extern CueSupport cueFileSupport();
    extern void setSingleTracksFolders(const QSet<QString> &folders);
    extern QList<Playlist> parsePlaylists(const QByteArray &data);
    extern MPDStatsValues parseStats(const QByteArray &data);
    extern MPDStatusValues parseStatus(const QByteArray &data);
    extern Song parseSong(const QList<QByteArray> &lines, Location location);
    inline Song parseSong(const QByteArray &data, Location location) { return parseSong(data.split('\n'), location); }
    extern QList<Song> parseSongs(const QByteArray &data, Location location);
    extern QList<IdPos> parseChanges(const QByteArray &data);
    extern QStringList parseList(const QByteArray &data, const QByteArray &key);
    typedef QMap<QByteArray, QStringList> MessageMap;
    extern MessageMap parseMessages(const QByteArray &data);
    extern void parseDirItems(const QByteArray &data, const QString &mpdDir, long mpdVersion, QList<Song> &songList, const QString &dir, QStringList &subDirs, Location loc);
    extern QList<Output> parseOuputs(const QByteArray &data);
    extern QByteArray parseSticker(const QByteArray &data, const QByteArray &sticker);
    extern QList<Sticker> parseStickers(const QByteArray &data, const QByteArray &sticker);
    // Single hash when saving streams to [Radio Streams] - for compatability
    extern QString addStreamName(const QString &url, const QString &name, bool singleHash=false);
    extern QString getStreamName(const QString &url);
    // checkSingleHash - check for #<Name> as well as #StreamName:<Name>
    extern QString getAndRemoveStreamName(QString &url, bool checkSingleHash=false);
};

#endif
