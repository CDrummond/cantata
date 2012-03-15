/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include "song.h"
#include "musiclibraryitemalbum.h"

class MusicLibraryItemRoot;
class QSize;
class QPixmap;

class AlbumsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    static MusicLibraryItemAlbum::CoverSize currentCoverSize();
    static void setCoverSize(MusicLibraryItemAlbum::CoverSize size);
    static int iconSize();
    static void setItemSize(const QSize &sz);
    static void setUseLibrarySizes(bool u);
    static bool useLibrarySizes();

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
        AlbumItem(const QString &ar, const QString &al, bool albumFirst);
        virtual ~AlbumItem();
        bool isAlbum() { return true; }
        void setSongs(MusicLibraryItemAlbum *ai);
        void setName(bool albumFirst);
        quint32 totalTime();
        QString artist;
        QString album;
        QString name;
        QList<SongItem *> songs;
        QSet<QString> genres;
        QPixmap *cover;
        bool updated;
        bool coverRequested;
        bool isSingleTracks;
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
    QStringList filenames(const QModelIndexList &indexes) const;
    QList<Song> songs(const QModelIndexList &indexes) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    void clear();
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
    bool albumFirst() const { return sortAlbumFirst; }
    void setAlbumFirst(bool a);

public Q_SLOTS:
    void setCover(const QString &artist, const QString &album, const QImage &img, const QString &file);
    void update(const MusicLibraryItemRoot *root);

private:
    bool enabled;
    bool sortAlbumFirst;
    mutable QList<AlbumItem *> items;
};

#endif
