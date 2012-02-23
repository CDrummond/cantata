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

#include <QtGui/QPalette>
#include <QtGui/QFont>
#include <QtGui/QIcon>
#include <QtCore/QModelIndex>
#include <QtCore/QMimeData>
#include <QtCore/QTextStream>
#include <QtCore/QSet>
#include <QtCore/QUrl>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include "playqueuemodel.h"
#include "playqueueview.h"
#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "streamfetcher.h"
#include "httpserver.h"
#include "settings.h"
#include "debugtimer.h"

static QStringList reverseList(const QStringList &orig)
{
    QStringList rev;
    foreach (const QString &s, orig) {
        rev.prepend(s);
    }
    return rev;
}

static QString unencodeUrl(QString u)
{
    return u.replace("%20", " ").replace("%5C", "\\");
}

const QLatin1String PlayQueueModel::constMoveMimeType("cantata/move");
const QLatin1String PlayQueueModel::constFileNameMimeType("cantata/filename");
const QLatin1String PlayQueueModel::constUriMimeType("text/uri-list");

void PlayQueueModel::encode(QMimeData &mimeData, const QString &mime, const QStringList &values)
{
    QByteArray encodedData;
    QTextStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QString &v, values) {
        stream << v << endl;
    }

    mimeData.setData(mime, encodedData);
}

QStringList PlayQueueModel::decode(const QMimeData &mimeData, const QString &mime)
{
    QByteArray encodedData=mimeData.data(mime);
    QTextStream stream(&encodedData, QIODevice::ReadOnly);
    QStringList rv;

    while (!stream.atEnd()) {
        rv.append(stream.readLine().remove('\n'));
    }
    return rv;
}

