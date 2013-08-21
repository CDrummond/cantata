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

static QString cacheFileName(const MPDConnectionDetails &details, bool withPort=true)
{
    QString fileName=(withPort && !details.isLocal() ? details.hostname+'_'+QString::number(details.port) : details.hostname)
                     +MusicLibraryModel::constLibraryCompressedExt;
    fileName.replace('/', '_');
    return Utils::cacheDir(MusicLibraryModel::constLibraryCache)+fileName;
}

static QString cacheFileName(bool withPort=true)
{
    return cacheFileName(MPDConnection::self()->getDetails(), withPort);
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
        QString withPort=cacheFileName(conn);
        QString withoutPort=cacheFileName(conn, false);
        if (withPort!=withoutPort) {
            existing.insert(withoutPort);
        }
        existing.insert(withPort);
    }
    QDir dir(Utils::cacheDir(constLibraryCache));
    QFileInfoList files=dir.entryInfoList(QStringList() << "*"+constLibraryExt << "*"+constLibraryCompressedExt, QDir::Files);
    foreach (const QFileInfo &file, files) {
        if (!existing.contains(file.fileName())) {
            QFile::remove(file.absoluteFilePath());
        }
    }
}

MusicLibraryModel::MusicLibraryModel(QObject *parent, bool isMpdModel, bool isCheckable)
    : MusicModel(parent)
    , mpdModel(isMpdModel)
    , checkable(isCheckable)
    , rootItem(new MusicLibraryItemRoot)
{
    if (mpdModel)
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
    rootItem->setModel(this);
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

QVariant MusicLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (checkable && Qt::CheckStateRole==role) {
        return static_cast<MusicLibraryItem *>(index.internalPointer())->checkState();
    }
    return MusicModel::data(index, role);
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
            QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());
            item->setCheckState(check);
            foreach (MusicLibraryItem *album, artistItem->childItems()) {
                if (check!=album->checkState()) {
                    MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
                    QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
                    album->setCheckState(check);
                    foreach (MusicLibraryItem *song, albumItem->childItems()) {
                        song->setCheckState(check);
                    }
                    emit dataChanged(index(0, 0, albumIndex), index(0, albumItem->childCount(), albumIndex));
                }
                emit dataChanged(index(0, 0, artistIndex), index(0, artistItem->childCount(), artistIndex));
            }
            emit dataChanged(idx, idx);
            break;
        }
        case MusicLibraryItem::Type_Album: {
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(item->parentItem());
            QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(item);
            albumItem->setCheckState(check);
            QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
            item->setCheckState(check);
            foreach (MusicLibraryItem *song, albumItem->childItems()) {
                song->setCheckState(check);
            }
            emit dataChanged(index(0, 0, albumIndex), index(0, albumItem->childCount(), albumIndex));
            setParentState(artistIndex, value.toBool(), artistItem, item);
            emit dataChanged(idx, idx);
            break;
        }
        case MusicLibraryItem::Type_Song: {
            item->setCheckState(check);
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(item->parentItem());
            MusicLibraryItemArtist *artistItem=static_cast<MusicLibraryItemArtist *>(albumItem->parentItem());
            QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());
            QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
            setParentState(albumIndex, value.toBool(), albumItem, item);
            setParentState(artistIndex, Qt::Unchecked!=albumItem->checkState(), artistItem, albumItem);
            emit dataChanged(idx, idx);
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
    rootItem->setModel(this);
    rootItem->setLargeImages(oldRoot->useLargeImages());
    rootItem->setUseAlbumImages(oldRoot->useAlbumImages());
    rootItem->setUseArtistImages(oldRoot->useArtistImages());
    delete oldRoot;
    endResetModel();

    if (mpdModel) {
        AlbumsModel::self()->update(rootItem);
    }
}

