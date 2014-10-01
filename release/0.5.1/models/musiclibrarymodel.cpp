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
#include "network.h"
#include <QtGui/QCommonStyle>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QStringRef>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
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
    static MusicLibraryModel *instance=0;;
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

    QString fileName=Settings::self()->connectionHost()+constLibraryExt;
    fileName.replace('/', '_');
    return Network::cacheDir(constLibraryCache)+fileName;
}

MusicLibraryModel::MusicLibraryModel(QObject *parent)
    : QAbstractItemModel(parent)
    , rootItem(new MusicLibraryItemRoot)
{
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)),
            this, SLOT(setCover(const QString &, const QString &, const QImage &, const QString &)));
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

    MusicLibraryItem * const childItem = parentItem->child(row);
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
    MusicLibraryItem * const parentItem = childItem->parent();

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

#ifndef ENABLE_KDE_SUPPORT
const QIcon & MusicLibraryModel::vaIcon() const
{
    static QIcon icon;

    if (icon.isNull()) {
        icon.addFile(":va16.png");
        icon.addFile(":va22.png");
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
        switch (item->type()) {
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            #ifdef ENABLE_KDE_SUPPORT
            return artist->isVarious() ? QIcon::fromTheme("cantata-view-media-artist-various") : QIcon::fromTheme("view-media-artist");
            #else
            return artist->isVarious() ? vaIcon() : QIcon::fromTheme("view-media-artist");
            #endif
        }
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return QIcon::fromTheme("media-optical-audio");
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->type()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (static_cast<MusicLibraryItemAlbum *>(song->parent())->isSingleTracks()) {
                return song->song().artistSong();
            } else {
                return song->song().trackAndTitleStr(static_cast<MusicLibraryItemArtist *>(song->parent()->parent())->isVarious() &&
                                                     !Song::isVariousArtists(song->song().artist));
            }
        } else if(MusicLibraryItem::Type_Album==item->type() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        switch (item->type()) {
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
            return item->parent()->data()+QLatin1String("<br/>")+(0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1<br/>1 Track (%3)", "%1<br/>%2 Tracks (%3)", item->data(), item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
                    #else
                    (item->childCount()>1
                        ? tr("%1<br/>%2 Tracks (%3)").arg(item->data()).arg(item->childCount()).arg(Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
                        : tr("%1<br/>1 Track (%3)").arg(item->data()).arg(Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime())))
                    #endif
                );
        case MusicLibraryItem::Type_Song: {
            return item->parent()->parent()->data()+QLatin1String("<br/>")+item->parent()->data()+QLatin1String("<br/>")+
                   data(index, Qt::DisplayRole).toString()+QLatin1String("<br/>")+Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time())+
                   QLatin1String("<br/><small><i>")+static_cast<MusicLibraryItemSong *>(item)->song().file+QLatin1String("</i></small>");
        }
        default: return QVariant();
        }
    case ItemView::Role_ImageSize:
        if (MusicLibraryItem::Type_Album==item->type()) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->type()) {
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
        if (MusicLibraryItem::Type_Album==item->type()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
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

bool MusicLibraryModel::songExists(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (const MusicLibraryItem *songItem, albumItem->children()) {
                if (songItem->data()==s.title) {
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
        beginInsertRows(createIndex(rootItem->children().indexOf(artistItem), 0, artistItem), artistItem->childCount(), artistItem->childCount());
        albumItem = artistItem->createAlbum(s);
        endInsertRows();
    }
    foreach (const MusicLibraryItem *songItem, albumItem->children()) {
        if (songItem->data()==s.title) {
            return;
        }
    }

    beginInsertRows(createIndex(artistItem->children().indexOf(albumItem), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
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
    foreach (MusicLibraryItem *song, albumItem->children()) {
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
        int row=rootItem->children().indexOf(artistItem);
        beginRemoveRows(QModelIndex(), row, row);
        rootItem->remove(artistItem);
        endRemoveRows();
        return;
    }

    if (1==albumItem->childCount()) {
        // multiple albums, but this album only has 1 song - remove album
        int row=artistItem->children().indexOf(albumItem);
        beginRemoveRows(createIndex(rootItem->children().indexOf(artistItem), 0, artistItem), row, row);
        artistItem->remove(albumItem);
        endRemoveRows();
        return;
    }

    // Just remove particular song
    beginRemoveRows(createIndex(artistItem->children().indexOf(albumItem), 0, albumItem), songRow, songRow);
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
        databaseTime = QDateTime();
    }
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
        QSet<Song> currentSongs=rootItem->allSongs();
        QSet<Song> updateSongs=newroot->allSongs();
        QSet<Song> removed=currentSongs-updateSongs;
        QSet<Song> added=updateSongs-currentSongs;

        updatedSongs=added.count()||removed.count();
        foreach (const Song &s, added) {
            addSongToList(s);
        }
        foreach (const Song &s, removed) {
            removeSongFromList(s);
        }
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

void MusicLibraryModel::setCover(const QString &artist, const QString &album, const QImage &img, const QString &file)
{
    Q_UNUSED(file)
    if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    if (img.isNull()) {
        return;
    }

    Song song;
    song.artist=artist;
    song.album=album;
    MusicLibraryItemArtist *artistItem = rootItem->artist(song, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
        if (albumItem) {
            if (static_cast<const MusicLibraryItemAlbum *>(albumItem)->setCover(img)) {
                QModelIndex idx=index(artistItem->children().indexOf(albumItem), 0, index(rootItem->children().indexOf(artistItem), 0, QModelIndex()));
                emit dataChanged(idx, idx);
            }
        }
    }

//     int i=0;
//     foreach (const MusicLibraryItem *artistItem, rootItem->children()) {
//         if (artistItem->data()==artist) {
//             int j=0;
//             foreach (const MusicLibraryItem *albumItem, artistItem->children()) {
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
    root->toXML(cacheFileName(), QString(), date, Settings::self()->groupSingle());
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
bool MusicLibraryModel::fromXML(const QDateTime dbUpdate)
{
    MusicLibraryItemRoot *root=new MusicLibraryItemRoot;
    quint32 date=root->fromXML(cacheFileName(), QString(), dbUpdate, Settings::self()->groupSingle());
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

QStringList MusicLibraryModel::filenames(const QModelIndexList &indexes) const
{
    QList<Song> songList=songs(indexes);
    QStringList fnames;
    foreach (const Song &s, songList) {
        fnames.append(s.file);
    }
    return fnames;
}

QList<Song> MusicLibraryModel::songs(const QModelIndexList &indexes) const
{
    QList<Song> songs;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Artist:
            foreach (const MusicLibraryItem *album, item->children()) {
                foreach (MusicLibraryItem *song, album->children()) {
                    if (MusicLibraryItem::Type_Song==song->type() && !songs.contains(static_cast<MusicLibraryItemSong*>(song)->song())) {
                        static_cast<MusicLibraryItemSong*>(song)->song().updateSize(Settings::self()->mpdDir());
                        songs << static_cast<MusicLibraryItemSong*>(song)->song();
                    }
                }
            }
            break;
        case MusicLibraryItem::Type_Album:
            foreach (MusicLibraryItem *song, item->children()) {
                if (MusicLibraryItem::Type_Song==song->type() && !songs.contains(static_cast<MusicLibraryItemSong*>(song)->song())) {
                    static_cast<MusicLibraryItemSong*>(song)->song().updateSize(Settings::self()->mpdDir());
                    songs << static_cast<MusicLibraryItemSong*>(song)->song();
                }
            }
            break;
        case MusicLibraryItem::Type_Song:
            if (!songs.contains(static_cast<MusicLibraryItemSong*>(item)->song())) {
                static_cast<MusicLibraryItemSong*>(item)->song().updateSize(Settings::self()->mpdDir());
                songs << static_cast<MusicLibraryItemSong*>(item)->song();
            }
            break;
        default:
            break;
        }
    }
    qSort(songs);
    return songs;
}

QList<Song> MusicLibraryModel::songs(const QStringList &filenames) const
{
    QSet<QString> files=filenames.toSet();
    QList<Song> songs;

    foreach (MusicLibraryItem *artist, rootItem->children()) {
        foreach (MusicLibraryItem *album, artist->children()) {
            foreach (MusicLibraryItem *song, album->children()) {
                QSet<QString>::Iterator it=files.find(static_cast<MusicLibraryItemSong*>(song)->file());

                if (it!=files.end()) {
                    files.erase(it);
                    songs.append(static_cast<MusicLibraryItemSong*>(song)->song());
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
    QStringList files=filenames(indexes);
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, files);
    if (!Settings::self()->mpdDir().isEmpty()) {
        QStringList paths;
        foreach (const QString &f, files) {
            paths << Settings::Settings::self()->mpdDir()+f;
        }
        PlayQueueModel::encode(*mimeData, PlayQueueModel::constUriMimeType, paths);
    }
    return mimeData;
}