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
#include "localize.h"
#include "utils.h"
#include "icons.h"
#include "stdactions.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QCommonStyle>
#include <QFile>
#include <QTimer>
#include <QStringRef>
#include <QDateTime>
#include <QDir>
#include <QMimeData>
#include <QStringList>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#endif

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

const QLatin1String MusicLibraryModel::constLibraryCache("library/");
const QLatin1String MusicLibraryModel::constLibraryExt(".xml");
const QLatin1String MusicLibraryModel::constLibraryCompressedExt(".xml.gz");

static const QString cacheFileName()
{
    QString fileName=MPDConnection::self()->getDetails().hostname+MusicLibraryModel::constLibraryCompressedExt;
    fileName.replace('/', '_');
    return Utils::cacheDir(MusicLibraryModel::constLibraryCache)+fileName;
}

void MusicLibraryModel::convertCache(const QString &compressedName)
{
    QString prev=compressedName;
    prev.replace(constLibraryCompressedExt, constLibraryExt);

    if (QFile::exists(prev) && !QFile::exists(compressedName)) {
        QFile old(prev);
        if (old.open(QIODevice::ReadOnly)) {
            QByteArray a=old.readAll();
            old.close();

            QFile newCache(compressedName);
            QtIOCompressor compressor(&newCache);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(a);
                compressor.close();
                QFile::remove(prev);
            }
        }
    }
}

void MusicLibraryModel::cleanCache()
{
    QSet<QString> existing;
    QList<MPDConnectionDetails> connections=Settings::self()->allConnections();
    foreach (const MPDConnectionDetails &conn, connections) {
        QString fileName=conn.hostname;
        fileName.replace('/', '_');
        existing.insert(fileName+constLibraryExt);
    }

    QDir dir(Utils::cacheDir(constLibraryCache));
    QFileInfoList files=dir.entryInfoList(QStringList() << "*"+constLibraryExt, QDir::Files);
    foreach (const QFileInfo &file, files) {
        if (!existing.contains(file.fileName())) {
            QFile::remove(file.absoluteFilePath());
        }
    }
}

MusicLibraryModel::MusicLibraryModel(QObject *parent, bool isMpdModel, bool isCheckable)
    : ActionModel(parent)
    , checkable(isCheckable)
    , rootItem(new MusicLibraryItemRoot)
{
    if (isMpdModel)
    {
        connect(Covers::self(), SIGNAL(artistImage(const Song &, const QImage &, const QString &)),
                this, SLOT(setArtistImage(const Song &, const QImage &)));
        connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)),
                this, SLOT(setCover(const Song &, const QImage &, const QString &)));
        connect(Covers::self(), SIGNAL(coverUpdated(const Song &, const QImage &, const QString &)),
                this, SLOT(updateCover(const Song &, const QImage &, const QString &)));
        connect(MPDConnection::self(), SIGNAL(musicLibraryUpdated(MusicLibraryItemRoot *, QDateTime)),
                this, SLOT(updateMusicLibrary(MusicLibraryItemRoot *, QDateTime)));
    }
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