PlayQueueModel::PlayQueueModel(QObject *parent)
    : QAbstractTableModel(parent)
    , currentSongId(-1)
    , mpdState(MPDStatus::State_Inactive)
    , grouped(false)
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
    if (Qt::Horizontal==orientation) {
        if (Qt::DisplayRole==role) {
            switch (section) {
            case COL_STATUS: return QString();
            #ifdef ENABLE_KDE_SUPPORT
            case COL_TITLE:  return i18n("Title");
            case COL_ARTIST: return i18n("Artist");
            case COL_ALBUM:  return i18n("Album");
            case COL_TRACK:  return i18n("#");
            case COL_LENGTH: return i18n("Length");
            case COL_DISC:   return i18n("Disc");
            case COL_YEAR:   return i18n("Year");
            case COL_GENRE:  return i18n("Genre");
            #else
            case COL_TITLE:  return tr("Title");
            case COL_ARTIST: return tr("Artist");
            case COL_ALBUM:  return tr("Album");
            case COL_TRACK:  return tr("#");
            case COL_LENGTH: return tr("Length");
            case COL_DISC:   return tr("Disc");
            case COL_YEAR:   return tr("Year");
            case COL_GENRE:  return tr("Genre");
            #endif
            default:         break;
            }
        } else if (Qt::TextAlignmentRole==role) {
            switch (section) {
            case COL_TITLE:
            case COL_ARTIST:
            case COL_ALBUM:
            case COL_GENRE:
            default:
                return int(Qt::AlignVCenter|Qt::AlignLeft);
            case COL_STATUS:
            case COL_TRACK:
            case COL_LENGTH:
            case COL_DISC:
            case COL_YEAR:
                return int(Qt::AlignVCenter|Qt::AlignRight);
            }
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
    return grouped ? 1 : COL_COUNT;
}

QVariant PlayQueueModel::data(const QModelIndex &index, int role) const
{

    if (!index.isValid() || index.row() >= songs.size()) {
        return QVariant();
    }

    // Mark background of song currently being played

//     if (role == Qt::BackgroundRole && songs.at(index.row()).id == currentSongId) {
//         QPalette palette;
//         QColor col(palette.color(QPalette::Highlight));
//         col.setAlphaF(0.2);
//         return QVariant(col);
//     }

    switch (role) {
    case PlayQueueView::Role_Key:
        return songs.at(index.row()).key;
//     case PlayQueueView::Role_Artist:
//         return songs.at(index.row()).artist;
    case PlayQueueView::Role_AlbumArtist:
        return songs.at(index.row()).albumArtist();
    case PlayQueueView::Role_Album: {
        const Song &song = songs.at(index.row());
        return song.album.isEmpty() && !song.name.isEmpty() && (song.file.isEmpty() || song.file.contains("://"))
                ? song.name : song.album;
    }
    case PlayQueueView::Role_Title: {
        const Song &song = songs.at(index.row());
        if (!song.albumartist.isEmpty() && song.albumartist != song.artist) {
            return song.title + " - " + song.artist;
        }
        return song.title;
    }
    case PlayQueueView::Role_Track:
        return songs.at(index.row()).track;
    case PlayQueueView::Role_Duration:
        return songs.at(index.row()).time;
    case PlayQueueView::Role_StatusIcon:
        if (songs.at(index.row()).id == currentSongId) {
            switch (mpdState) {
            case MPDStatus::State_Inactive:
            case MPDStatus::State_Stopped: return QIcon::fromTheme("media-playback-stop");
            case MPDStatus::State_Playing: return QIcon::fromTheme("media-playback-start");
            case MPDStatus::State_Paused:  return QIcon::fromTheme("media-playback-pause");
            }
        }
        return QVariant();
    case Qt::FontRole:
        if (songs.at(index.row()).id == currentSongId) {
            QFont font;
            font.setBold(true);
            return font;
        }
        break;
    case Qt::BackgroundRole:
        if (songs.at(index.row()).id == currentSongId) {
            QColor col(QPalette().color(QPalette::Highlight));
            col.setAlphaF(0.2);
            return QVariant(col);
        }
        break;
    case Qt::DisplayRole: {
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
        break;
    }
    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case COL_TITLE:
        case COL_ARTIST:
        case COL_ALBUM:
        case COL_GENRE:
        default:
            return int(Qt::AlignVCenter|Qt::AlignLeft);
        case COL_STATUS:
        case COL_TRACK:
        case COL_LENGTH:
        case COL_DISC:
        case COL_YEAR:
            return int(Qt::AlignVCenter|Qt::AlignRight);
        }
    case Qt::DecorationRole:
        if (COL_STATUS==index.column() && songs.at(index.row()).id == currentSongId) {
            switch (mpdState) {
            case MPDStatus::State_Inactive:
            case MPDStatus::State_Stopped: return QIcon::fromTheme("media-playback-stop");
            case MPDStatus::State_Playing: return QIcon::fromTheme("media-playback-start");
            case MPDStatus::State_Paused:  return QIcon::fromTheme("media-playback-pause");
            }
        }
        break;
    case Qt::SizeHintRole:
        return QSize(18, 18);
    default:
        break;
    }

    return QVariant();
}

Qt::DropActions PlayQueueModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlayQueueModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    } else {
        return Qt::ItemIsDropEnabled;
    }
}

/**
 * @return A QStringList with the mimetypes we support
 */
QStringList PlayQueueModel::mimeTypes() const
{
    QStringList types;
    types << constMoveMimeType;
    types << constFileNameMimeType;
    if (MPDConnection::self()->isLocal() || HttpServer::self()->isAlive()) {
        types << constUriMimeType;
    }
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
            if (rows.contains(index.row())) {
                continue;
            }

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
        addItems(reverseList(decode(*data, constFileNameMimeType)), row);
        return true;
    } else if(data->hasFormat(constUriMimeType)/* && MPDConnection::self()->isLocal()*/) {
        QStringList orig=reverseList(decode(*data, constUriMimeType));
        QStringList useable;
        bool haveHttp=HttpServer::self()->isAlive();
        bool alwaysUseHttp=haveHttp && Settings::self()->alwaysUseHttp();
        bool mpdLocal=MPDConnection::self()->isLocal();
        bool allowLocal=haveHttp || mpdLocal;

        foreach (QString u, orig) {
            if (u.startsWith("http://")) {
                useable.append(u);
            } else if (allowLocal && (u.startsWith('/') || u.startsWith("file:///"))) {
                if (u.startsWith("file://")) {
                    u=u.mid(7);
                }
                if (alwaysUseHttp || !mpdLocal) {
                    useable.append(HttpServer::self()->encodeUrl(unencodeUrl(u)));
                } else {
                    useable.append(QLatin1String("file://")+unencodeUrl(u));
                }
            }
        }
        if (useable.count()) {
            addItems(useable, row);
            return true;
        }
    }

    return false;
}

