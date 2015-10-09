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

#ifndef BROWSE_MODEL_H
#define BROWSE_MODEL_H

#include "actionmodel.h"
#include "mpd-interface/song.h"
#include "support/utils.h"
#include <QMap>
#include <sys/time.h>

struct MPDStatsValues;

class BrowseModel : public ActionModel
{
    Q_OBJECT

public:

    class FolderItem;
    class Item
    {
    public:
        Item(FolderItem *p=0)
            : parent(p), row(0) { }
        virtual ~Item() { }

        virtual int getChildCount() const { return 0; }
        virtual bool isFolder() const { return false; }
        virtual QString getText() const =0;
        virtual QString getSubText() const =0;
        FolderItem * getParent() const { return parent; }
        int getRow() const { return row; }
        void setRow(int r) { row=r; }

    private:
        FolderItem *parent;
        int row;
    };

    class TrackItem : public Item
    {
    public:
        TrackItem(const Song &s, FolderItem *p=0)
            : Item(p)
            , song(s) { }
        virtual ~TrackItem() { }

        virtual QString getText() const { return song.trackAndTitleStr(); }
        virtual QString getSubText() const { return Song::Playlist==song.type ? QString() : Utils::formatTime(song.time, true); }
        const Song & getSong() const { return song; }

    private:
        Song song;
    };

    class FolderItem : public Item
    {
    public:
        FolderItem(const QString &n, const QString &pth, FolderItem *p=0)
            : Item(p), name(n), path(pth), fetching(false) { }
        virtual ~FolderItem() { }

        void clear() { qDeleteAll(children); children.clear(); }
        const QList<Item *> getChildren() const { return children; }
        virtual int getChildCount() const { return children.count();}
        virtual bool isFolder() const { return true; }
        void add(Item *i);
        virtual QString getText() const { return name; }
        virtual QString getSubText() const { return QString(); }
        const QString & getPath() const { return path; }
        bool isFetching() const { return fetching; }
        void setFetching(bool f) { fetching=f; }
        QStringList allEntries() const;

    private:
        QString name;
        QString path;
        QList<Item *> children;
        bool fetching;
    };

    BrowseModel(QObject *p);

    void clear();
    void load();
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
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
    QList<Song> songs(const QModelIndexList &indexes, bool allowPlaylists) const;

Q_SIGNALS:
    void listFolder(const QString &path);

private Q_SLOTS:
    void connectionChanged();
    void statsUpdated(const MPDStatsValues &stats);
    void folderContents(const QString &path, const QStringList &folders, const QList<Song> &songs);

private:
    Item * toItem(const QModelIndex &index) const { return index.isValid() ? static_cast<Item*>(index.internalPointer()) : root; }

private:
    FolderItem *root;
    QMap<QString, FolderItem *> folderIndex;
    bool enabled;
    time_t dbVersion;
};

#endif
