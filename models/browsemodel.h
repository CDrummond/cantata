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

#ifndef BROWSE_MODEL_H
#define BROWSE_MODEL_H

#include "actionmodel.h"
#include "mpd-interface/song.h"
#include "support/utils.h"
#include "support/icon.h"
#include <QMap>

struct MPDStatsValues;

class BrowseModel : public ActionModel
{
    Q_OBJECT

public:

    class FolderItem;
    class Item
    {
    public:
        Item(FolderItem *p=nullptr)
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
        TrackItem(const Song &s, FolderItem *p=nullptr)
            : Item(p)
            , song(s) { }
        ~TrackItem() override { }

        QString getText() const override { return song.trackAndTitleStr(); }
        QString getSubText() const override { return Song::Playlist==song.type || 0==song.time ? QString() : Utils::formatTime(song.time, true); }
        const Song & getSong() const { return song; }

    private:
        Song song;
    };

    class FolderItem : public Item
    {
    public:
        enum State {
            State_Initial,
            State_Fetching,
            State_Fetched
        };

        FolderItem(const QString &n, const QString &pth, FolderItem *p=nullptr)
            : Item(p), name(n), path(pth), state(State_Initial) { }
        ~FolderItem() override { }

        void clear() { qDeleteAll(children); children.clear(); state=State_Initial; }
        const QList<Item *> getChildren() const { return children; }
        int getChildCount() const override { return children.count();}
        bool isFolder() const override { return true; }
        void add(Item *i);
        QString getText() const override { return name; }
        QString getSubText() const override { return QString(); }
        const QString & getPath() const { return path; }
        bool canFetchMore() const { return State_Initial==state; }
        bool isFetching() const { return State_Fetching==state; }
        void setState(State s) { state=s; }
        QStringList allEntries(bool allowPlaylists) const;

    private:
        QString name;
        QString path;
        QList<Item *> children;
        State state;
    };

    BrowseModel(QObject *p);

    QString name() const;
    QString title() const;
    QString descr() const;
    const QIcon & icon() const { return icn; }
    void clear();
    void load();
    bool isEnabled() const { return enabled; }
    void setEnabled(bool e);
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
    QIcon icn;
    FolderItem *root;
    QMap<QString, FolderItem *> folderIndex;
    bool enabled;
    time_t dbVersion;
};

#endif
