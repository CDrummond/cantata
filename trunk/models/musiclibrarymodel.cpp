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

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "albumsmodel.h"
#include "playqueuemodel.h"
#include "settings.h"
#include "config.h"
#include "covers.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "mpdconnection.h"
#include "network.h"
#include "localize.h"
#include "utils.h"
#include "icon.h"
#include <QtGui/QCommonStyle>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QStringRef>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#endif
#include "debugtimer.h"

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(MusicLibraryModel, instance)
#endif

MusicLibraryModel * MusicLibraryModel::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static MusicLibraryModel *instance=0;
    if(!instance) {
        instance=new MusicLibraryModel;
    }
    return instance;
    #endif
}

static const QLatin1String constLibraryCache("library/");
static const QLatin1String constLibraryExt(".xml");

static const QString cacheFileName()
{

    QString fileName=MPDConnection::self()->getDetails().hostname+constLibraryExt;
    fileName.replace('/', '_');
    return Network::cacheDir(constLibraryCache)+fileName;
}

MusicLibraryModel::MusicLibraryModel(QObject *parent)
    : QAbstractItemModel(parent)
    , artistImages(false)
    , rootItem(new MusicLibraryItemRoot)
{
    connect(Covers::self(), SIGNAL(artistImage(const QString &, const QImage &)),
            this, SLOT(setArtistImage(const QString &, const QImage &)));
    connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
            this, SLOT(setCover(const Song &, const QImage &, const QString &)));
}

MusicLibraryModel::~MusicLibraryModel()
{
    delete rootItem;
}

QModelIndex MusicLibraryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const MusicLibraryItem * parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<MusicLibraryItem *>(parent.internalPointer());
    }

    MusicLibraryItem * const childItem = parentItem->childItem(row);
    if (childItem) {
        return createIndex(row, column, childItem);
    }

    return QModelIndex();
}

QModelIndex MusicLibraryModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    const MusicLibraryItem * const childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem * const parentItem = childItem->parentItem();

    if (parentItem == rootItem) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

QVariant MusicLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    return Qt::Horizontal==orientation && Qt::DisplayRole==role ? rootItem->data() : QVariant();
}

int MusicLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }

    const MusicLibraryItem *parentItem;

    if (!parent.isValid()) {
        parentItem = rootItem;
    } else {
        parentItem = static_cast<MusicLibraryItem *>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int MusicLibraryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return static_cast<MusicLibraryItem *>(parent.internalPointer())->columnCount();
    } else {
        return rootItem->columnCount();
    }
}

#if !defined ENABLE_KDE_SUPPORT && !defined CANTATA_ANDROID
const QIcon & MusicLibraryModel::vaIcon()
{
    static QIcon icon;

    if (icon.isNull()) {
        icon.addFile(":va16.png");
        icon.addFile(":va22.png");
        icon.addFile(":va32.png");
        icon.addFile(":va48.png");
        icon.addFile(":va64.png");
        icon.addFile(":va128.png");
    }
    return icon;
}
#endif

