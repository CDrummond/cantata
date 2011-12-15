/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtGui/QPalette>
#include <QtGui/QFont>
#include <QtCore/QModelIndex>
#include <QtCore/QMimeData>
#include <QtCore/QTextStream>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "playqueuemodel.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "streamfetcher.h"

const QLatin1String PlayQueueModel::constMoveMimeType("cantata/move");
const QLatin1String PlayQueueModel::constFileNameMimeType("cantata/filename");

void PlayQueueModel::encode(QMimeData &mimeData, const QString &mime, const QStringList &values)
{
    QByteArray encodedData;
    QTextStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QString &v, values) {
        stream << v << endl;
    }

    mimeData.setData(mime, encodedData);
}

QStringList PlayQueueModel::decode(const QMimeData &mimeData, const QString &mime, bool rev)
{
    QByteArray encodedData=mimeData.data(mime);
    QTextStream stream(&encodedData, QIODevice::ReadOnly);
    QStringList rv;

    while (!stream.atEnd()) {
        rev ? rv.prepend(stream.readLine().remove('\n')) : rv.append(stream.readLine().remove('\n'));
    }
    return rv;
}

PlayQueueModel::PlayQueueModel(QObject *parent)
    : QAbstractTableModel(parent),
      song_id(-1)
{
    fetcher=new StreamFetcher(this);
    connect(this, SIGNAL(modelReset()), this, SLOT(playListReset()));
    connect(fetcher, SIGNAL(result(const QStringList &, int)), SLOT(addFiles(const QStringList &, int)));
}

PlayQueueModel::~PlayQueueModel()
{
}

QVariant PlayQueueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
#ifdef ENABLE_KDE_SUPPORT
            case COL_TITLE:
                return i18n("Title");
            case COL_ARTIST:
                return i18n("Artist");
            case COL_ALBUM:
                return i18n("Album");
            case COL_TRACK:
                return i18n("Track");
            case COL_LENGTH:
                return i18n("Length");
            case COL_DISC:
                return i18n("Disc");
            case COL_YEAR:
                return i18n("Year");
            case COL_GENRE:
                return i18n("Genre");
#else
            case COL_TITLE:
                return tr("Title");
            case COL_ARTIST:
                return tr("Artist");
            case COL_ALBUM:
                return tr("Album");
            case COL_TRACK:
                return tr("Track");
            case COL_LENGTH:
                return tr("Length");
            case COL_DISC:
                return tr("Disc");
            case COL_YEAR:
                return tr("Year");
            case COL_GENRE:
                return tr("Genre");
#endif
            default:
                break;
            }
        } else if (role == Qt::TextAlignmentRole) {
            return section<COL_TRACK ? Qt::AlignLeft : Qt::AlignRight;
        }
    }

    return QVariant();
}

int PlayQueueModel::rowCount(const QModelIndex &) const
{
    return songs.size();
}

int PlayQueueModel::columnCount(const QModelIndex &) const
{
    return COL_COUNT;
}

QVariant PlayQueueModel::data(const QModelIndex &index, int role) const
{

    if (!index.isValid())
        return QVariant();

    if (index.row() >= songs.size())
        return QVariant();

    // Mark background of song currently being played

    if (role == Qt::BackgroundRole && songs.at(index.row()).id == song_id) {
        QPalette palette;
        QColor col(palette.color(QPalette::Highlight));
        col.setAlphaF(0.2);
        return QVariant(col);
    }

    if (role == Qt::FontRole && songs.at(index.row()).id == song_id) {
        QFont font;
        font.setBold(true);
        return font;
    }

    if (role == Qt::DisplayRole) {
        const Song &song = songs.at(index.row());
        switch (index.column()) {
        case COL_TITLE:
            return song.title.isEmpty() ? song.file : song.title;
        case COL_ARTIST:
            return song.artist;
        case COL_ALBUM:
            return song.album.isEmpty() && !song.name.isEmpty() && (song.file.isEmpty() || song.file.contains("://"))
                    ? song.name : song.album;
        case COL_TRACK:
            if (song.track <= 0)
                return QVariant();
            return song.track;
        case COL_LENGTH:
            return Song::formattedTime(song.time);
        case COL_DISC:
            if (song.disc <= 0)
                return QVariant();
            return song.disc;
        case COL_YEAR:
            if (song.year <= 0)
                return QVariant();
            return song.year;
        case COL_GENRE:
            return song.genre;
        default:
            break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        return index.column()<COL_TRACK || COL_GENRE==index.column() ? Qt::AlignLeft : Qt::AlignRight;
    }

    return QVariant();
}

Qt::DropActions PlayQueueModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlayQueueModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::ItemIsDropEnabled;
}

/**
 * @return A QStringList with the mimetypes we support
 */
QStringList PlayQueueModel::mimeTypes() const
{
    QStringList types;
    types << constMoveMimeType;
    types << constFileNameMimeType;
    return types;
}

/**
 * Convert the data at indexes into mimedata ready for transport
 *
 * @param indexes The indexes to pack into mimedata
 * @return The mimedata
 */
QMimeData *PlayQueueModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList positions;
    QStringList filenames;
    /*
     * Loop over all our indexes. However we have rows*columns indexes
     * We pack per row so ingore the columns
     */
    QList<int> rows;
    foreach(QModelIndex index, indexes) {
        if (index.isValid()) {
            if (rows.contains(index.row()))
                continue;

            positions.append(QString::number(getPosByRow(index.row())));
            rows.append(index.row());
            filenames.append(songs.at(index.row()).file);
        }
    }

    encode(*mimeData, constMoveMimeType, positions);
    encode(*mimeData, constFileNameMimeType, filenames);

    return mimeData;
}