QVariant MusicLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::CheckStateRole:
        if (!checkable) {
            return QVariant();
        }
        return item->checkState();
    case Qt::DecorationRole:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            if (rootItem->useArtistImages()) {
                return artist->cover();
            } else {
                return artist->isVarious() ? Icons::variousArtistsIcon : Icons::artistIcon;
            }
        }
        case MusicLibraryItem::Type_Album:
            if (!rootItem->useAlbumImages() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return Icons::albumIcon;
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
                : item->data()+"<br/>"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Album", "%1 Albums", item->childCount());
                    #else
                    QTP_ALBUMS_STR(item->childCount());
                    #endif
        case MusicLibraryItem::Type_Album:
            return item->parentItem()->data()+QLatin1String("<br/>")+(0==item->childCount()
                ? item->data()
                : item->data()+"<br/>"+
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("1 Track (%2)", "%1 Tracks (%2)", item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
                    #else
                    QTP_TRACKS_DURATION_STR(item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()))
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
            return MusicLibraryItemAlbum::iconSize(rootItem->useLargeImages());
        } else if (MusicLibraryItem::Type_Album==item->itemType() || (rootItem->useArtistImages() && MusicLibraryItem::Type_Artist==item->itemType())) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return QTP_ALBUMS_STR(item->childCount());
            #endif
            break;
        case MusicLibraryItem::Type_Song:
            return Song::formattedTime(static_cast<MusicLibraryItemSong *>(item)->time());
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track (%2)", "%1 Tracks (%2)", item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #else
            return QTP_TRACKS_DURATION_STR(item->childCount(), Song::formattedTime(static_cast<MusicLibraryItemAlbum *>(item)->totalTime()));
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image:
        if (MusicLibraryItem::Type_Album==item->itemType()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
        } else if (MusicLibraryItem::Type_Artist==item->itemType() && rootItem->useArtistImages()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemArtist *>(item)->cover());
            return v;
        }
    case Qt::SizeHintRole:
        if (!rootItem->useArtistImages() && MusicLibraryItem::Type_Artist==item->itemType()) {
            return QVariant();
        }
        if (rootItem->useLargeImages() && MusicLibraryItem::Type_Song!=item->itemType() && !MusicLibraryItemAlbum::itemSize().isNull()) {
            return MusicLibraryItemAlbum::itemSize();
        }
    default:
        return ActionModel::data(index, role);
    }
    return QVariant();
}

