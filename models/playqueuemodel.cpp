/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QFont>
#include <QModelIndex>
#include <QMimeData>
#include <QTextStream>
#include <QSet>
#include <QUrl>
#include <QTimer>
#include "localize.h"
#include "playqueuemodel.h"
#include "groupedview.h"
#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "streamfetcher.h"
#include "streamsmodel.h"
#ifdef TAGLIB_FOUND
#include "httpserver.h"
#endif
#include "settings.h"
#include "icon.h"
#include "config.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#endif

const QLatin1String PlayQueueModel::constMoveMimeType("cantata/move");
const QLatin1String PlayQueueModel::constFileNameMimeType("cantata/filename");
const QLatin1String PlayQueueModel::constUriMimeType("text/uri-list");

#ifdef TAGLIB_FOUND
static bool checkExtension(const QString &file)
{
    static QSet<QString> constExtensions=QSet<QString>()
            << QLatin1String("mp3") << QLatin1String("ogg") << QLatin1String("flac") << QLatin1String("wma") << QLatin1String("m4a")
            << QLatin1String("m4b") << QLatin1String("mp4") << QLatin1String("m4p") << QLatin1String("wav") << QLatin1String("wv")
            << QLatin1String("wvp") << QLatin1String("aiff") << QLatin1String("aif") << QLatin1String("aifc") << QLatin1String("ape")
            << QLatin1String("spx") << QLatin1String("tta") << QLatin1String("mpc") << QLatin1String("mpp") << QLatin1String("mp+")
            << QLatin1String("dff") << QLatin1String("dsf");

    int pos=file.lastIndexOf('.');
    return pos>1 ? constExtensions.contains(file.mid(pos+1).toLower()) : false;
}
#endif

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
    case COL_TITLE:  return i18n("Title");
    case COL_ARTIST: return i18n("Artist");
    case COL_ALBUM:  return i18n("Album");
    case COL_TRACK:  return i18nc("Track Number (#)", "#");
    case COL_LENGTH: return i18n("Length");
    case COL_DISC:   return i18n("Disc");
    case COL_YEAR:   return i18n("Year");
    case COL_GENRE:  return i18n("Genre");
    case COL_PRIO:   return i18n("Priority");
    default:         return QString();
    }
}

PlayQueueModel::PlayQueueModel(QObject *parent)
    : QAbstractItemModel(parent)
    , currentSongId(-1)
    , currentSongRowNum(-1)
    , mpdState(MPDState_Inactive)
    , dropAdjust(0)
    , stopAfterCurrent(false)
    , stopAfterTrackId(-1)
{
    fetcher=new StreamFetcher(this);
    connect(this, SIGNAL(modelReset()), this, SLOT(stats()));
    connect(fetcher, SIGNAL(result(const QStringList &, int, bool, quint8)), SLOT(addFiles(const QStringList &, int, bool, quint8)));
    connect(fetcher, SIGNAL(result(const QStringList &, int, bool, quint8)), SIGNAL(streamsFetched()));
    connect(this, SIGNAL(filesAdded(const QStringList, quint32, quint32, bool, quint8)),
            MPDConnection::self(), SLOT(add(const QStringList, quint32, quint32, bool, quint8)));
    connect(this, SIGNAL(move(const QList<quint32> &, quint32, quint32)),
            MPDConnection::self(), SLOT(move(const QList<quint32> &, quint32, quint32)));
    connect(MPDConnection::self(), SIGNAL(prioritySet(const QList<qint32> &, quint8)), SLOT(prioritySet(const QList<qint32> &, quint8)));
    connect(MPDConnection::self(), SIGNAL(stopAfterCurrentChanged(bool)), SLOT(stopAfterCurrentChanged(bool)));
    connect(this, SIGNAL(stop(bool)), MPDConnection::self(), SLOT(stopPlaying(bool)));
    connect(this, SIGNAL(clearStopAfter()), MPDConnection::self(), SLOT(clearStopAfter()));
    connect(this, SIGNAL(removeSongs(QList<qint32>)), MPDConnection::self(), SLOT(removeSongs(QList<qint32>)));
    #ifdef ENABLE_DEVICES_SUPPORT
    connect(DevicesModel::self(), SIGNAL(invalid(QList<Song>)), SLOT(remove(QList<Song>)));
    connect(DevicesModel::self(), SIGNAL(updatedDetails(QList<Song>)), SLOT(updateDetails(QList<Song>)));
    #endif
}

