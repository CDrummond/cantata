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

#ifndef MUSIC_LIBRARY_ITEM_ROOT_H
#define MUSIC_LIBRARY_ITEM_ROOT_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QDateTime>
#include "musiclibraryitem.h"
#include "song.h"

class QDateTime;
class MusicLibraryItemArtist;

class MusicLibraryItemRoot : public MusicLibraryItem
{
public:
    MusicLibraryItemRoot(const QString &name=QString())
        : MusicLibraryItem(name, MusicLibraryItem::Type_Root, 0) {
    }
    virtual ~MusicLibraryItemRoot() {
    }

    virtual QString icon() const {
        return QString();
    }

    virtual bool isDevice() const {
        return false;
    }

    MusicLibraryItemArtist * artist(const Song &s, bool create=true);
    MusicLibraryItemArtist * createArtist(const Song &s);
    void groupSingleTracks();
    void groupMultipleArtists();
    bool isFromSingleTracks(const Song &s) const;
    void refreshIndexes();
    void remove(MusicLibraryItemArtist *artist);
    QSet<Song> allSongs() const;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres);
    void updateSongFile(const Song &from, const Song &to);
    void toXML(const QString &filename, const QString &pathRemove, const QDateTime &date=QDateTime()) const;
    quint32 fromXML(const QString &filename, const QString &pathAppend, const QDateTime &date=QDateTime());

private:
    QHash<QString, int> m_indexes;
};

#endif