bool MusicLibraryModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (checkable && Qt::CheckStateRole==role) {
        if (!idx.isValid()) {
            return false;
        }

        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(idx.internalPointer());
        Qt::CheckState check=value.toBool() ? Qt::Checked : Qt::Unchecked;

        if (item->checkState()==check) {
            return false;
        }

        switch (item->itemType()) {
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(item);
            QModelIndex artistIndex;
            item->setCheckState(check);
            foreach (MusicLibraryItem *album, artistItem->childItems()) {
                if (check!=album->checkState()) {
                    MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
                    if (!artistIndex.isValid()) {
                        artistIndex=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
                    }
                    QModelIndex albumIndex=index(artistItem->childItems().indexOf(album), 0, artistIndex);
                    album->setCheckState(check);
                    foreach (MusicLibraryItem *song, albumItem->childItems()) {
                        if (check!=song->checkState()) {
                            song->setCheckState(check);
                            QModelIndex songIndex=index(albumItem->childItems().indexOf(song), 0, albumIndex);
                            emit dataChanged(songIndex, songIndex);
                        }
                    }
                    emit dataChanged(albumIndex, albumIndex);
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album: {
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(item->parentItem());
            QModelIndex artistIndex=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(item);
            QModelIndex albumIndex;
            albumItem->setCheckState(check);
            foreach (MusicLibraryItem *song, albumItem->childItems()) {
                if (check!=song->checkState()) {
                    song->setCheckState(check);
                    if (!albumIndex.isValid()) {
                        albumIndex=index(artistItem->childItems().indexOf(item), 0, artistIndex);
                    }
                    QModelIndex songIndex=index(albumItem->childItems().indexOf(song), 0, albumIndex);
                    emit dataChanged(songIndex, songIndex);
                }
            }

            setParentState(artistIndex, value.toBool(), artistItem, item);
            break;
        }
        case MusicLibraryItem::Type_Song: {
            item->setCheckState(check);
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(item->parentItem());
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(albumItem->parentItem());
            QModelIndex artistIndex=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
            QModelIndex albumIndex=index(artistItem->childItems().indexOf(albumItem), 0, artistIndex);
            QModelIndex songIndex=index(albumItem->childItems().indexOf(item), 0, albumIndex);
            setParentState(albumIndex, value.toBool(), albumItem, item);
            setParentState(artistIndex, Qt::Unchecked!=albumItem->checkState(), artistItem, albumItem);
            emit dataChanged(songIndex, songIndex);
            break;
        }
        case MusicLibraryItem::Type_Root:
            return false;
        }

        return true;
    }
    return ActionModel::setData(idx, value, role);
}

void MusicLibraryModel::setParentState(const QModelIndex &parent, bool childChecked, MusicLibraryItemContainer *parentItem, MusicLibraryItem *item)
{
    Qt::CheckState parentCheck=childChecked ? Qt::PartiallyChecked : Qt::Unchecked;
    int checkedChildren=childChecked ? 1 : 0;
    int uncheckedChildren=childChecked ? 0 : 1;
    bool stop=false;
    foreach (MusicLibraryItem *child, parentItem->childItems()) {
        if (child!=item) {
            switch (child->checkState()) {
            case Qt::PartiallyChecked:
                parentCheck=Qt::PartiallyChecked;
                stop=true;
                break;
            case Qt::Unchecked:
                uncheckedChildren++;
                parentCheck=checkedChildren ? Qt::PartiallyChecked : Qt::Unchecked;
                stop=checkedChildren && uncheckedChildren;
                break;
            case Qt::Checked:
                checkedChildren++;
                parentCheck=uncheckedChildren ? Qt::PartiallyChecked : Qt::Checked;
                stop=checkedChildren && uncheckedChildren;
                break;
            }
        }
        if (stop) {
            break;
        }
    }
    if (parentItem->checkState()!=parentCheck) {
        parentItem->setCheckState(parentCheck);
        emit dataChanged(parent, parent);
    }
}

void MusicLibraryModel::clear()
{
    const MusicLibraryItemRoot *oldRoot = rootItem;
    beginResetModel();
    databaseTime = QDateTime();
    rootItem = new MusicLibraryItemRoot;
    rootItem->setLargeImages(oldRoot->useLargeImages());
    rootItem->setUseAlbumImages(oldRoot->useAlbumImages());
    rootItem->setUseArtistImages(oldRoot->useArtistImages());
    delete oldRoot;
    endResetModel();

//     emit updated(rootItem);
    AlbumsModel::self()->update(rootItem);
    //emit updateGenres(QSet<QString>());
}

QModelIndex MusicLibraryModel::findSongIndex(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (MusicLibraryItem *songItem, albumItem->childItems()) {
                if (songItem->data()==s.displayTitle()) {
                    return createIndex(albumItem->childItems().indexOf(songItem), 0, songItem);
                }
            }
        }
    }

    return QModelIndex();
}

QModelIndex MusicLibraryModel::findArtistIndex(const QString &artist) const
{
    Song s;
    s.artist=artist;
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    return artistItem ? index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex()) : QModelIndex();
}

QModelIndex MusicLibraryModel::findAlbumIndex(const QString &artist, const QString &album) const
{
    Song s;
    s.artist=artist;
    s.album=album;
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    MusicLibraryItemAlbum *albumItem = artistItem ? artistItem->album(s, false) : 0;
    return artistItem ? index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex())) : QModelIndex();
}

const MusicLibraryItem * MusicLibraryModel::findSong(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
                if (songItem->data()==s.displayTitle() && static_cast<const MusicLibraryItemSong *>(songItem)->song().track==s.track) {
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

bool MusicLibraryModel::updateSong(const Song &orig, const Song &edit)
{
    if (orig.albumArtist()==edit.albumArtist() && orig.album==edit.album) {
        MusicLibraryItemArtist *artistItem = rootItem->artist(orig, false);
        if (!artistItem) {
            return false;
        }
        MusicLibraryItemAlbum *albumItem = artistItem->album(orig, false);
        if (!albumItem) {
            return false;
        }
        int songRow=0;
        foreach (MusicLibraryItem *song, albumItem->childItems()) {
            if (static_cast<MusicLibraryItemSong *>(song)->song()==orig) {
                static_cast<MusicLibraryItemSong *>(song)->setSong(edit);
                if (orig.genre!=edit.genre) {
                    albumItem->updateGenres();
                    artistItem->updateGenres();
                    rootItem->updateGenres();
                }

                if (orig.year!=edit.year) {
                    if (albumItem->updateYear()) {
                        QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex()));
                        emit dataChanged(idx, idx);
                    }
                }

                QModelIndex idx=index(songRow, 0, index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex())));
                emit dataChanged(idx, idx);
                return true;
            }
            songRow++;
        }
    }
    return false;
}