QVariant MusicLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            if (artistImages) {
                return artist->cover();
            } else {
                #if defined ENABLE_KDE_SUPPORT || defined CANTATA_ANDROID
                return Icon(artist->isVarious() ? "cantata-view-media-artist-various" : "view-media-artist");
                #else
                return artist->isVarious() ? vaIcon() : Icon("view-media-artist");
                #endif
            }
        }
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return Icon(DEFAULT_ALBUM_ICON);
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song: return Icon(Song::Playlist==static_cast<MusicLibraryItemSong *>(item)->song().type ? "view-media-playlist" : "audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->itemType()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (Song::Playlist==song->song().type) {
                return song->song().title;
            }
            if (static_cast<MusicLibraryItemAlbum *>(song->parentItem())->isSingleTracks()) {
                return song->song().artistSong();
            } else {
                return song->song().trackAndTitleStr(static_cast<MusicLibraryItemArtist *>(song->parentItem()->parentItem())->isVarious() &&
                                                     !Song::isVariousArtists(song->song().artist));
            }
        } else if(MusicLibraryItem::Type_Album==item->itemType() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist:
            return 0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1<br/>1 Album", "%1<br/>%2 Albums", item->data(), item->childCount());
                    #else
                    (item->childCount()>1
                        ? tr("%1<br/>%2 Albums").arg(item->data()).arg(item->childCount())
                        : tr("%1<br/>1 Album").arg(item->data()));
                    #endif
        case MusicLibraryItem::Type_Album:
            return item->parentItem()->data()+QLatin1String("<br/>")+(0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1<br/>1 Track (%3)", "%1<br/>%2 Tracks (%3)", item->data(), item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
                    #else
                    (item->childCount()>1
                        ? tr("%1<br/>%2 Tracks (%3)").arg(item->data()).arg(item->childCount()).arg(Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
                        : tr("%1<br/>1 Track (%2)").arg(item->data()).arg(Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime())))
                    #endif
                );
        case MusicLibraryItem::Type_Song: {
            return item->parentItem()->parentItem()->data()+QLatin1String("<br/>")+item->parentItem()->data()+QLatin1String("<br/>")+
                   data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+
                   (Song::Playlist==static_cast<MusicLibraryItemSong *>(item)->song().type
                        ? QString() : Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time())+QLatin1String("<br/>"))+
                   QLatin1String("<small><i>")+static_cast<MusicLibraryItemSong *>(item)->song().file+QLatin1String("</i></small>");
        }
        default: return QVariant();
        }
    case ItemView::Role_ImageSize:
        if (MusicLibraryItem::Type_Song!=item->itemType() && !MusicLibraryItemAlbum::itemSize().isNull()) { // icon/list style view...
            return MusicLibraryItemAlbum::iconSize(true);
        } else if (MusicLibraryItem::Type_Album==item->itemType() || (artistImages && MusicLibraryItem::Type_Artist==item->itemType())) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Album") : tr("%1 Albums").arg(item->childCount());
            #endif
            break;
        case MusicLibraryItem::Type_Song:
            return Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time());
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track (%2)", "%1 Tracks (%2)", item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #else
            return 1==item->childCount() ? tr("1 Track (%1)").arg(Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
                                         : tr("%1 Tracks (%2)").arg(item->childCount()).arg(Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
        } else if (MusicLibraryItem::Type_Artist==item->itemType() && artistImages) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemArtist *>(item)->cover());
            return v;
        }
    case Qt::SizeHintRole:
        if (MusicLibraryItem::Type_Song!=item->itemType() && !MusicLibraryItemAlbum::itemSize().isNull()) {
            return MusicLibraryItemAlbum::itemSize();
        }
    default:
        return QVariant();
    }
    return QVariant();
}

void MusicLibraryModel::clear()
{
    const MusicLibraryItemRoot *oldRoot = rootItem;
    beginResetModel();
    databaseTime = QDateTime();
    rootItem = new MusicLibraryItemRoot;
    delete oldRoot;
    endResetModel();

//     emit updated(rootItem);
    AlbumsModel::self()->update(rootItem);
    emit updateGenres(QSet<QString>());
}

QModelIndex MusicLibraryModel::findSongIndex(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (MusicLibraryItem *songItem, albumItem->childItems()) {
                if (songItem->data()==s.title) {
                    return createIndex(albumItem->childItems().indexOf(songItem), 0, songItem);
                }
            }
        }
    }

    return QModelIndex();
}

const MusicLibraryItem * MusicLibraryModel::findSong(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
                if (songItem->data()==s.title) {
                    return songItem;
                }
            }
        }
    }

    return 0;
}

bool MusicLibraryModel::songExists(const Song &s) const
{
    const MusicLibraryItem *song=findSong(s);

    if (song) {
        return true;
    }

    if (!s.isVariousArtists()) {
        Song mod(s);
        mod.albumartist=i18n("Various Artists");
        if (MPDParseUtils::groupMultiple()) {
            song=findSong(mod);
            if (song) {
                Song sng=static_cast<const MusicLibraryItemSong *>(song)->song();
                if (sng.albumArtist()==s.albumArtist()) {
                    return true;
                }
            }
        }
        if (MPDParseUtils::groupSingle()) {
            mod.album=i18n("Single Tracks");

            song=findSong(mod);
            if (song) {
                Song sng=static_cast<const MusicLibraryItemSong *>(song)->song();
                if (sng.albumArtist()==s.albumArtist() && sng.album==s.album) {
                    return true;
                }
            }
        }
    }

    return false;
}