PlayQueueModel::~PlayQueueModel()
{
}

QModelIndex PlayQueueModel::index(int row, int column, const QModelIndex &parent) const
{
    return hasIndex(row, column, parent) ? createIndex(row, column, (void *)&songs.at(row)) : QModelIndex();
}

QModelIndex PlayQueueModel::parent(const QModelIndex &idx) const
{
    Q_UNUSED(idx)
    return QModelIndex();
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
            case COL_PRIO:
                return int(Qt::AlignVCenter|Qt::AlignRight);
            }
        }
    }

    return QVariant();
}

int PlayQueueModel::rowCount(const QModelIndex &idx) const
{
    return idx.isValid() ? 0 : songs.size();
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
        quint16 key=songs.at(index.row()).key;
        for (int i=index.row()+1; i<songs.count(); ++i) {
            const Song &s=songs.at(i);
            if (s.key!=key) {
                return QVariant();
            }
            if (s.id==currentSongId) {
                switch (mpdState) {
                case MPDState_Inactive:
                case MPDState_Stopped: return (int)GroupedView::State_Stopped;
                case MPDState_Playing: return (int)(stopAfterCurrent ? GroupedView::State_StopAfter : GroupedView::State_Playing);
                case MPDState_Paused:  return (int)GroupedView::State_Paused;
                }
            }  else if (-1!=s.id && s.id==stopAfterTrackId) {
                return GroupedView::State_StopAfterTrack;
            }
        }
        return QVariant();
    }
    case GroupedView::Role_Status: {
        qint32 id=songs.at(index.row()).id;
        if (id==currentSongId) {
            switch (mpdState) {
            case MPDState_Inactive:
            case MPDState_Stopped: return (int)GroupedView::State_Stopped;
            case MPDState_Playing: return (int)(stopAfterCurrent ? GroupedView::State_StopAfter : GroupedView::State_Playing);
            case MPDState_Paused:  return (int)GroupedView::State_Paused;
            }
        } else if (-1!=id && id==stopAfterTrackId) {
            return GroupedView::State_StopAfterTrack;
        }
        return (int)GroupedView::State_Default;
        break;
    }
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
            return song.artist.isEmpty() ? i18n("Unknown") : song.artist;
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
        case COL_PRIO:
            return song.priority;
        default:
            break;
        }
        break;
    }
    case Qt::ToolTipRole: {
        Song s=songs.at(index.row());
        if (s.album.isEmpty() && s.isStream()) {
            return s.file;
        } else {
            return s.albumArtist()+QLatin1String("<br/>")+
                   s.album+(s.year>0 ? (QLatin1String(" (")+QString::number(s.year)+QChar(')')) : QString())+QLatin1String("<br/>")+
                   s.trackAndTitleStr(Song::isVariousArtists(s.albumArtist()))+QLatin1String("<br/>")+
                   Song::formattedTime(s.time)+QLatin1String("<br/>")+
                   (s.priority>0 ? i18n("<b>(Priority: %1)</b>").arg(s.priority)+QLatin1String("<br/>") : QString())+
                   QLatin1String("<small><i>")+s.file+QLatin1String("</i></small>");
        }
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
        case COL_PRIO:
            return int(Qt::AlignVCenter|Qt::AlignRight);
        }
    case Qt::DecorationRole:
        if (COL_STATUS==index.column()) {
            qint32 id=songs.at(index.row()).id;
            if (id==currentSongId) {
                switch (mpdState) {
                case MPDState_Inactive:
                case MPDState_Stopped: return Icon("media-playback-stop");
                case MPDState_Playing: return Icon(stopAfterCurrent ? "media-playback-stop" : "media-playback-start");
                case MPDState_Paused:  return Icon("media-playback-pause");
                }
            } else if (-1!=id && id==stopAfterTrackId) {
                return Icon("media-playback-stop");
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
        return QAbstractItemModel::setData(index, value, role);
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
    #ifdef TAGLIB_FOUND
    if (HttpServer::self()->isAlive()) {
        types << constUriMimeType;
    }
    #endif
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
            emit move(items, songs.size(), songs.size());
        } else {
            emit move(items, row, songs.size());
        }
        return true;
    } else if (data->hasFormat(constFileNameMimeType)) {
        //Act on moves from the music library and dir view
        addItems(decode(*data, constFileNameMimeType), row, false, 0);
        return true;
    }
    #ifdef TAGLIB_FOUND
    else if(data->hasFormat(constUriMimeType)/* && MPDConnection::self()->getDetails().isLocal()*/) {
        QStringList orig=decode(*data, constUriMimeType);
        QStringList useable;
        bool haveHttp=HttpServer::self()->isAlive();

        foreach (QString u, orig) {
            if (u.startsWith(QLatin1String("http://"))) {
                useable.append(u);
            } else if (haveHttp && (u.startsWith('/') || u.startsWith(QLatin1String("file://")))) {
                if (u.startsWith(QLatin1String("file://"))) {
                    u=u.mid(7);
                }
                if (checkExtension(u)) {
                    useable.append(HttpServer::self()->encodeUrl(QUrl::fromPercentEncoding(u.toUtf8())));
                }
            }
        }
        if (useable.count()) {
            addItems(useable, row, false, 0);
            return true;
        }
    }
    #endif
    return false;
}

