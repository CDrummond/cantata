/*
 * Cantata
 *
 * Copyright (c) 2017-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

class Configuration;

class SqlLibraryModel : public ActionModel
{
    Q_OBJECT

public:
    enum Type {
        T_Root,
        T_Genre,
        T_Artist,
        T_Album,
        T_Track
    };

    static Type toGrouping(const QString &str);
    static QString groupingStr(Type m);

    class CollectionItem;
    class Item
    {
    public:
        Item(Type t,  CollectionItem *p=nullptr)
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
        TrackItem(const Song &s, CollectionItem *p=nullptr)
            : Item(T_Track, p) { setSong(s); }
        ~TrackItem() override { }

        const QString & getId() const override { return getSong().file; }
        QString getText() const override { return getSong().trackAndTitleStr(); }
        QString getSubText() const override { return Utils::formatTime(getSong().time, true); }
    };

    class CollectionItem : public Item
    {
    public:
        CollectionItem(Type t, const QString &i, const QString &txt=QString(), const QString &sub=QString(), CollectionItem *p=nullptr)
            : Item(t, p), id(i), text(txt), subText(sub) { }
        ~CollectionItem() override { qDeleteAll(children); }

        const QList<Item *> & getChildren() const { return children; }
        int getChildCount() const override { return children.count();}
        void add(Item *i);
        const Item * getChild(const QString &id) const;
        const QString & getId() const override { return id; }
        QString getText() const override { return text; }
        QString getSubText() const override { return subText; }

    private:
        QString id;
        QString text;
        QString subText;
        QList<Item *> children;
        QMap<QString, Item *> childMap;
    };

    class AlbumItem : public CollectionItem {
    public:
        AlbumItem(const QString &ar, const QString &i, const QString &txt=QString(), const QString &sub=QString(),
                  const QString &tSub=QString(), CollectionItem *p=nullptr, int cat=-1)
            : CollectionItem(T_Album, i, txt, sub, p), artistId(ar), titleSub(tSub), category(cat) { }
        ~AlbumItem() override { }

        const QString & getArtistId() const { return artistId; }
        const QString getUniqueId() const override { return artistId+getId(); }
        const QString & getTitleSub() const {return titleSub; }
        int getCategory() { return category; }

    private:
        QString artistId;
        QString titleSub;
        int category;
    };

    SqlLibraryModel(LibraryDb *d, QObject *p, Type top=T_Artist);

    void reload() { libraryUpdated(); }
    void clear();
    void settings(Type top, LibraryDb::AlbumSort lib, LibraryDb::AlbumSort al);
    Type topLevel() const { return tl; }
    LibraryDb::AlbumSort libraryAlbumSort() const { return librarySort; }
    LibraryDb::AlbumSort albumAlbumSort() const { return albumSort; }
    void setTopLevel(Type t);
    void setLibraryAlbumSort(LibraryDb::AlbumSort s);
    void setAlbumAlbumSort(LibraryDb::AlbumSort s);
    virtual void load(Configuration &config);
    virtual void save(Configuration &config);

    void search(const QString &str, const QString &genre=QString());

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    bool hasChildren(const QModelIndex &index) const override;
    bool canFetchMore(const QModelIndex &index) const override;
    void fetchMore(const QModelIndex &index) override;
    QVariant data(const QModelIndex &index, int role) const override;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;

    QList<Song> songs(const QModelIndexList &list, bool allowPlaylists) const;
    QStringList filenames(const QModelIndexList &list, bool allowPlaylists) const;
    QModelIndex findSongIndex(const Song &song);
    QModelIndex findAlbumIndex(const QString &artist, const QString &album);
    QModelIndex findArtistIndex(const QString &artist);
    QSet<QString> getGenres() const;
    QSet<QString> getArtists() const;
    QList<Song> getAlbumTracks(const QString &artistId, const QString &albumId) const;
    QList<Song> getAlbumTracks(const Song &song) const { return getAlbumTracks(song.albumArtistOrComposer(), song.albumId()); }
    QList<Song> songs(const QStringList &files, bool allowPlaylists=false) const;
    QList<LibraryDb::Album> getArtistOrComposerAlbums(const QString &artist) const;
    void getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &composers, QSet<QString> &albums, QSet<QString> &genres);
    bool songExists(const Song &song);
    LibraryDb::Album getRandomAlbum(const QStringList &genres, const QStringList &artists) const { return db->getRandomAlbum(genres, artists); }
    int trackCount() const;

Q_SIGNALS:
    void error(const QString &str);

public Q_SLOTS:
    void clearDb();

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
    LibraryDb::AlbumSort librarySort;
    LibraryDb::AlbumSort albumSort;
    QStringList categories;
};

#endif
