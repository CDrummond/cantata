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

#ifndef PLAYQUEUEMODEL_H
#define PLAYQUEUEMODEL_H

#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/song.h"
#include "mpd-interface/mpdstatus.h"
#include "config.h"
#include <QStringList>
#include <QAbstractItemModel>
#include <QList>
#include <QSet>
#include <QStack>
#include <QMap>

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
        COL_ORIGYEAR,
        COL_GENRE,
        COL_PRIO,
        COL_COMPOSER,
        COL_PERFORMER,
        COL_RATING,
        COL_FILENAME,
        COL_PATH,

        COL_COUNT
    };

    struct UndoItem
    {
        bool operator==(const UndoItem &o) const { return priority==o.priority && files==o.files; }
        bool operator!=(const UndoItem &o) const { return !(*this==o); }
        QStringList files;
        QList<quint8> priority;
    };

    static const QLatin1String constMoveMimeType;
    static const QLatin1String constFileNameMimeType;
    static const QLatin1String constUriMimeType;
    static QSet<QString> constFileExtensions;

    static void encode(QMimeData &mimeData, const QString &mime, const QStringList &values);
    static void encode(QMimeData &mimeData, const QString &mime, const QList<quint32> &values);
    static QStringList decode(const QMimeData &mimeData, const QString &mime);
    static QList<quint32> decodeInts(const QMimeData &mimeData, const QString &mime);
    static QString headerText(int col);

    static PlayQueueModel * self();

    PlayQueueModel(QObject *parent = nullptr);
    ~PlayQueueModel() override;
    QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &idx) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override { return COL_COUNT; }
    QVariant data(const QModelIndex &, int) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void updateCurrentSong(quint32 id);
    qint32 getIdByRow(qint32 row) const;
    qint32 getSongId(const QString &file) const;
//     qint32 getPosByRow(qint32 row) const;
    qint32 getRowById(qint32 id) const;
    Song getSongByRow(const qint32 row) const;
    Song getSongById(qint32 id) const;
    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    QStringList filenames();
    void clear();
    qint32 currentSong() const { return currentSongId; }
    qint32 currentSongRow() const;
    void setState(MPDState st);
    void update(const QList<Song> &songList, bool isComplete);
    void setStopAfterTrack(qint32 track);
    void clearStopAfterTrack() { setStopAfterTrack(-1); }
    bool removeCantataStreams(bool cdOnly=false);
    void removeAll();
    void remove(const QList<int> &rowsToRemove);
    void crop(const QList<int> &rowsToKeep);
    void setRating(const QList<int> &rows, quint8 rating) const;
    Action * shuffleAct() { return shuffleAction; }
    Action * removeDuplicatesAct() { return removeDuplicatesAction; }
    Action * sortAct() { return sortAction; }
    Action * undoAct() { return undoAction; }
    Action * redoAct() { return redoAction; }
    void enableUndo(bool e);
    bool lastCommandWasUnodOrRedo() const { return Cmd_Other!=lastCommand; }
    qint32 totalTime() const { return time; }
    void remove(const QList<Song> &rem);

private:
    void saveHistory(const QList<Song> &prevList);
    void controlActions();
    void addSortAction(const QString &name, const QString &key);
    QStringList parseUrls(const QStringList &urls);

public Q_SLOTS:
    void load(const QStringList &urls, int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false);
    void addItems(const QStringList &items, int row, int action, quint8 priority, bool decreasePriority);
    void addItems(const QStringList &items, int action, quint8 priority, bool decreasePriority) { addItems(items, -1, action, priority, decreasePriority); }
    void addFiles(const QStringList &filenames, int row, int action, quint8 priority, bool decreasePriority=false);
    void prioritySet(const QMap<qint32, quint8> &prio);
    void stats();
    void cancelStreamFetch();
    void shuffleAlbums();
    void playSong(const QString &file);

private Q_SLOTS:
    void sortBy();
    void stopAfterCurrentChanged(bool afterCurrent);
    void updateDetails(const QList<Song> &updated);
    void undo();
    void redo();
    void removeDuplicates();
    void ratingResult(const QString &file, quint8 r);
    void stickerDbChanged();

Q_SIGNALS:
    void stop(bool afterCurrent);
    void clearStopAfter();
    void filesAdded(const QStringList filenames, const quint32 row, const quint32 size, int action, quint8 priority, bool decreasePriority);
    void populate(const QStringList &items, const QList<quint8> &priority);
    void move(const QList<quint32> &items, const quint32 row, const quint32 size);
    void setOrder(const QList<quint32> &items);
    void getRating(const QString &file) const;
    void setRating(const QStringList &files, quint8 rating) const;
    void statsUpdated(int songs, quint32 time);
    void fetchingStreams();
    void streamsFetched();
    void removeSongs(const QList<qint32> &items);
    void updateCurrent(const Song &s);
    void streamFetchStatus(const QString &msg);
    void clearEntries();
    void addAndPlay(const QString &file);
    void startPlayingSongId(qint32 id);
    void currentSongRating(const QString &file, quint8 r);
    void error(const QString &str);

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
    quint32 dropAdjust;
    Action *removeDuplicatesAction;
    Action *undoAction;
    Action *redoAction;
    Action *shuffleAction;
    Action *sortAction;
    QMap<int, int> alignments;
};

#endif
