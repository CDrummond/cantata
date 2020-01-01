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

#ifndef PLAYLISTS_MODEL_H
#define PLAYLISTS_MODEL_H

#include <QIcon>
#include <QAbstractItemModel>
#include <QList>
#include <QMap>
#include "mpd-interface/playlist.h"
#include "mpd-interface/song.h"
#include "support/icon.h"
#include "actionmodel.h"

class MirrorMenu;
class QAction;

class PlaylistsModel : public ActionModel
{
    Q_OBJECT

public:
    enum Columns
    {
        COL_TITLE,
        COL_ARTIST,
        COL_ALBUM,
        COL_YEAR,
        COL_ORIGYEAR,
        COL_GENRE,
        COL_LENGTH,
        COL_COMPOSER,
        COL_PERFORMER,
        COL_FILENAME,
        COL_PATH,

        COL_COUNT
    };

    struct Item
    {
        virtual bool isPlaylist() = 0;
        virtual ~Item() { }
    };

    struct PlaylistItem;
    struct SongItem : public Item, public Song
    {
        SongItem() : parent(nullptr) { }
        SongItem(const Song &s, PlaylistItem *p=nullptr) : Song(s), parent(p) { }
        bool isPlaylist() override { return false; }
        PlaylistItem *parent;
    };

    struct PlaylistItem : public Item
    {
        PlaylistItem(quint32 k) : loaded(false), isSmartPlaylist(false), time(0), key(k) { }
        PlaylistItem(const Playlist &pl, quint32 k);
        ~PlaylistItem() override;
        bool isPlaylist() override { return true; }
        SongItem * getSong(const Song &song, int offset);
        void clearSongs();
        quint32 totalTime();
        const QString & visibleName() const { return shortName.isEmpty() ? name : shortName; }
        QString name;
        QString shortName;
        bool loaded;
        bool isSmartPlaylist;
        QList<SongItem *> songs;
        quint32 time;
        quint32 key;
        QDateTime lastModified;
    };

    static PlaylistsModel * self();
    static QString headerText(int col);

    PlaylistsModel(QObject *parent = nullptr);
    ~PlaylistsModel() override;
    QString name() const;
    QString title() const;
    QString descr() const;
    const QIcon & icon() const { return icn; }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override { Q_UNUSED(parent) return COL_COUNT; }
    bool canFetchMore(const QModelIndex &index) const override;
    void fetchMore(const QModelIndex &index) override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QModelIndex index(int row, int col, const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &, int) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole) override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList filenames(const QModelIndexList &indexes, bool filesOnly=false) const;
    QList<Song> songs(const QModelIndexList &indexes) const;
    QMimeData * mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*col*/, const QModelIndex &parent) override;
    QStringList mimeTypes() const override;
    void getPlaylists();
    void clear();
    bool exists(const QString &n) { return nullptr!=getPlaylist(n); }
    MirrorMenu * menu();
    static QString strippedText(QString s);
    void setMultiColumn(bool m) { multiCol=m; }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files);
    void listPlaylists();
    void playlistInfo(const QString &name) const;
    void addToPlaylist(const QString &name, const QStringList &songs, quint32 pos, quint32 size);
    void moveInPlaylist(const QString &name, const QList<quint32> &idx, quint32 pos, quint32 size);

    void addToNew();
    void addToExisting(const QString &name);
    void updated(const QModelIndex &idx);
    void playlistRemoved(quint32 key);

    // Used in Touch variant only...
    void updated();

private Q_SLOTS:
    void setPlaylists(const QList<Playlist> &playlists);
    void playlistInfoRetrieved(const QString &name, const QList<Song> &songs);
    void removedFromPlaylist(const QString &name, const QList<quint32> &positions);
    void movedInPlaylist(const QString &name, const QList<quint32> &idx, quint32 pos);
    void emitAddToExisting();
    void playlistRenamed(const QString &from, const QString &to);
    void mpdConnectionStateChanged(bool connected);
    void coverLoaded(const Song &song, int s);

private:
    void updateItemMenu(bool craete=false);
    PlaylistItem * getPlaylist(const QString &name);
    void clearPlaylists();
    quint32 allocateKey();

private:
    QIcon icn;
    bool multiCol;
    QList<PlaylistItem *> items;
    QSet<quint32> usedKeys;
    MirrorMenu *itemMenu;
    quint32 dropAdjust;
    QAction *newAction;
    QMap<int, int> alignments;
};

#endif
