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
#include "groupedview.h"
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

QString PlayQueueModel::headerText(int col)
{
    switch (col) {
    case COL_STATUS: return QString();
    #ifdef ENABLE_KDE_SUPPORT
    case COL_TITLE:  return i18n("Title");
    case COL_ARTIST: return i18n("Artist");
    case COL_ALBUM:  return i18n("Album");
    case COL_TRACK:  return i18nc("Track Number (#)", "#");
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
    default:         return QString();
    }
}

PlayQueueModel::PlayQueueModel(QObject *parent)
    : QAbstractTableModel(parent)
    , currentSongId(-1)
    , mpdState(MPDState_Inactive)
    , grouped(false)
    , dropAdjust(0)
{
    fetcher=new StreamFetcher(this);
    connect(this, SIGNAL(modelReset()), this, SLOT(playListStats()));
    connect(fetcher, SIGNAL(result(const QStringList &, int, bool)), SLOT(addFiles(const QStringList &, int, bool)));
    connect(this, SIGNAL(filesAddedInPlaylist(const QStringList, quint32, quint32, bool)),
            MPDConnection::self(), SLOT(addid(const QStringList, quint32, quint32, bool)));
    connect(this, SIGNAL(moveInPlaylist(const QList<quint32> &, quint32, quint32)),
            MPDConnection::self(), SLOT(move(const QList<quint32> &, quint32, quint32)));
}

PlayQueueModel::~PlayQueueModel()
{
}

QVariant PlayQueueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (Qt::Horizontal==orientation) {
        if (Qt::DisplayRole==role) {
            return headerText(section);
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
    case GroupedView::Role_IsCollection:
        return false;
    case GroupedView::Role_CollectionId:
        return 0;
    case GroupedView::Role_Key:
        return songs.at(index.row()).key;
    case GroupedView::Role_Id:
        return songs.at(index.row()).id;
    case GroupedView::Role_Song: {
        QVariant var;
        var.setValue<Song>(songs.at(index.row()));
        return var;
    }
    case GroupedView::Role_AlbumDuration: {
        const Song &first = songs.at(index.row());
        quint32 d=first.time;
        for (int i=index.row()+1; i<songs.count(); ++i) {
            const Song &song = songs.at(i);
            if (song.key!=first.key) {
                break;
            }
            d+=song.time;
        }
        if (index.row()>1) {
            for (int i=index.row()-1; i<=0; ++i) {
                const Song &song = songs.at(i);
                if (song.key!=first.key) {
                    break;
                }
                d+=song.time;
            }
        }
        return d;
    }
    case GroupedView::Role_SongCount: {
        const Song &first = songs.at(index.row());
        quint32 count=1;
        for (int i=index.row()+1; i<songs.count(); ++i) {
            const Song &song = songs.at(i);
            if (song.key!=first.key) {
                break;
            }
            count++;
        }
        if (index.row()>1) {
            for (int i=index.row()-1; i<=0; ++i) {
                const Song &song = songs.at(i);
                if (song.key!=first.key) {
                    break;
                }
                count++;
            }
        }
        return count;
    }
    case GroupedView::Role_CurrentStatus: {
        switch (mpdState) {
        case MPDState_Inactive:
        case MPDState_Stopped: return (int)GroupedView::State_Stopped;
        case MPDState_Playing: return (int)GroupedView::State_Playing;
        case MPDState_Paused:  return (int)GroupedView::State_Paused;
        }
    }
    case GroupedView::Role_Status:
        if (songs.at(index.row()).id == currentSongId) {
            switch (mpdState) {
            case MPDState_Inactive:
            case MPDState_Stopped: return (int)GroupedView::State_Stopped;
            case MPDState_Playing: return (int)GroupedView::State_Playing;
            case MPDState_Paused:  return (int)GroupedView::State_Paused;
            }
        }
        return (int)GroupedView::State_Default;
        break;
    case Qt::FontRole: {
        Song s=songs.at(index.row());

        if (s.isStream()) {
            QFont font;
            if (songs.at(index.row()).id == currentSongId) {
                font.setBold(true);
            }
            font.setItalic(true);
            return font;
        }
        else if (songs.at(index.row()).id == currentSongId) {
            QFont font;
            font.setBold(true);
            return font;
        }
        break;
    }
//     case Qt::BackgroundRole:
//         if (songs.at(index.row()).id == currentSongId) {
//             QColor col(QPalette().color(QPalette::Highlight));
//             col.setAlphaF(0.2);
//             return QVariant(col);
//         }
//         break;
    case Qt::DisplayRole: {
        const Song &song = songs.at(index.row());
        switch (index.column()) {
        case COL_TITLE:
            return song.title.isEmpty() ? song.file : song.title;
        case COL_ARTIST:
            return song.artist;
        case COL_ALBUM:
            return song.album.isEmpty() && !song.name.isEmpty() && song.isStream() ? song.name : song.album;
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
            case MPDState_Inactive:
            case MPDState_Stopped: return QIcon::fromTheme("media-playback-stop");
            case MPDState_Playing: return QIcon::fromTheme("media-playback-start");
            case MPDState_Paused:  return QIcon::fromTheme("media-playback-pause");
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

bool PlayQueueModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (GroupedView::Role_DropAdjust==role) {
        dropAdjust=value.toUInt();
        return true;
    } else {
        return QAbstractTableModel::setData(index, value, role);
    }
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

            positions.append(QString::number(index.row())); // getPosByRow(index.row())));
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

    row+=dropAdjust;
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
        addItems(reverseList(decode(*data, constFileNameMimeType)), row, false);
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
            addItems(useable, row, false);
            return true;
        }
    }
    return false;
}

void PlayQueueModel::addItems(const QStringList &items, int row, bool replace)
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
        fetcher->get(items, row, replace);
    } else {
        addFiles(items, row, replace);
    }
}

