/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef ALBUMSMODEL_H
#define ALBUMSMODEL_H

#include <QList>
#include <QSet>
#include <QStringList>
#include "song.h"
#include "musiclibraryitemalbum.h"
#include "actionmodel.h"

class MusicLibraryItemRoot;
class QSize;
class QPixmap;

class AlbumsModel : public ActionModel
{
    Q_OBJECT

public:
    static MusicLibraryItemAlbum::CoverSize currentCoverSize();
    static void setCoverSize(MusicLibraryItemAlbum::CoverSize size);
    static int iconSize();
    static void setItemSize(const QSize &sz);
    static void setUseLibrarySizes(bool u);
    static bool useLibrarySizes();

    enum Sort
    {
        Sort_AlbumArtist,
        Sort_ArtistAlbum,
        Sort_ArtistAlbumYear
    };

    enum Columnms
    {
        COL_NAME,
        COL_FILES,
        COL_GENRES
    };

    struct Item
    {
        virtual bool isAlbum() { return false; }
        virtual ~Item() { }
    };

    struct AlbumItem;
    struct SongItem : public Item, public Song
    {
        SongItem(const Song &s, AlbumItem *p=0) : Song(s), parent(p) { }
        virtual ~SongItem() { }
        AlbumItem *parent;
    };

    struct AlbumItem : public Item
    {
        AlbumItem(const QString &ar, const QString &al, quint16 y);
        virtual ~AlbumItem();
        bool operator<(const AlbumItem &o) const;
        bool isAlbum() { return true; }
        void clearSongs();
        void setSongs(MusicLibraryItemAlbum *ai);
        void setName();
        quint32 totalTime();
        void getCover(bool urgent);
        bool isSingleTracks() const { return Song::SingleTracks==type; }
        const SongItem *getCueFile() const;
        QString artist;
        QString album;
        QString name;
        quint16 year;
        QList<SongItem *> songs;
        QSet<QString> genres;
        QPixmap *cover;
        bool updated;
        bool coverRequested;
        Song::Type type;
        quint32 time;
    };

    static AlbumsModel * self();

    AlbumsModel(QObject *parent=0);
    ~AlbumsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QStringList filenames(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists=false) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void clear();
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    int albumSort() const;
    void setAlbumSort(int s);
//     void getCovers();

public Q_SLOTS:
    void setCover(const Song &song, const QImage &img, const QString &file);
    void update(const MusicLibraryItemRoot *root);

private:
    bool enabled;
//     bool coversRequested;
    mutable QList<AlbumItem *> items;
};

#endif