void MusicLibraryModel::addSongToList(const Song &s)
{
//     databaseTime=QDateTime();
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
    quint32 year=albumItem->year();
    foreach (const MusicLibraryItem *songItem, albumItem->childItems()) {
        const MusicLibraryItemSong *song=static_cast<const MusicLibraryItemSong *>(songItem);
        if (song->track()==s.track && song->disc()==s.disc && song->data()==s.displayTitle()) {
            return;
        }
    }

    beginInsertRows(createIndex(artistItem->childItems().indexOf(albumItem), 0, albumItem), albumItem->childCount(), albumItem->childCount());
    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
    albumItem->append(songItem);
    rootItem->addGenre(s.genre);
    endInsertRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex()));
        emit dataChanged(idx, idx);
    }
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

//     databaseTime=QDateTime();
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
    quint32 year=albumItem->year();
    albumItem->remove(songRow);
    endRemoveRows();
    if (year!=albumItem->year()) {
        QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex()));
        emit dataChanged(idx, idx);
    }
}

void MusicLibraryModel::updateSongFile(const Song &from, const Song &to)
{
    rootItem->updateSongFile(from, to);
}

void MusicLibraryModel::removeCache()
{
    QString cacheFile(cacheFileName());
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }

    // Remove old (non-compressed) cache file as well...
    QString oldCache=cacheFile;
    oldCache.replace(constLibraryCompressedExt, constLibraryExt);
    if (oldCache!=cacheFile && QFile::exists(oldCache)) {
        QFile::remove(oldCache);
    }

    databaseTime = QDateTime();
}

void MusicLibraryModel::getDetails(QSet<QString> &artists, QSet<QString> &albumArtists, QSet<QString> &albums, QSet<QString> &genres)
{
    rootItem->getDetails(artists, albumArtists, albums, genres);
}

QSet<QString> MusicLibraryModel::getAlbumArtists()
{
    QSet<QString> a;
    foreach (MusicLibraryItem *i, rootItem->childItems()) {
        a.insert(i->data());
    }
    return a;
}