QModelIndex MusicLibraryModel::findSongIndex(const Song &s) const
{
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    if (artistItem) {
        MusicLibraryItemAlbum *albumItem = artistItem->album(s, false);
        if (albumItem) {
            foreach (MusicLibraryItem *songItem, albumItem->childItems()) {
                if (songItem->data()==s.displayTitle()) {
                    return createIndex(songItem->row(), 0, songItem);
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
    return artistItem ? index(artistItem->row(), 0, QModelIndex()) : QModelIndex();
}

QModelIndex MusicLibraryModel::findAlbumIndex(const QString &artist, const QString &album) const
{
    Song s;
    s.artist=artist;
    s.album=album;
    MusicLibraryItemArtist *artistItem = rootItem->artist(s, false);
    MusicLibraryItemAlbum *albumItem = artistItem ? artistItem->album(s, false) : 0;
    return artistItem ? index(albumItem->row(), 0, index(artistItem->row(), 0, QModelIndex())) : QModelIndex();
}

void MusicLibraryModel::removeCache()
{
    QString cacheFile(cacheFileName());
    if (QFile::exists(cacheFile)) {
        QFile::remove(cacheFile);
    }

    // Remove old (non-compressed) cache file as well...
    QString cacheFileWithoutPort(cacheFileName(false));
    if (cacheFileWithoutPort!=cacheFile && QFile::exists(cacheFileWithoutPort)) {
        QFile::remove(cacheFileWithoutPort);
    }

    // Remove old (non-compressed) cache file as well...
    QString oldCache=cacheFile;
    oldCache.replace(constLibraryCompressedExt, constLibraryExt);
    if (oldCache!=cacheFile && QFile::exists(oldCache)) {
        QFile::remove(oldCache);
    }

    databaseTime = QDateTime();
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
    if (!mpdModel || databaseTime >= dbUpdate) {
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
        rootItem->setModel(this);
        rootItem->setLargeImages(oldRoot->useLargeImages());
        rootItem->setUseAlbumImages(oldRoot->useAlbumImages());
        rootItem->setUseArtistImages(oldRoot->useArtistImages());
        delete oldRoot;
        endResetModel();
        updatedSongs=true;
    }

    if ((updatedSongs || needToUpdate) && (!fromFile && (needToSave || needToUpdate))) {
        rootItem->toXML(cacheFileName(), dbUpdate);
    }

    AlbumsModel::self()->update(rootItem);
    emit updateGenres(rootItem->genres());
}

bool MusicLibraryModel::update(const QSet<Song> &songs)
{
    bool updatedSongs=rootItem->update(songs);

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
                        artistIndex=index(artistItem->row(), 0, QModelIndex());
                    }
                    QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);
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
                    artistIndex=index(artistItem->row(), 0, QModelIndex());
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
        QModelIndex artistIndex=index(artistItem->row(), 0, QModelIndex());

        foreach (MusicLibraryItem *album, artistItem->childItems()) {
            MusicLibraryItemAlbum *albumItem=static_cast<MusicLibraryItemAlbum *>(album);
            QModelIndex albumIndex=index(albumItem->row(), 0, artistIndex);

            foreach (MusicLibraryItem *song, albumItem->childItems()) {
                if (Qt::Unchecked!=song->checkState()) {
                    song->setCheckState(Qt::Unchecked);
                    QModelIndex songIndex=index(song->row(), 0, albumIndex);
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
    if (mpdModel) {
        AlbumsModel::self()->update(rootItem);
    }
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

QList<Song> MusicLibraryModel::getArtistAlbumsFirstTracks(const Song &song) const
{
    QString artist=song.isVariousArtists() ? song.artist : song.albumArtist();
    QString basicArtist=song.basicArtist();
    QList<Song> tracks;
    foreach (MusicLibraryItem *ar, rootItem->childItems()) {
        if (static_cast<MusicLibraryItemArtist *>(ar)->isVarious()) {
            foreach (MusicLibraryItem *al, static_cast<MusicLibraryItemContainer *>(ar)->childItems()) {
                MusicLibraryItemAlbum *a=static_cast<MusicLibraryItemAlbum *>(al);
                if (a->containsArtist(basicArtist)) {
                    tracks.append(static_cast<MusicLibraryItemSong *>(a->childItems().first())->song());
                }
            }
        } else if (ar->data()==artist) {
            foreach (MusicLibraryItem *al, static_cast<MusicLibraryItemContainer *>(ar)->childItems()) {
                MusicLibraryItemContainer *a=static_cast<MusicLibraryItemContainer *>(al);
                if (!a->childItems().isEmpty()) {
                    tracks.append(static_cast<MusicLibraryItemSong *>(a->childItems().first())->song());
                }
            }
        }
    }
    return tracks;
}

void MusicLibraryModel::setArtistImage(const Song &song, const QImage &img, bool update)
{
    if (!rootItem->useArtistImages() || img.isNull() || MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize() ||
        song.file.startsWith("http://") || song.name.startsWith("http://")) {
        return;
    }

    MusicLibraryItemArtist *artistItem = rootItem->artist(song, false);
    if (artistItem && artistItem->setCover(img, update)) {
        QModelIndex idx=index(artistItem->row(), 0, QModelIndex());
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
            QModelIndex idx=index(albumItem->row(), 0, index(artistItem->row(), 0, QModelIndex()));
            emit dataChanged(idx, idx);
        }
    }
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
    // If socket connection used, then check if cache file has port number...
    QString withPort=cacheFileName();
    QString withoutPort=cacheFileName(false);
    if (withPort!=withoutPort && QFile::exists(withoutPort) && !QFile::exists(withPort)) {
        QFile::rename(withoutPort, withPort);
    }

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

    foreach (const MusicLibraryItem *artist, rootItem->childItems()) {
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