/**
 * Act on mime data that is dropped in our model
 *
 * @param data The actual data that is dropped
 * @param action The action. This could mean drop/copy etc
 * @param row The row where it is dropper
 * @param column The column where it is dropper
 * @param parent The parent of where we have dropped it
 *
 * @return bool if we accest the drop
 */
bool PlayQueueModel::dropMimeData(const QMimeData *data,
                                      Qt::DropAction action, int row, int /*column*/, const QModelIndex & /*parent*/)
{
    if (Qt::IgnoreAction==action) {
        return true;
    }

    if (data->hasFormat(constMoveMimeType)) {
        //Act on internal moves
        QStringList positions=decode(*data, constMoveMimeType);
        QList<quint32> items;

        foreach (const QString &s, positions) {
            items.append(s.toUInt());
        }

        if (row < 0) {
            emit moveInPlaylist(items, songs.size(), songs.size());
        } else {
            emit moveInPlaylist(items, row, songs.size());
        }

        return true;
    } else if (data->hasFormat(constFileNameMimeType)) {
        //Act on moves from the music library and dir view
        addItems(decode(*data, constFileNameMimeType, true), row);
        return true;
    }

    return false;
}

void PlayQueueModel::addItems(const QStringList &items, int row)
{
    bool haveHttp=false;

    if (row<0) {
        row=songs.size();
    }

    foreach (const QString &f, items) {
        QUrl u(f);

        if (u.scheme()=="http") {
            haveHttp=true;
            break;
        }
    }

    if (haveHttp) {
        QList<QUrl> urls;
        foreach (const QString &f, items) {
            urls << QUrl(f);
        }

        fetcher->get(urls, 0);
    } else {
        addFiles(items, row);
    }
}

void PlayQueueModel::addFiles(const QStringList &filenames, int row)
{
    //Check for empty playlist
    if (songs.size() == 1 && songs.at(0).artist.isEmpty() && songs.at(0).album.isEmpty() && songs.at(0).title.isEmpty()) {
        emit filesAddedInPlaylist(filenames, 0, 0);
    } else {
        if (row < 0) {
            emit filesAddedInPlaylist(filenames, songs.size(), songs.size());
        } else {
            emit filesAddedInPlaylist(filenames, row, songs.size());
        }
    }
}

qint32 PlayQueueModel::getIdByRow(qint32 row) const
{
    if (songs.size() <= row) {
        return -1;
    }

    return songs.at(row).id;
}

qint32 PlayQueueModel::getPosByRow(qint32 row) const
{
    if (songs.size() <= row) {
        return -1;
    }

    return songs.at(row).pos;
}

qint32 PlayQueueModel::getRowById(qint32 id) const
{
    for (int i = 0; i < songs.size(); i++) {
        if (songs.at(i).id == id) {
            return i;
        }
    }

    return -1;
}

Song PlayQueueModel::getSongByRow(const qint32 row) const
{
    if (songs.size() <= row) {
        return Song();
    }

    return songs.at(row);
}

void PlayQueueModel::updateCurrentSong(quint32 id)
{
    qint32 oldIndex = -1;

    oldIndex = song_id;
    song_id = id;

    if (oldIndex != -1)
        emit dataChanged(index(getRowById(oldIndex), 0), index(getRowById(oldIndex), 2));

    emit dataChanged(index(getRowById(song_id), 0), index(getRowById(song_id), 2));
}

void PlayQueueModel::clear()
{
    songs=QList<Song>();
    reset();
}

void PlayQueueModel::updatePlaylist(const QList<Song> &songs)
{
    this->songs = songs;
    reset();
}

/**
 * Act on a resetted playlist.
 *
 * --Update stats
 */
void PlayQueueModel::playListReset()
{
    playListStats();
}

/**
 * Set all the statistics to the stats singleton
 */
void PlayQueueModel::playListStats()
{
    MPDStats * const stats = MPDStats::self();
    QSet<QString> artists;
    QSet<QString> albums;
    quint32 time = 0;

    //If playlist is empty return empty stats
    if (songs.size() == 1 && songs.at(0).artist.isEmpty() && songs.at(0).album.isEmpty() && songs.at(0).title.isEmpty()) {
        stats->acquireWriteLock();
        stats->setPlaylistArtists(0);
        stats->setPlaylistAlbums(0);
        stats->setPlaylistSongs(0);
        stats->setPlaylistTime(0);
        stats->releaseWriteLock();
        emit playListStatsUpdated();
        return;
    }

    //Loop over all songs
    foreach(const Song &song, songs) {
        artists.insert(song.artist);
        albums.insert(song.album);
        time += song.time;
    }

    stats->acquireWriteLock();
    stats->setPlaylistArtists(artists.size());
    stats->setPlaylistAlbums(albums.size());
    stats->setPlaylistSongs(songs.size());
    stats->setPlaylistTime(time);
    stats->releaseWriteLock();
    emit playListStatsUpdated();
}

QSet<qint32>  PlayQueueModel::getSongIdSet()
{
    QSet<qint32> ids;

    foreach(const Song &song, songs) {
        ids << song.id;
    }

    return ids;
}
