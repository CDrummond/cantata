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

#ifndef SONG_H
#define SONG_H

#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QMetaType>

struct Song
{
    static const quint16 constNullKey;

    struct AlbumKey {
        AlbumKey(quint16 y, const QString &n)
            : year(y)
            , name(n) {
        }
        bool operator==(const AlbumKey &o) const {
            return year==o.year && name==o.name;
        }
        bool operator<(const AlbumKey &o) const {
            return year<o.year || (year==o.year && name<o.name);
        }
        quint16 year;
        QString name;
    };

    enum Type {
        Standard        = 0,
        SingleTracks    = 1,
        MultipleArtists = 2
    };

    qint32 id;
    QString file;
    QString album;
    QString artist;
    QString albumartist;
    QString title;
    QString genre;
    QString name;
//     quint32 pos;
    quint16 disc;
    quint16 time;
    qint16 track;
    quint16 year : 14;
    mutable Type type : 2;
    mutable qint32 size;

    // Only used in PlayQueue/PlayLists...
    quint16 key;

    Song();
    Song(const Song &o) { *this=o; }
    Song & operator=(const Song &o);
    bool operator==(const Song &o) const;
    bool operator<(const Song &o) const;
    virtual ~Song() { }
    bool isEmpty() const;
    void fillEmptyFields();
    void setKey();
    virtual void clear();
    static QString formattedTime(quint32 seconds);
    QString format();
    QString entryName() const;
    QString artistSong() const;
    const QString & albumArtist() const { return albumartist.isEmpty() ? artist : albumartist; }
    QString displayTitle() const { return !albumartist.isEmpty() && albumartist!=artist ? artistSong() : title; }
    QString trackAndTitleStr(bool addArtist=false) const;
    void updateSize(const QString &dir) const;
    static bool isVariousArtists(const QString &str);
    bool isVariousArtists() const { return isVariousArtists(albumArtist()); }
    bool isUnknown() const;
    bool fixVariousArtists();
    bool revertVariousArtists();
    static QString capitalize(const QString &s);
    bool capitalise();
    bool isStream() const { return file.isEmpty() || file.contains("://"); }
    bool isCantataStream() const;
};

Q_DECLARE_METATYPE(Song)

inline uint qHash(const Song &key)
{
    return qHash(key.file);
}

inline uint qHash(const Song::AlbumKey &key)
{
    return qHash(key.year)+qHash(key.name);
}

#endif
