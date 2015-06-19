/*
 * Cantata
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef SQL_LIBRARY_MODEL_H
#define SQL_LIBRARY_MODEL_H

#include "actionmodel.h"
#include "mpd-interface/song.h"
#include "support/utils.h"
#include "db/librarydb.h"
#include <QMap>

class SqlLibraryModel : public ActionModel
{
    Q_OBJECT

public:

    static const QLatin1String constGroupGenre;
    static const QLatin1String constGroupArtist;
    static const QLatin1String constGroupAlbum;

    enum Type {
        T_Root,
        T_Genre,
        T_Artist,
        T_Album,
        T_Track
    };

    class CollectionItem;
    class Item
    {
    public:
        Item(Type t,  CollectionItem *p=0)
            : type(t), parent(p) { }
        virtual ~Item() { }

        Type getType() const { return type; }
        virtual const QString & getId() const =0;
        virtual const QString getUniqueId() const { return getId(); }
        virtual QString getText() const =0;
        virtual QString getSubText() const =0;
        CollectionItem * getParent() const { return parent; }
        int getRow() const { return row; }
        void setRow(int r) { row=r; }
        virtual int getChildCount() const { return 0;}
        const Song & getSong() const { return song; }
        void setSong(const Song &s) { song=s; }

    private:
        Type type;
        CollectionItem *parent;
        int row;
        Song song;
    };

    class TrackItem : public Item
    {
    public:
        TrackItem(const Song &s, CollectionItem *p=0)
            : Item(T_Track, p) { setSong(s); }
        virtual ~TrackItem() { }

        virtual const QString & getId() const { return getSong().file; }
        virtual QString getText() const { return getSong().trackAndTitleStr(); }
        virtual QString getSubText() const { return Utils::formatTime(getSong().time, true); }
    };

    class CollectionItem : public Item
    {
    public:
        CollectionItem(Type t, const QString &i, const QString &txt=QString(), const QString &sub=QString(), CollectionItem *p=0)
            : Item(t, p), id(i), text(txt), subText(sub) { }
        virtual ~CollectionItem() { qDeleteAll(children); }

        const QList<Item *> getChildren() const { return children; }
        virtual int getChildCount() const { return children.count();}
        void add(Item *i);
        const Item * getChild(const QString &id) const;
        virtual const QString & getId() const { return id; }
        virtual QString getText() const { return text; }
        virtual QString getSubText() const { return subText; }

    private:
        QString id;
        QString text;
        QString subText;
        QList<Item *> children;
        QMap<QString, Item *> childMap;
    };

    class AlbumItem : public CollectionItem {
    public:
        AlbumItem(const QString &ar, const QString &i, const QString &txt=QString(), const QString &sub=QString(), CollectionItem *p=0)
            : CollectionItem(T_Album, i, txt, sub, p), artistId(ar) { }
        virtual ~AlbumItem() { }

        const QString & getArtistId() const { return artistId; }
        const QString getUniqueId() const { return artistId+getId(); }

    private:
        QString artistId;
    };

    struct AlbumInfo {
        QString albumName;
        QString albumId;
        QString artistId;
        QString genreId;
    };

    SqlLibraryModel(LibraryDb *d, QObject *p);

    void clear();
    void clearDb();
    void settings(const QString &top, const QString &lib, const QString &al);
    Type topLevel() const { return tl; }
    void search(const QString &str);

    Qt::ItemFlags flags(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    bool hasChildren(const QModelIndex &index) const;
    bool canFetchMore(const QModelIndex &index) const;
    void fetchMore(const QModelIndex &index);
    QVariant data(const QModelIndex &index, int role) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;

    QList<Song> songs(const QModelIndexList &list, bool allowPlaylists) const;
    QStringList filenames(const QModelIndexList &list, bool allowPlaylists) const;
    QModelIndex findSongIndex(const Song &song);
    QModelIndex findAlbumIndex(const QString &artist, const QString &album);
    QModelIndex findArtistIndex(const QString &artist);
    QSet<QString> getArtists() const;
    QList<Song> getAlbumTracks(const Song &song) const;
    QList<Song> songs(const QStringList &files, bool allowPlaylists=false) const;
    QList<LibraryDb::Album> getArtistAlbums(const QString &artist) const;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres);
    bool songExists(const Song &song);

protected Q_SLOTS:
    void libraryUpdated();

private:
    void populate(const QModelIndexList &list) const;
    QModelIndexList children(const QModelIndex &parent) const;
    QList<Song> songs(const QModelIndex &idx, bool allowPlaylists) const;
    Item * toItem(const QModelIndex &index) const { return index.isValid() ? static_cast<Item*>(index.internalPointer()) : root; }
    virtual Song & fixPath(Song &s) const { return s; }

protected:
    Type tl;
    CollectionItem *root;
    LibraryDb *db;
    QString librarySort;
    QString albumSort;
};

#endif
