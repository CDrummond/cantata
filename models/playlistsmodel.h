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

#ifndef PLAYLISTS_MODEL_H
#define PLAYLISTS_MODEL_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include "playlist.h"
#include "song.h"

class QMenu;

class PlaylistsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    struct Item
    {
        virtual bool isPlaylist() = 0;
        virtual ~Item() { }
    };

    struct PlaylistItem;
    struct SongItem : public Item, public Song
    {
        SongItem() { }
        SongItem(const Song &s, PlaylistItem *p=0) : Song(s), parent(p) { }
        bool isPlaylist() { return false; }
        PlaylistItem *parent;
    };

    struct PlaylistItem : public Item
    {
        PlaylistItem() : loaded(false) { }
        PlaylistItem(const QString &n) : name(n), loaded(false) { }
        virtual ~PlaylistItem();
        bool isPlaylist() { return true; }
        void updateGenres();
        SongItem * getSong(const Song &song);
        void clearSongs();
        QString name;
        bool loaded;
        QList<SongItem *> songs;
        QSet<QString> genres;
    };

    static PlaylistsModel * self();

    PlaylistsModel(QObject *parent = 0);
    ~PlaylistsModel();
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int col, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Qt::DropActions supportedDropActions() const;
    QStringList filenames(const QModelIndexList &indexes, bool filesOnly=false) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*col*/, const QModelIndex &parent);
    QStringList mimeTypes() const;
    void getPlaylists();
    void clear();
    bool exists(const QString &n) { return 0!=getPlaylist(n); }

    QMenu * menu() { return itemMenu; }

    static QString strippedText(QString s);

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files);
    void listPlaylists();
    void playlistInfo(const QString &name) const;
    void addToPlaylist(const QString &name, const QStringList &songs);
    void moveInPlaylist(const QString &name, int from, int to);

    void addToNew();
    void addToExisting(const QString &name);
    void updateGenres(const QSet<QString> &genres);

private Q_SLOTS:
    void setPlaylists(const QList<Playlist> &playlists);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);
    void removedFromPlaylist(const QString &name, const QList<int> &positions);
    void movedInPlaylist(const QString &name, int from, int to);
    void emitAddToExisting();

private:
    void updateGenreList();
    void updateItemMenu();
    PlaylistItem * getPlaylist(const QString &name);
    void clearPlaylists();

private:
    QList<PlaylistItem *> items;
    QMenu *itemMenu;
};

#endif