void MusicLibraryModel::updateMusicLibrary(MusicLibraryItemRoot *newroot, QDateTime dbUpdate, bool fromFile)
{
    if (databaseTime >= dbUpdate) {
        delete newroot;
        return;
    }

    newroot->setUseAlbumImages(rootItem->useAlbumImages());
    newroot->setUseArtistImages(rootItem->useArtistImages());
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
        rootItem->setLargeImages(oldRoot->useLargeImages());
        rootItem->setUseAlbumImages(oldRoot->useAlbumImages());
        rootItem->setUseArtistImages(oldRoot->useArtistImages());
        delete oldRoot;
        endResetModel();
        updatedSongs=true;
    }

    if (updatedSongs || needToUpdate) {
        if (!fromFile && (needToSave || needToUpdate)) {
            toXML(rootItem, dbUpdate);
        }
    }

    AlbumsModel::self()->update(rootItem);
    emit updateGenres(rootItem->genres());
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

    if (updatedSongs && checkable) {
        QSet<Song> chkdSongs;
        foreach (MusicLibraryItem *artist, rootItem->childItems()) {
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(artist);
            QModelIndex artistIndex;
            int numCheckedAlbums=0;
            int numUnCheckedAlbums=0;
            int numPartialAlbums=0;

            foreach (MusicLibraryItem *album, artistItem->childItems()) {
                MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
                int numCheckedSongs=0;
                int numUnCheckedSongs=0;
                foreach (MusicLibraryItem *song, albumItem->childItems()) {
                    if (Qt::Unchecked==song->checkState()) {
                        numUnCheckedSongs++;
                    } else {
                        chkdSongs.insert(static_cast<MusicLibraryItemSong *>(song)->song());
                        numCheckedSongs++;
                    }
                }
                Qt::CheckState albumState=numCheckedAlbums && numUnCheckedAlbums ? Qt::PartiallyChecked : numCheckedAlbums ? Qt::Checked : Qt::Unchecked;
                if (albumState!=albumItem->checkState()) {
                    albumItem->setCheckState(albumState);
                    if (!artistIndex.isValid()) {
                        artistIndex=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
                    }
                    QModelIndex albumIndex=index(artistItem->childItems().indexOf(albumItem), 0, artistIndex);
                    emit dataChanged(albumIndex, albumIndex);
                }

                switch (albumState) {
                case Qt::PartiallyChecked: numPartialAlbums++;   break;
                case Qt::Checked:          numCheckedAlbums++;   break;
                case Qt::Unchecked:        numUnCheckedAlbums++; break;
                }
            }

            Qt::CheckState artistState=numPartialAlbums ? Qt::PartiallyChecked : numCheckedAlbums ? Qt::Checked : Qt::Unchecked;
            if (artistState!=artistItem->checkState()) {
                artistItem->setCheckState(artistState);
                if (!artistIndex.isValid()) {
                    artistIndex=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
                }
                emit dataChanged(artistIndex, artistIndex);
            }
        }
        emit checkedSongs(chkdSongs);
    }

    return updatedSongs;
}

void MusicLibraryModel::uncheckAll()
{
    if (!checkable) {
        return;
    }

    foreach (MusicLibraryItem *artist, rootItem->childItems()) {
        MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(artist);
        QModelIndex artistIndex=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());

        foreach (MusicLibraryItem *album, artistItem->childItems()) {
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
            QModelIndex albumIndex=index(artistItem->childItems().indexOf(albumItem), 0, artistIndex);

            foreach (MusicLibraryItem *song, albumItem->childItems()) {
                if (Qt::Unchecked!=song->checkState()) {
                    song->setCheckState(Qt::Unchecked);
                    QModelIndex songIndex=index(albumItem->childItems().indexOf(song), 0, albumIndex);
                    emit dataChanged(songIndex, songIndex);
                }
            }
            if (Qt::Unchecked!=albumItem->checkState()) {
                albumItem->setCheckState(Qt::Unchecked);
                emit dataChanged(albumIndex, albumIndex);
            }
        }
        if (Qt::Unchecked!=artistItem->checkState()) {
            artistItem->setCheckState(Qt::Unchecked);
            emit dataChanged(artistIndex, artistIndex);
        }
    }
    emit checkedSongs(QSet<Song>());
}

void MusicLibraryModel::toggleGrouping()
{
    beginResetModel();
    rootItem->toggleGrouping();
    endResetModel();
}

QList<Song> MusicLibraryModel::getAlbumTracks(const Song &s) const
{
    QList<Song> songs;
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (MusicLibraryItem *songItem, albumItem->childItems()) {
                songs.append(static_cast<MusicLibraryItemSong *>(songItem)->song());
            }
            qSort(songs);
        }
    }
    return songs;
}

QList<Song> MusicLibraryModel::getArtistAlbums(const QString &albumArtist) const
{
    QList<Song> tracks;
    foreach (MusicLibraryItem *ar, rootItem->childItems()) {
        if (ar->data()==albumArtist) {
            foreach (MusicLibraryItem *al, static_cast<MusicLibraryItemContainer *>(ar)->childItems()) {
                MusicLibraryItemContainer *a=static_cast<MusicLibraryItemContainer *>(al);
                if (!a->childItems().isEmpty()) {
                    tracks.append(static_cast<MusicLibraryItemSong *>(a->childItems().first())->song());
                }
            }
            break;
        }
    }
    return tracks;
}