void PlayQueueModel::addItems(const QStringList &items, int row)
{
    bool haveHttp=false;

    foreach (const QString &f, items) {
        QUrl u(f);

        if (u.scheme()=="http") {
            haveHttp=true;
            break;
        }
    }

    if (haveHttp) {
        fetcher->get(items, row);
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
    return row>=songs.size() ? -1 : songs.at(row).id;
}

qint32 PlayQueueModel::getPosByRow(qint32 row) const
{
    return row>=songs.size() ? -1 : songs.at(row).pos;
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
    return row>=songs.size() ? Song() : songs.at(row);
}

void PlayQueueModel::updateCurrentSong(quint32 id)
{
    qint32 oldIndex = -1;

    oldIndex = currentSongId;
    currentSongId = id;

    if (-1!=oldIndex) {
        emit dataChanged(index(getRowById(oldIndex), 0), index(getRowById(oldIndex), 2));
    }

    emit dataChanged(index(getRowById(currentSongId), 0), index(getRowById(currentSongId), 2));
}

void PlayQueueModel::clear()
{
    beginResetModel();
    songs=QList<Song>();
    endResetModel();
}

void PlayQueueModel::setState(MPDStatus::State st)
{
    if (st!=mpdState) {
        mpdState=st;
        if (-1!=currentSongId) {
            emit dataChanged(index(getRowById(currentSongId), 0), index(getRowById(currentSongId), 2));
        }
    }
}

void PlayQueueModel::setGrouped(bool g)
{
    if (g==grouped) {
        return;
    }
    grouped=g;
    if (grouped && songs.count()) {
        updatePlaylist(songs);
    }
}

void PlayQueueModel::updatePlaylist(const QList<Song> &songs)
{
    TF_DEBUG
    beginResetModel();
    QMap<QString, unsigned short> albums;
    QMap<QString, unsigned short> artists;
    if (HttpServer::self()->isAlive()) {
        QList<Song> songList;
        foreach (const Song &s, songs) {
            Song song(s);

            if (grouped) {
                QMap<QString, unsigned short>::ConstIterator al=albums.find(song.album);
                QMap<QString, unsigned short>::ConstIterator ar=artists.find(song.albumArtist());
                song.key=0;

                if (al==albums.end()) {
                    song.key+=albums.size()&0xFFFF;
                    albums.insert(song.album, albums.size());
                } else {
                    song.key+=al.value()&0xFFFF;
                }
                if (ar==artists.end()) {
                    song.key=(artists.size()&0xFFFF)<<16;
                    artists.insert(song.albumArtist(), artists.size());
                } else {
                    song.key=(ar.value()&0xFFFF)<<16;
                }
            }

            if (song.file.startsWith("http") && HttpServer::self()->isOurs(song.file)) {
                Song mod=HttpServer::self()->decodeUrl(song.file);
                if (mod.title.isEmpty()) {
                    songList.append(song);
                } else {
                    song.id=s.id;
                    songList.append(song);
                }
            } else {
                songList.append(song);
            }
        }
        this->songs = songList;
    } else {
        if (grouped) {
            QList<Song> songList;
            foreach (const Song &s, songs) {
                Song song(s);
                QMap<QString, unsigned short>::ConstIterator al=albums.find(song.album);
                QMap<QString, unsigned short>::ConstIterator ar=artists.find(song.albumArtist());
                song.key=0;

                if (al==albums.end()) {
                    song.key+=albums.size()&0xFFFF;
                    albums.insert(song.album, albums.size());
                } else {
                    song.key+=al.value()&0xFFFF;
                }
                if (ar==artists.end()) {
                    song.key=(artists.size()&0xFFFF)<<16;
                    artists.insert(song.albumArtist(), artists.size());
                } else {
                    song.key=(ar.value()&0xFFFF)<<16;
                }
                songList.append(song);
            }
            this->songs = songList;
        } else {
            this->songs = songs;
        }
    }
    endResetModel();
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