void MusicLibraryModel::addSongToList(const Song &s)
{
    databaseTime=QDateTime();
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (!artistItem) {
        beginInsertRows(QModelIndex(), rootItem->childCount(), rootItem->childCount());
        artistItem = rootItem->createArtist(s);
        endInsertRows();
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        beginInsertRows(createIndex(rootItem->childItems().indexOf(artistItem), 0, artistItem), artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        endInsertRows();
    }
    foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
        if (songItem->data()==s.title) {
            return;
        }
    }

    beginInsertRows(createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
    rootItem->addGenre(s.genre);
    endInsertRows();
}

void MusicLibraryModel::removeSongFromList(const Song &s)
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (!artistItem) {
        return;
    }
    MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
    if (!albumItem) {
        return;
    }
    MusicLibraryItem *songItem=0;
    int songRow=0;
    foreach (MusicLibraryItem *song, albumItem->childItems()) {
        if (static_cast<MusicLibraryItemSong *>(song)->song().title==s.title) {
            songItem=song;
            break;
        }
        songRow++;
    }
    if (!songItem) {
        return;
    }

    databaseTime=QDateTime();
    if (1==artistItem->childCount() && 1==albumItem->childCount()) {
        // 1 album with 1 song - so remove whole artist
        int row=rootItem->childItems().indexOf(artistItem);
        beginRemoveRows(QModelIndex(), row, row);
        rootItem->remove(artistItem);
        endRemoveRows();
        return;
    }

    if (1==albumItem->childCount()) {
        // multiple albums, but this album only has 1 song - remove album
        int row=artistItem->childItems().indexOf(albumItem);
        beginRemoveRows(createIndex(rootItem->childItems().indexOf(artistItem), 0, artistItem), row, row);
        artistItem->remove(albumItem);
        endRemoveRows();
        return;
    }

    // Just remove particular song
    beginRemoveRows(createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), songRow, songRow);
    albumItem->remove(songRow);
    endRemoveRows();
}

void MusicLibraryModel::updateSongFile(const Song &from, const Song &to)
{
    rootItem->updateSongFile(from, to);
}

void MusicLibraryModel::removeCache()
{
    //Check if dir exists
    QString cacheFile(cacheFileName());
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }
    databaseTime = QDateTime();
}

void MusicLibraryModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    rootItem->getDetails(artists, albumArtists, albums, genres);
}

void MusicLibraryModel::updateMusicLibrary(MusicLibraryItemRoot *newroot, QDateTime dbUpdate, bool fromFile)
{
    if (databaseTime >= dbUpdate) {
        delete newroot;
        return;
    }

    TF_DEBUG
    bool updatedSongs=false;
    bool needToSave=dbUpdate>databaseTime;
    bool incremental=rootItem->childCount() && newroot->childCount();
    bool needToUpdate=databaseTime.isNull();

    if (incremental && !QFile::exists(cacheFileName())) {
        incremental=false;
    }

    if (incremental) {
        updatedSongs=update(newroot->allSongs());
        delete newroot;
        databaseTime = dbUpdate;
    } else {
        const MusicLibraryItemRoot *oldRoot = rootItem;
        beginResetModel();
        databaseTime = dbUpdate;
        rootItem = newroot;
        delete oldRoot;
        endResetModel();
        updatedSongs=true;
    }

    if (updatedSongs || needToUpdate) {
        if (!fromFile && (needToSave || needToUpdate)) {
            toXML(rootItem, dbUpdate);
        }

        AlbumsModel::self()->update(rootItem);
//         emit updated(rootItem);
        emit updateGenres(rootItem->genres());
    }
}

bool MusicLibraryModel::update(const QSet<Song> &songs)
{
    QSet<Song> currentSongs=rootItem->allSongs();
    QSet<Song> updateSongs=songs;
    QSet<Song> removed=currentSongs-updateSongs;
    QSet<Song> added=updateSongs-currentSongs;

    bool updatedSongs=added.count()||removed.count();
    foreach (const Song &s, removed) {
        removeSongFromList(s);
    }
    foreach (const Song &s, added) {
        addSongToList(s);
    }
    return updatedSongs;
}

void MusicLibraryModel::setArtistImage(const QString &artist, const QImage &img)
{
    if (img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    Song song;
    song.artist=song.albumartist=artist;
    MusicLibraryItemArtist *artistItem = rootItem->artist(song, false);
    if (artistItem && static_cast<const MusicLibraryItemArtist *>(artistItem)->setCover(img)) {
        QModelIndex idx=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
        emit dataChanged(idx, idx);
    }
}

void MusicLibraryModel::setCover(const Song &song, const QImage &img, const QString &file)
{
    Q_UNUSED(file)
    if (img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    MusicLibraryItemArtist *artistItem = rootItem->artist(song, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
        if (albumItem && static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
            QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex()));
            emit dataChanged(idx, idx);
        }
    }

//     int i=0;
//     foreach (const MusicLibraryItem *artistItem, rootItem->childItems()) {
//         if (artistItem->data()==artist) {
//             int j=0;
//             foreach (const MusicLibraryItem *albumItem, artistItem->childItems()) {
//                 if (albumItem->data()==album) {
//                     if (static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
//                         QModelIndex idx=index(j, 0, index(i, 0, QModelIndex()));
//                         emit dataChanged(idx, idx);
//                     }
//                     return;
//                 }
//                 j++;
//             }
//         }
//         i++;
//     }
}