QMap<QString, QStringList> MusicLibraryModel::getAlbums(const Song &song) const
{
    QMap<QString, QStringList> albums;

    foreach (MusicLibraryItem *ar, rootItem->childItems()) {
        if (static_cast<MusicLibraryItemArtist *>(ar)->isVarious()) {
            foreach (MusicLibraryItem *al, static_cast<MusicLibraryItemContainer *>(ar)->childItems()) {
                foreach (MusicLibraryItem *s, static_cast<MusicLibraryItemContainer *>(al)->childItems()) {
                    if (static_cast<MusicLibraryItemSong *>(s)->song().artist.contains(song.artist)) {
                        albums[ar->data()].append(al->data());
                        break;
                    }
                }
            }
        } else if (ar->data()==song.albumArtist()) {
            foreach (MusicLibraryItem *al, static_cast<MusicLibraryItemContainer *>(ar)->childItems()) {
                albums[song.albumArtist()].append(al->data());
            }
        }
    }
    return albums;
}

void MusicLibraryModel::setArtistImage(const Song &song, const QImage &img, bool update)
{
    if (!rootItem->useArtistImages() || img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() ||
        song.file.startsWith("http://") || song.name.startsWith("http://")) {
        return;
    }

    MusicLibraryItemArtist *artistItem = rootItem->artist(song, false);
    if (artistItem && artistItem->setCover(img, update)) {
        QModelIndex idx=index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex());
        emit dataChanged(idx, idx);
    }
}

void MusicLibraryModel::setCover(const Song &song, const QImage &img, const QString &file)
{
    setCover(song, img, file, false);
}

void MusicLibraryModel::updateCover(const Song &song, const QImage &img, const QString &file)
{
    if (song.isArtistImageRequest()) {
        setArtistImage(song, img, true);
    } else {
        setCover(song, img, file, true);
    }
}

void MusicLibraryModel::setCover(const Song &song, const QImage &img, const QString &file, bool update)
{
    Q_UNUSED(file)
    if (!rootItem->useAlbumImages() || img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() ||
        song.file.startsWith("http:/") || song.name.startsWith("http:/")) {
        return;
    }

    MusicLibraryItemArtist *artistItem = rootItem->artist(song, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(song, false);
        if (albumItem && albumItem->setCover(img, update)) {
            QModelIndex idx=index(artistItem->childItems().indexOf(albumItem), 0, index(rootItem->childItems().indexOf(artistItem), 0, QModelIndex()));
            emit dataChanged(idx, idx);
        }
    }
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
    convertCache(cacheFileName());
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
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | (checkable ? Qt::ItemIsUserCheckable : Qt::NoItemFlags);
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
            // First, sort all albums as they would appear in UI...
            QList<MusicLibraryItem *> artistAlbums=static_cast<const MusicLibraryItemContainer *>(item)->childItems();
            qSort(artistAlbums.begin(), artistAlbums.end(), MusicLibraryItemAlbum::lessThan);

            foreach (MusicLibraryItem *i, artistAlbums) {
                QList<Song> albumSongs;
                const MusicLibraryItemSong *cue=allowPlaylists ? static_cast<const MusicLibraryItemAlbum *>(i)->getCueFile() : 0;
                if (cue) {
                    addSong(cue, albumSongs, songs, true);
                } else {
                    foreach (const MusicLibraryItem *song, static_cast<const MusicLibraryItemContainer *>(i)->childItems()) {
                        addSong(song, albumSongs, songs, false);
                    }
                }
                qSort(albumSongs);
                songs << albumSongs;
            }
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

QList<Song> MusicLibraryModel::songs(const QStringList &filenames, bool insertNotFound) const
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

    if (insertNotFound && files.size()) {
        foreach (const QString &file, files) {
            Song s;
            s.file=file;
            songs.append(s);
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
