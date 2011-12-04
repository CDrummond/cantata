/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QPalette>
#include <QModelIndex>
#include <QStringListModel>
#include <QMimeData>
#include <QSet>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "playlisttablemodel.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdstatus.h"

PlaylistTableModel::PlaylistTableModel(QObject *parent)
    : QAbstractTableModel(parent),
      song_id(-1)
{
    connect(this, SIGNAL(modelReset()), this, SLOT(playListReset()));
}

PlaylistTableModel::~PlaylistTableModel()
{
}

QVariant PlaylistTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
#ifdef ENABLE_KDE_SUPPORT
            case 0:
                return i18n("Title");
            case 1:
                return i18n("Artist");
            case 2:
                return i18n("Album");
            case 3:
                return i18n("Track");
            case 4:
                return i18n("Length");
            case 5:
                return i18n("Disc");
            case 6:
                return i18n("Year");
#else
            case 0:
                return tr("Title");
            case 1:
                return tr("Artist");
            case 2:
                return tr("Album");
            case 3:
                return tr("Track");
            case 4:
                return tr("Length");
            case 5:
                return tr("Disc");
            case 6:
                return tr("Year");
#endif
            default:
                break;
            }
        } else if (role == Qt::TextAlignmentRole) {
            return Qt::AlignLeft;
        }
    }

    return QVariant();
}

int PlaylistTableModel::rowCount(const QModelIndex &) const
{
    return songs.size();
}

int PlaylistTableModel::columnCount(const QModelIndex &) const
{
    return 7;
}

QVariant PlaylistTableModel::data(const QModelIndex &index, int role) const
{
    QPalette palette;

    if (!index.isValid())
        return QVariant();

    if (index.row() >= songs.size())
        return QVariant();

    // Mark background of song currently being played

    if (role == Qt::BackgroundRole && songs.at(index.row()).id == song_id) {
        return QVariant(palette.color(QPalette::Mid));
    }

    if (role == Qt::DisplayRole) {
        const Song &song = songs.at(index.row());
        switch (index.column()) {
        case 0:
            return song.displayTitle();
            break;
        case 1:
            return song.albumArtist();
            break;
        case 2:
            return song.album;
            break;
        case 3:
            if (song.track <= 0)
                return QVariant();
            return song.track;
            break;
        case 4:
            return Song::formattedTime(song.time);
            break;
        case 5:
            if (song.disc <= 0)
                return QVariant();
            return song.disc;
            break;
        case 6:
            if (song.year <= 0)
                return QVariant();
            return song.year;
            break;
        default:
            break;
        }
    }

    return QVariant();
}

Qt::DropActions PlaylistTableModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlaylistTableModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::ItemIsDropEnabled;
}

/**
 * @return A QStringList with the mimetypes we support
 */
QStringList PlaylistTableModel::mimeTypes() const
{
    QStringList types;
    types << "application/cantata_song_move_text";
    types << "application/cantata_songs_filename_text";
    return types;
}

/**
 * Convert the data at indexes into mimedata ready for transport
 *
 * @param indexes The indexes to pack into mimedata
 * @return The mimedata
 */
QMimeData *PlaylistTableModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    /*
     * Loop over all our indexes. However we have rows*columns indexes
     * We pack per row so ingore the columns
     */
    QList<int> rows;
    foreach(QModelIndex index, indexes) {
        if (index.isValid()) {
            if (rows.contains(index.row()))
                continue;

            QString text = QString::number(getPosByRow(index.row()));
            stream << text;
            rows.append(index.row());
        }
    }

    mimeData->setData("application/cantata_song_move_text", encodedData);
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
bool PlaylistTableModel::dropMimeData(const QMimeData *data,
                                      Qt::DropAction action, int row, int /*column*/, const QModelIndex & /*parent*/)
{
    if (action == Qt::IgnoreAction) {
        return true;
    }

    if (data->hasFormat("application/cantata_song_move_text")) {
        //Act on internal moves
        QByteArray encodedData = data->data("application/cantata_song_move_text");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QList<quint32> items;

        while (!stream.atEnd()) {
            QString text;
            stream >> text;
            items.append(text.toUInt());
        }

        if (row < 0) {
            emit moveInPlaylist(items, songs.size(), songs.size());
        } else {
            emit moveInPlaylist(items, row, songs.size());
        }

        return true;
    } else if (data->hasFormat("application/cantata_songs_filename_text")) {
        //Act on moves from the music library and dir view
        QByteArray encodedData = data->data("application/cantata_songs_filename_text");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QStringList filenames;

        QString text;
        while (!stream.atEnd()) {
            stream >> text;
            filenames << text;
        }
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

        return true;
    }

    return false;
}

qint32 PlaylistTableModel::getIdByRow(qint32 row) const
{
    if (songs.size() <= row) {
        return -1;
    }

    return songs.at(row).id;
}

qint32 PlaylistTableModel::getPosByRow(qint32 row) const
{
    if (songs.size() <= row) {
        return -1;
    }

    return songs.at(row).pos;
}

qint32 PlaylistTableModel::getRowById(qint32 id) const
{
    for (int i = 0; i < songs.size(); i++) {
        if (songs.at(i).id == id) {
            return i;
        }
    }

    return -1;
}

Song PlaylistTableModel::getSongByRow(const qint32 row) const
{
    if (songs.size() <= row) {
        return Song();
    }

    return songs.at(row);
}

void PlaylistTableModel::updateCurrentSong(quint32 id)
{
    qint32 oldIndex = -1;

    oldIndex = song_id;
    song_id = id;

    if (oldIndex != -1)
        emit dataChanged(index(getRowById(oldIndex), 0), index(getRowById(oldIndex), 2));

    emit dataChanged(index(getRowById(song_id), 0), index(getRowById(song_id), 2));
}

void PlaylistTableModel::updatePlaylist(const QList<Song> &songs)
{
    this->songs = songs;
    reset();
}

/**
 * Act on a resetted playlist.
 *
 * --Update stats
 */
void PlaylistTableModel::playListReset()
{
    playListStats();
}

/**
 * Set all the statistics to the stats singleton
 */
void PlaylistTableModel::playListStats()
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

QSet<qint32>  PlaylistTableModel::getSongIdSet()
{
    QSet<qint32> ids;

    foreach(const Song &song, songs) {
        ids << song.id;
    }

    return ids;
}