/**
 * Writes the musiclibrarymodel to and xml file so we can store it on
 * disk for faster startup the next time
 *
 * @param filename The name of the file to write the xml to
 */
void MusicLibraryModel::toXML(const MusicLibraryItemRoot *root, const QDateTime &date)
{
    root->toXML(cacheFileName(), date);
}

/**
 * Read an xml file from disk.
 *
 * @param filename The name of the xmlfile to read the db from
 *
 * @return true on succesfull parsing, false otherwise
 * TODO: check for hostname
 * TODO: check for database version
 */
bool MusicLibraryModel::fromXML()
{
    MusicLibraryItemRoot *root=new MusicLibraryItemRoot;
    quint32 date=root->fromXML(cacheFileName(), MPDStats::self()->dbUpdate());
    if (!date) {
        delete root;
        return false;
    }

    QDateTime dt;
    dt.setTime_t(date);
    updateMusicLibrary(root, dt, true);
    return true;
}

Qt::ItemFlags MusicLibraryModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    } else {
        return Qt::ItemIsDropEnabled;
    }
}

QStringList MusicLibraryModel::filenames(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> songList=songs(indexes, allowPlaylists);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

static inline void addSong(const MusicLibraryItem *song, QList<Song> &insertInto, QList<Song> &checkAgainst, bool allowPlaylists)
{
    if (MusicLibraryItem::Type_Song==song->itemType() &&
        (allowPlaylists || Song::Playlist!=static_cast<const MusicLibraryItemSong*>(song)->song().type) &&
        !checkAgainst.contains(static_cast<const MusicLibraryItemSong*>(song)->song())) {
        static_cast<const MusicLibraryItemSong*>(song)->song().updateSize(MPDConnection::self()->getDetails().dir);
        insertInto << static_cast<const MusicLibraryItemSong*>(song)->song();
    }
}

QList<Song> MusicLibraryModel::songs(const QModelIndexList &indexes, bool allowPlaylists) const
{
    QList<Song> songs;

    foreach(QModelIndex index, indexes) {
        const MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist: {
            QList<Song> artistSongs;
            foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                const MusicLibraryItemSong *cue=allowPlaylists ? static_cast<const MusicLibraryItemAlbum *>(album)->getCueFile() : 0;
                if (cue) {
                    addSong(cue, artistSongs, songs, true);
                } else {
                    foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                        addSong(song, artistSongs, songs, false);
                    }
                }
            }
            qSort(artistSongs);
            songs << artistSongs;
            break;
        }
        case MusicLibraryItem::Type_Album: {
            QList<Song> albumSongs;
            const MusicLibraryItemSong *cue=allowPlaylists ? static_cast<const MusicLibraryItemAlbum *>(item)->getCueFile() : 0;
            if (cue) {
                addSong(cue, albumSongs, songs, true);
            } else {
                foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(item)->childItems()) {
                    addSong(song, albumSongs, songs, false);
                }
            }
            qSort(albumSongs);
            songs << albumSongs;
            break;
        }
        case MusicLibraryItem::Type_Song:
            addSong(item, songs, songs, allowPlaylists);
            break;
        default:
            break;
        }
    }

    return songs;
}

QList<Song> MusicLibraryModel::songs(const QStringList &filenames) const
{
    QList<Song> songs;

    if (filenames.isEmpty()) {
        return songs;
    }

    QSet<QString> files=filenames.toSet();

    foreach (const MusicLibraryItem *artist, static_cast<const MusicLibraryItemContainer *>(rootItem)->childItems()) {
        foreach (const MusicLibraryItem *album, static_cast<const MusicLibraryItemContainer *>(artist)->childItems()) {
            foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(album)->childItems()) {
                QSet<QString>::Iterator it=files.find(static_cast<const MusicLibraryItemSong*>(song)->file());

                if (it!=files.end()) {
                    files.erase(it);
                    songs.append(static_cast<const MusicLibraryItemSong*>(song)->song());
                }
            }
        }
    }

    return songs;
}

/**
* Convert the data at indexes into mimedata ready for transport
*
* @param indexes The indexes to pack into mimedata
* @return The mimedata
*/
QMimeData *MusicLibraryModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList files=filenames(indexes, true);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    if (!MPDConnection::self()->getDetails().dir.isEmpty()) {
        QStringList paths;
        foreach (const QString &f, files) {
            paths << MPDConnection::self()->getDetails().dir+f;
        }
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}