void PlayQueueModel::addItems(const QStringList &items, int row, bool replace, quint8 priority)
{
    bool haveRadioStream=false;

    foreach (const QString &f, items) {
        QUrl u(f);

        if (u.scheme().startsWith(StreamsModel::constPrefix)) {
            haveRadioStream=true;
            break;
        }
    }

    if (haveRadioStream) {
        emit fetchingStreams();
        fetcher->get(items, row, replace, priority);
    } else {
        addFiles(items, row, replace, priority);
    }
}

void PlayQueueModel::addFiles(const QStringList &filenames, int row, bool replace, quint8 priority)
{
    //Check for empty playlist
    if (replace || songs.isEmpty()) {
        emit filesAdded(filenames, 0, 0, replace, priority);
    } else {
        if (row < 0) {
            emit filesAdded(filenames, songs.size(), songs.size(), false, priority);
        } else {
            emit filesAdded(filenames, row, songs.size(), false, priority);
        }
    }
}

void PlayQueueModel::prioritySet(const QList<qint32> &ids, quint8 priority)
{
    QSet<qint32> i=ids.toSet();
    int row=0;

    foreach (const Song &s, songs) {
        if (i.contains(s.id)) {
            s.priority=priority;
            i.remove(s.id);
            QModelIndex idx(index(row, 0));
            emit dataChanged(idx, idx);
            if (i.isEmpty()) {
                return;
            }
        }
        ++row;
    }
}

qint32 PlayQueueModel::getIdByRow(qint32 row) const
{
    return row>=songs.size() ? -1 : songs.at(row).id;
}

qint32 PlayQueueModel::getSongId(const QString &file) const
{
    foreach (const Song &s, songs) {
        if (s.file==file) {
            return s.id;
        }
    }

    return -1;
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
    return row<0 || row>=songs.size() ? Song() : songs.at(row);
}

Song PlayQueueModel::getSongById(qint32 id) const
{
    foreach (const Song &s, songs) {
        if (s.id==id) {
            return s;
        }
    }
    return Song();
}

void PlayQueueModel::updateCurrentSong(quint32 id)
{
    qint32 oldIndex = currentSongId;
    currentSongId = id;

    if (-1!=oldIndex) {
        int row=-1==currentSongRowNum ? getRowById(oldIndex) : currentSongRowNum;
        emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
    }

    currentSongRowNum=getRowById(currentSongId);
    emit dataChanged(index(currentSongRowNum, 0), index(currentSongRowNum, columnCount(QModelIndex())-1));

    if (-1!=currentSongId && stopAfterTrackId==currentSongId) {
        stopAfterTrackId=-1;
        emit stop(true);
    }
}

void PlayQueueModel::clear()
{
    beginResetModel();
    songs.clear();
    ids.clear();
    currentSongId=-1;
    currentSongRowNum=0;
    stopAfterTrackId=-1;
    endResetModel();
}

qint32 PlayQueueModel::currentSongRow() const
{
    if (-1==currentSongRowNum) {
        currentSongRowNum=getRowById(currentSongId);
    }
    return currentSongRowNum;
}

void PlayQueueModel::setState(MPDState st)
{
    if (st!=mpdState) {
        mpdState=st;
        if (-1!=currentSongId) {
            if (-1==currentSongRowNum) {
                currentSongRowNum=getRowById(currentSongId);
            }
            emit dataChanged(index(currentSongRowNum, 0), index(currentSongRowNum, 2));
        }
    }
}