void PlayQueueModel::addFiles(const QStringList &filenames, int row, bool replace)
{
    //Check for empty playlist
    if (replace || (songs.size() == 1 && songs.at(0).artist.isEmpty() && songs.at(0).album.isEmpty() && songs.at(0).title.isEmpty())) {
        emit filesAddedInPlaylist(filenames, 0, 0, replace);
    } else {
        if (row < 0) {
            emit filesAddedInPlaylist(filenames, songs.size(), songs.size(), false);
        } else {
            emit filesAddedInPlaylist(filenames, row, songs.size(), false);
        }
    }
}

qint32 PlayQueueModel::getIdByRow(qint32 row) const
{
    return row>=songs.size() ? -1 : songs.at(row).id;
}

// qint32 PlayQueueModel::getPosByRow(qint32 row) const
// {
//     return row>=songs.size() ? -1 : songs.at(row).pos;
// }

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
        emit dataChanged(index(getRowById(oldIndex), 0), index(getRowById(oldIndex), columnCount(QModelIndex())-1));
    }

    emit dataChanged(index(getRowById(currentSongId), 0), index(getRowById(currentSongId), columnCount(QModelIndex())-1));
}

void PlayQueueModel::clear()
{
    beginResetModel();
    songs=QList<Song>();
    endResetModel();
}

void PlayQueueModel::setState(MPDState st)
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
    grouped=g;
}

// Update playqueue with contents returned from MPD.
void PlayQueueModel::updatePlaylist(const QList<Song> &songList)
{
    TF_DEBUG
    QSet<qint32> newIds;
    foreach (const Song &s, songList) {
        newIds.insert(s.id);
    }

    if (songs.isEmpty() || songList.isEmpty()) {
        beginResetModel();
        songs=songList;
        ids=newIds;
        endResetModel();
    } else {
        QSet<QString> artists;
        QSet<QString> albums;
        quint32 time = 0;

        QSet<qint32> removed=ids-newIds;
        foreach (qint32 id, removed) {
            qint32 row=getRowById(id);
            if (row!=-1) {
                beginRemoveRows(QModelIndex(), row, row);
                songs.removeAt(row);
                endRemoveRows();
            }
        }
        for (qint32 i=0; i<songList.count(); ++i) {
            Song s=songList.at(i);
            bool newSong=i>=songs.count();
            Song curentSongAtPos=newSong ? Song() : songs.at(i);
            bool isEmpty=s.isEmpty();

            if (newSong || s.id!=curentSongAtPos.id) {
                qint32 existingPos=newSong ? -1 : getRowById(s.id);
                if (-1==existingPos) {
                    beginInsertRows(QModelIndex(), i, i);
                    songs.insert(i, s);
                    endInsertRows();
                } else {
                    beginMoveRows(QModelIndex(), existingPos, existingPos, QModelIndex(), i>existingPos ? i+1 : i);
                    Song old=songs.takeAt(existingPos);
//                     old.pos=s.pos;
                    songs.insert(i, isEmpty ? old : s);
                    endMoveRows();
                }
            } else if (isEmpty) {
                s=curentSongAtPos;
            } else {
                songs.replace(i, s);
            }

            artists.insert(s.artist);
            albums.insert(s.album);
            time += s.time;
        }

        ids=newIds;
        emit statsUpdated(artists.size(), albums.size(), songs.size(), time);
    }
}

void PlayQueueModel::playListStats()
{
    QSet<QString> artists;
    QSet<QString> albums;
    quint32 time = 0;

    //Loop over all songs
    foreach(const Song &song, songs) {
        artists.insert(song.artist);
        albums.insert(song.album);
        time += song.time;
    }

    emit statsUpdated(artists.size(), albums.size(), songs.size(), time);
}

QSet<qint32>  PlayQueueModel::getSongIdSet()
{
    QSet<qint32> ids;

    foreach(const Song &song, songs) {
        ids << song.id;
    }

    return ids;
}
