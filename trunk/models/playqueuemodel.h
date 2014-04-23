/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 * Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
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

#ifndef PLAYQUEUEMODEL_H
#define PLAYQUEUEMODEL_H

#include "song.h"
#include "mpdstatus.h"
#include "config.h"
#include <QStringList>
#include <QAbstractItemModel>
#include <QList>
#include <QSet>
#include <QStack>

class StreamFetcher;
class Action;

class PlayQueueModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Columns
    {
        COL_TRACK,
        COL_DISC,
        COL_TITLE,
        COL_ARTIST,
        COL_ALBUM,
        COL_LENGTH,
        COL_YEAR,
        COL_GENRE,
        COL_PRIO,

        COL_COUNT
    };

    struct UndoItem
    {
        bool operator==(const UndoItem &o) const { return priority==o.priority && files==o.files; }
        bool operator!=(const UndoItem &o) const { return !(*this==o); }
        QStringList files;
        QList<quint8> priority;
    };

    #ifndef ENABLE_UBUNTU
    static const QLatin1String constMoveMimeType;
    static const QLatin1String constFileNameMimeType;
    static const QLatin1String constUriMimeType;

    static void encode(QMimeData &mimeData, const QString &mime, const QStringList &values);
    static QStringList decode(const QMimeData &mimeData, const QString &mime);
    static QString headerText(int col);
    #endif

    PlayQueueModel(QObject *parent = 0);
    ~PlayQueueModel();
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const;
    QModelIndex parent(const QModelIndex &idx) const;
    #ifndef ENABLE_UBUNTU
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    #endif
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &) const { return COL_COUNT; }
    #ifdef ENABLE_UBUNTU
    QHash<int, QByteArray> roleNames() const;
    #endif
    QVariant data(const QModelIndex &, int) const;
    #ifndef ENABLE_UBUNTU
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);
    #endif
    void updateCurrentSong(quint32 id);
    qint32 getIdByRow(qint32 row) const;
    qint32 getSongId(const QString &file) const;
//     qint32 getPosByRow(qint32 row) const;
    qint32 getRowById(qint32 id) const;
    Song getSongByRow(const qint32 row) const;
    Song getSongById(qint32 id) const;
    Qt::DropActions supportedDropActions() const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    #ifndef ENABLE_UBUNTU
    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    #endif
    QStringList filenames();
    void clear();
    qint32 currentSong() const { return currentSongId; }
    qint32 currentSongRow() const;
    void setState(MPDState st);
    void update(const QList<Song> &songList);
    void setStopAfterTrack(qint32 track);
    void clearStopAfterTrack() { setStopAfterTrack(-1); }
    bool removeCantataStreams();
    void removeAll();
    void remove(const QList<int> &rowsToRemove);
    void crop(const QList<int> &rowsToKeep);
    #ifndef ENABLE_UBUNTU
    Action * shuffleAct() { return shuffleAction; }
    Action * removeDuplicatesAct() { return removeDuplicatesAction; }
    Action * sortAct() { return sortAction; }
    Action * undoAct() { return undoAction; }
    Action * redoAct() { return redoAction; }
    #endif
    void enableUndo(bool e);
    bool lastCommandWasUnodOrRedo() const { return Cmd_Other!=lastCommand; }
    qint32 totalTime() const { return time; }

private:
    void saveHistory(const QList<Song> &prevList);
    void controlActions();
    void addSortAction(const QString &name, const QString &key);

public Q_SLOTS:
    void load(const QStringList &urls);
    void addItems(const QStringList &items, int row, bool replace, quint8 priority);
    void addItems(const QStringList &items, bool replace, quint8 priority) { addItems(items, -1, replace, priority); }
    void addFiles(const QStringList &filenames, int row, bool replace, quint8 priority);
    void prioritySet(const QList<qint32> &ids, quint8 priority);
    void stats();
    void cancelStreamFetch();
    void shuffleAlbums();
    void playSong(const QString &file);

private Q_SLOTS:
    void sortBy();
    void stopAfterCurrentChanged(bool afterCurrent);
    void remove(const QList<Song> &rem);
    void updateDetails(const QList<Song> &updated);
    void undo();
    void redo();
    void removeDuplicates();
    // Touch version...
    void setCover(const Song &song, const QImage &img, const QString &file);

Q_SIGNALS:
    void stop(bool afterCurrent);
    void clearStopAfter();
    void filesAdded(const QStringList filenames, const quint32 row, const quint32 size, int action, quint8 priority);
    void populate(const QStringList &items, const QList<quint8> &priority);
    void move(const QList<quint32> &items, const quint32 row, const quint32 size);
    void statsUpdated(int songs, quint32 time);
    void fetchingStreams();
    void streamsFetched();
    void removeSongs(const QList<qint32> &items);
    void updateCurrent(const Song &s);
    void streamFetchStatus(const QString &msg);
    void clearEntries();
    void addAndPlay(const QString &file);
    void startPlayingSongId(qint32 id);

private:
    QList<Song> songs;
    QSet<qint32> ids;
    qint32 currentSongId;
    mutable qint32 currentSongRowNum;
    quint32 time;
    StreamFetcher *fetcher;
    MPDState mpdState;
    bool stopAfterCurrent;
    qint32 stopAfterTrackId;

    enum Command
    {
        Cmd_Other,
        Cmd_Undo,
        Cmd_Redo
    };

    int undoLimit;
    bool undoEnabled;
    Command lastCommand;
    QStack<UndoItem> undoStack;
    QStack<UndoItem> redoStack;
    #ifdef ENABLE_UBUNTU
    mutable QMap<quint16, QString> covers;
    mutable QMap<quint16, Song> coverRequests;
    #else
    quint32 dropAdjust;
    Action *removeDuplicatesAction;
    Action *undoAction;
    Action *redoAction;
    Action *shuffleAction;
    Action *sortAction;
    #endif
};

#endif