// Update playqueue with contents returned from MPD.
void PlayQueueModel::update(const QList<Song> &songList)
{
    QSet<qint32> newIds;
    foreach (const Song &s, songList) {
        newIds.insert(s.id);
    }

    if (songs.isEmpty() || songList.isEmpty()) {
        beginResetModel();
        songs=songList;
        ids=newIds;
        endResetModel();
        if (songList.isEmpty()) {
            stopAfterTrackId=-1;
        }
    } else {
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
                s.key=curentSongAtPos.key;
                songs.replace(i, s);
                if (s.name!=curentSongAtPos.name || s.title!=curentSongAtPos.title || s.artist!=curentSongAtPos.artist) {
                    emit dataChanged(index(i, 0), index(i, columnCount(QModelIndex())-1));
                }
            }

            time += s.time;
        }

        if (songs.count()>songList.count()) {
            int toBeRemoved=songs.count()-songList.count();
            beginRemoveRows(QModelIndex(), songList.count(), songs.count()-1);
            for (int i=0; i<toBeRemoved; ++i) {
                songs.takeLast();
            }
            endRemoveRows();
        }

        ids=newIds;
        if (-1!=stopAfterTrackId && !ids.contains(stopAfterTrackId)) {
            stopAfterTrackId=-1;
        }
        emit statsUpdated(songs.size(), time);
    }
}

void PlayQueueModel::setStopAfterTrack(qint32 track)
{
    bool clear=track==stopAfterTrackId || (track==currentSongId && stopAfterCurrent);

    stopAfterTrackId=clear ? -1 : track;
    qint32 row=getRowById(track);
    if (-1!=row) {
        emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
    }
    if (clear) {
        emit clearStopAfter();
    } else if (stopAfterTrackId==currentSongId) {
        emit stop(true);
    }
}

void PlayQueueModel::stats()
{
    quint32 time = 0;

    //Loop over all songs
    foreach(const Song &song, songs) {
        time += song.time;
    }

    emit statsUpdated(songs.size(), time);
}

void PlayQueueModel::stopAfterCurrentChanged(bool afterCurrent)
{
    if (afterCurrent!=stopAfterCurrent) {
        stopAfterCurrent=afterCurrent;
        emit dataChanged(index(currentSongRowNum, 0), index(currentSongRowNum, columnCount(QModelIndex())-1));
    }
}

void PlayQueueModel::remove(const QList<Song> &rem)
{
    QSet<QString> s;
    QList<qint32> ids;

    foreach (const Song &song, rem) {
        s.insert(song.file);
    }

    foreach (const Song &song, songs) {
        if (s.contains(song.file)) {
            ids.append(song.id);
            s.remove(song.file);
            if (s.isEmpty()) {
                break;
            }
        }
    }

    if (!ids.isEmpty()) {
        emit removeSongs(ids);
    }
}

void PlayQueueModel::updateDetails(const QList<Song> &updated)
{
    QMap<QString, Song> songMap;
    QList<int> updatedRows;
    bool currentUpdated=false;
    Song currentSong;

    foreach (const Song &song, updated) {
        songMap[song.file]=song;
    }

    for (int i=0; i<songs.count(); ++i) {
        Song current=songs.at(i);
        if (songMap.contains(current.file)) {
            Song updatedSong=songMap[current.file];
            updatedSong.id=current.id;
            updatedSong.setKey();

            if (updatedSong.name!=current.name || updatedSong.title!=current.title || updatedSong.artist!=current.artist) {
                songs.replace(i, updatedSong);
                updatedRows.append(i);
                if (currentSongId==current.id) {
                    currentUpdated=true;
                    currentSong=updatedSong;
                }
            }

            songMap.remove(current.file);
            if (songMap.isEmpty()) {
                break;
            }
        }
    }

    if (!updatedRows.isEmpty()) {
        if (updatedRows.count()==updated.count()) {
            beginResetModel();
            endResetModel();
        } else {
            foreach (int row, updatedRows) {
                emit dataChanged(index(row, 0), index(row, columnCount(QModelIndex())-1));
            }
        }
    }

    if (currentUpdated) {
        emit updateCurrent(currentSong);
    }
}

QSet<qint32> PlayQueueModel::getSongIdSet()
{
    QSet<qint32> ids;

    foreach(const Song &song, songs) {
        ids << song.id;
    }

    return ids;
}

QStringList PlayQueueModel::filenames()
{
    QStringList names;
    foreach(const Song &song, songs) {
        names << song.file;
    }
    return names;
}
