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

#ifndef MUSIC_LIBRARY_MODEL_H
#define MUSIC_LIBRARY_MODEL_H

#include <QDateTime>
#include <QSet>
#include <QMap>
#include "musiclibraryitemroot.h"
#include "musiclibraryitemalbum.h"
#include "mpd-interface/song.h"
#include "musicmodel.h"

class QMimeData;
class MusicLibraryItemArtist;

class MusicLibraryModel : public MusicModel, public MusicLibraryErrorMonitor
{
    Q_OBJECT

public:
    static const QLatin1String constLibraryCache;
    static const QLatin1String constLibraryExt;
    static const QLatin1String constLibraryCompressedExt;

    static MusicLibraryModel * self();

    static void convertCache(const QString &compressedName);
    static void cleanCache();
    static bool validCacheDate(const QDateTime &dt) { return dt.isValid() && dt.date().year()>2000; }

    MusicLibraryModel(QObject *parent=0, bool isMpdModel=true, bool isCheckable=false);
    ~MusicLibraryModel();
    void readConfig();
    QModelIndex index(int, int, const QModelIndex & = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &, int) const;
    bool setData(const QModelIndex &idx, const QVariant &value, int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QStringList &filenames, bool insertNotFound=false) const;
    #ifndef ENABLE_UBUNTU
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    #endif
    const MusicLibraryItemRoot * root() const { return rootItem; }
    const MusicLibraryItemRoot * root(const MusicLibraryItem *) const { return root(); }
    bool isFromSingleTracks(const Song &s) const { return rootItem->isFromSingleTracks(s); }
    bool fromXML();
    void clear();
    QModelIndex findSongIndex(const Song &s) const;
    QModelIndex findArtistIndex(const QString &artist) const;
    QModelIndex findAlbumIndex(const QString &artist, const QString &album) const;
    const MusicLibraryItem * findSong(const Song &s) const;
    bool songExists(const Song &s) const { return rootItem->songExists(s); }
    bool updateSong(const Song &orig, const Song &edit) { return rootItem->updateSong(orig, edit); }
    void addSongToList(const Song &s) { rootItem->addSongToList(s); }
    void removeSongFromList(const Song &s) { rootItem->removeSongFromList(s); }
    void updateSongFile(const Song &from, const Song &to) { rootItem->updateSongFile(from, to); }
    void removeCache();
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres)
        { rootItem->getDetails(artists, albumArtists, composers, albums, genres); }
    QSet<QString> getAlbumArtists();
    bool update(const QSet<Song> &songs);
//    void uncheckAll();
    const QDateTime & lastUpdate() { return databaseTime; }
    void setSupportsAlbumArtistTag(bool s) { rootItem->setSupportsAlbumArtistTag(s); }
    void toggleGrouping();
    const QSet<QString> & genres() const { return rootItem->genres(); }
    // Get tracks of an album
    QList<Song> getAlbumTracks(const Song &s) const;
    // Get 1 track from each album by artist - used to create context view backdrop!
    QList<Song> getArtistAlbumsFirstTracks(const Song &song) const;
    void checkForNewSongs();

public Q_SLOTS:
    void clearNewState();
    void updateMusicLibrary(MusicLibraryItemRoot * root, QDateTime dbUpdate = QDateTime(), bool fromFile = false);
    void coverLoaded(const Song &song, int size);
    void updatingMpd();
    // Touch version...
    void setArtistImage(const Song &song, const QImage &img, const QString &file);
    void setCover(const Song &song, const QImage &img, const QString &file);

Q_SIGNALS:
    void haveNewItems(bool);
//     void updated(const MusicLibraryItemRoot *root);
    void updateGenres(const QSet<QString> &genres);
    void checkedSongs(const QSet<Song> &songs);
    void error(const QString &msg);

    // Used in Touch variant only...
    void updated();

private:
    void setParentState(const QModelIndex &parent);
    void loadError(const QString &msg) { emit error(msg); }

private:
    bool mpdModel;
    bool checkable;
    bool artistImages;
    MusicLibraryItemRoot *rootItem;
    QDateTime databaseTime;
    bool databaseTimeUnreliable;
};

#endif
