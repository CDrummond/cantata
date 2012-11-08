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

#ifndef MUSIC_LIBRARY_MODEL_H
#define MUSIC_LIBRARY_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QDateTime>
#include <QtCore/QSet>
#include "musiclibraryitemroot.h"
#include "musiclibraryitemalbum.h"
#include "song.h"

class QMimeData;
class MusicLibraryItemArtist;

class MusicLibraryModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    static MusicLibraryModel * self();

    static void cleanCache();

    MusicLibraryModel(QObject *parent = 0);
    ~MusicLibraryModel();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QStringList &filenames) const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    const MusicLibraryItemRoot * root() const { return rootItem; }
    bool isFromSingleTracks(const Song &s) const { return rootItem->isFromSingleTracks(s); }
    bool fromXML();
    void clear();
    QModelIndex findSongIndex(const Song &s) const;
    const MusicLibraryItem * findSong(const Song &s) const;
    bool songExists(const Song &s) const;
    bool updateSong(const Song &orig, const Song &edit);
    void addSongToList(const Song &s);
    void removeSongFromList(const Song &s);
    void updateSongFile(const Song &from, const Song &to);
    void removeCache();
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres);
    bool update(const QSet<Song> &songs);
    bool useArtistImages() const { return artistImages; }
    void setUseArtistImages(bool a) { artistImages=a; }
    const QDateTime & lastUpdate() { return databaseTime; }
    bool useLargeImages() const { return rootItem->useLargeImages(); }
    void setLargeImages(bool a) { rootItem->setLargeImages(a); }
    void setSupportsAlbumArtistTag(bool s) { rootItem->setSupportsAlbumArtistTag(s); }

public Q_SLOTS:
    void updateMusicLibrary(MusicLibraryItemRoot * root, QDateTime dbUpdate = QDateTime(), bool fromFile = false);
    void setArtistImage(const QString &artist, const QImage &img);
    void setCover(const Song &song, const QImage &img, const QString &file);

Q_SIGNALS:
//     void updated(const MusicLibraryItemRoot *root);
   void updateGenres(const QSet<QString> &genres);

private:
    void toXML(const MusicLibraryItemRoot *root, const QDateTime &date);

private:
    bool artistImages;
    MusicLibraryItemRoot *rootItem;
    QDateTime databaseTime;
};

#endif
