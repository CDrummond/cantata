/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "mtpdevice.h"
#include "models/musiclibraryitemsong.h"
#include "models/musiclibraryitemalbum.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemroot.h"
#include "models/mpdlibrarymodel.h"
#include "models/devicesmodel.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "gui/covers.h"
#include "mpd-interface/song.h"
#include "encoders.h"
#include "transcodingjob.h"
#include "support/utils.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdconnection.h"
#include "filejob.h"
#include "support/configuration.h"
#include "support/thread.h"
#include "support/monoicon.h"
#include <QTimer>
#include <QDir>
#include <QTemporaryFile>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
//#define TIME_MTP_OPERATIONS
#ifdef TIME_MTP_OPERATIONS
#include <QElapsedTimer>
#endif
#include <QDebug>

#define COVER_DBUG if (Covers::debugEnabled()) qWarning() << metaObject()->className() << __FUNCTION__
#define DBUG_CLASS(CLASS) if (DevicesModel::debugEnabled()) qWarning() << CLASS << QThread::currentThread()->objectName() << __FUNCTION__
#define DBUG DBUG_CLASS(metaObject()->className())

// Enable the following #define to have Cantata attempt to ascertain the AlbumArtist tag by
// looking at the file path
#define MTP_FAKE_ALBUMARTIST_SUPPORT

// Enable the following #define to have Cantata attempt to ascertain the tracks's track number
// from its filename. This will only be done if the device returns '0' as the track number.
#define MTP_TRACKNUMBER_FROM_FILENAME

static const QLatin1String constMtpDefaultCover("AlbumArt.jpg");
static const uint32_t constRootFolder=0xffffffffU;
static const QString constMusicFolder=QLatin1String("Music");

static const quint16 constOrigFileName = Song::Performer;

static int progressMonitor(uint64_t const processed, uint64_t const total, void const * const data)
{
    const MtpConnection *con=static_cast<const MtpConnection *>(data);
    const_cast<MtpConnection *>(con)->emitProgress((int)(((processed*1.0)/(total*1.0)*100.0)+0.5));
    return con->abortWasRequested() ? -1 : 0;
}

static int trackListMonitor(uint64_t const processed, uint64_t const total, void const * const data)
{
    const MtpConnection *con=static_cast<const MtpConnection *>(data);
    const_cast<MtpConnection *>(con)->trackListProgress(((processed*100.0)/(total*1.0))+0.5);
    return con->abortWasRequested() ? -1 : 0;
}

static uint16_t fileReceiver(void *params, void *priv, uint32_t sendlen, unsigned char *data, uint32_t *putlen)
{
    Q_UNUSED(params)
    QByteArray *byteData=static_cast<QByteArray *>(priv);
    (*byteData)+=QByteArray((char *)data, (int)sendlen);
    *putlen = sendlen;
    return LIBMTP_HANDLER_RETURN_OK;
}

MtpConnection::MtpConnection(const QString &id, unsigned int bus, unsigned int dev, bool aaSupport)
    : device(0)
    #ifdef MTP_CLEAN_ALBUMS
    , albums(0)
    #endif
    , library(0)
    , lastListPercent(-1)
    , abortRequested(false)
    , busNum(bus)
    , devNum(dev)
    , supprtAlbumArtistTag(aaSupport)
{
    size=0;
    used=0;
    LIBMTP_Init();
    thread=new Thread(metaObject()->className()+QLatin1String("::")+id);
    moveToThread(thread);
    thread->start();
}

MtpConnection::~MtpConnection()
{
    stop();
}

MusicLibraryItemRoot * MtpConnection::takeLibrary()
{
    MusicLibraryItemRoot *lib=library;
    library=0;
    return lib;
}

void MtpConnection::emitProgress(int percent)
{
    emit progress(percent);
}

void MtpConnection::trackListProgress(int percent)
{
    if (percent!=lastListPercent) {
        lastListPercent=percent;
        emit updatePercentage(percent);
    }
}

void MtpConnection::connectToDevice()
{
    #ifdef TIME_MTP_OPERATIONS
    QElapsedTimer timer;
    QElapsedTimer totalTimer;
    timer.start();
    totalTimer.start();
    #endif

    device=0;
    storage.clear();
    defaultMusicFolder=0;
    LIBMTP_raw_device_t *rawDevices=0;
    int numDev=-1;
    emit statusMessage(tr("Connecting to device..."));
    if (LIBMTP_ERROR_NONE!=LIBMTP_Detect_Raw_Devices(&rawDevices, &numDev) || numDev<=0) {
        emit statusMessage(tr("No devices found"));
        return;
    }
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Connect to device:" << timer.elapsed();
    timer.restart();
    #endif

    LIBMTP_mtpdevice_t *mptDev=0;
    for (int i = 0; i < numDev; i++) {
        if (0!=busNum && 0!=devNum) {
            if (rawDevices[i].bus_location==busNum && rawDevices[i].devnum==devNum) {
                mptDev = LIBMTP_Open_Raw_Device_Uncached(&rawDevices[i]);
                break;
            }
        } else {
            if ((mptDev = LIBMTP_Open_Raw_Device_Uncached(&rawDevices[i]))) {
                break;
            }
        }
    }

    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Open raw device:" << timer.elapsed();
    timer.restart();
    #endif

    size=0;
    used=0;
    device=mptDev;
    updateStorage();
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Update storage:" << timer.elapsed();
    #endif

    free(rawDevices);
    if (!device) {
        emit statusMessage(tr("No devices found"));
        #ifdef TIME_MTP_OPERATIONS
        qWarning() << "TOTAL connect:" <<totalTimer.elapsed();
        #endif
        return;
    }
    char *ser=LIBMTP_Get_Serialnumber(device);
    if (ser) {
        emit deviceDetails(QString::fromUtf8(ser));
        delete ser;
    } else {
        emit deviceDetails(QString());
    }

    defaultMusicFolder=device->default_music_folder;
    emit statusMessage(tr("Connected to device"));
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "TOTAL connect:" <<totalTimer.elapsed();
    #endif
}

void MtpConnection::disconnectFromDevice(bool showStatus)
{
    if (device) {
        destroyData();
        LIBMTP_Release_Device(device);
        device=0;
        if (showStatus) {
            emit statusMessage(tr("Disconnected from device"));
        }
    }
}

#ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
struct MtpAlbum {
    uint32_t folder;
    QSet<QString> artists;
    QList<MusicLibraryItemSong *> songs;
};
struct MtpFolder {
    MtpFolder(const QString &ar=QString(), const QString &al=QString()) : artist(ar), album(al) { }
    QString artist;
    QString album;
};
#endif

struct Path {
    Path() : storage(0), parent(0), id(0) { }
    bool ok() const { return 0!=storage && 0!=parent && 0!=id; }
    uint32_t storage;
    uint32_t parent;
    uint32_t id;
    QString path;
};

static QString encodePath(LIBMTP_track_t *track, const QString &path, const QString &store)
{
    return QChar('{')+QString::number(track->storage_id)+QChar('/')+QString::number(track->parent_id)+QChar('/')+QString::number(track->item_id)+
           (store.isEmpty() ? QString() : (QChar('/')+store))+QChar('}')+path;
}

static Path decodePath(const QString &path)
{
    Path p;
    if (path.startsWith(QChar('{')) && path.contains(QChar('}'))) {
        int end=path.indexOf(QChar('}'));
        QStringList details=path.mid(1, end-1).split(QChar('/'));
        if (details.count()>=3) {
            p.storage=details.at(0).toUInt();
            p.parent=details.at(1).toUInt();
            p.id=details.at(2).toUInt();
        }
        p.path=path.mid(end+1);
    } else {
        p.path=path;
    }

    return p;
}

void MtpConnection::updateLibrary(const DeviceOptions &opts)
{
    if (!isConnected()) {
        connectToDevice();
    }

    destroyData();

    if (!isConnected()) {
        emit libraryUpdated();
        return;
    }

    #ifdef TIME_MTP_OPERATIONS
    QElapsedTimer timer;
    QElapsedTimer totalTimer;
    timer.start();
    totalTimer.start();
    #endif

    library = new MusicLibraryItemRoot;
    emit statusMessage(tr("Updating folders..."));
    updateFilesAndFolders();
    if (abortRequested) {
        return;
    }
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Folder update:" << timer.elapsed();
    timer.restart();
    #endif
    if (folderMap.isEmpty()) {
        destroyData();
        emit libraryUpdated();
        return;
    }
    #ifdef MTP_CLEAN_ALBUMS
    updateAlbums();
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Clean albums:" << timer.elapsed();
    timer.restart();
    #endif
    #endif
    emit statusMessage(tr("Updating tracks..."));
    lastListPercent=-1;
    LIBMTP_track_t *tracks=LIBMTP_Get_Tracklisting_With_Callback(device, &trackListMonitor, this);
    QMap<uint32_t, Folder>::ConstIterator folderEnd=folderMap.constEnd();
    QList<Storage>::Iterator store=storage.begin();
    QList<Storage>::Iterator storeEnd=storage.end();
    QMap<int, QString> storageNames;
    for (; store!=storeEnd; ++store) {
        setMusicFolder(*store);
        storageNames[(*store).id]=(*store).description;
    }
    if (abortRequested) {
        return;
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
    QMap<QString, MtpAlbum> albumMap;
    QMap<uint32_t, MtpFolder> folders;
    bool getAlbumArtistFromPath=opts.scheme.startsWith(DeviceOptions::constAlbumArtist+QChar('/')+DeviceOptions::constAlbumTitle+QChar('/'));
    #endif
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Tracks update:" << timer.elapsed();
    timer.restart();
    #endif
    while (tracks) {
        if (abortRequested) {
            return;
        }
        LIBMTP_track_t *track=tracks;
        tracks=tracks->next;

        QMap<uint32_t, Folder>::ConstIterator folder=folderMap.find(track->parent_id);
        if (folder==folderEnd) {
            // We only care about tracks in the music folder...
            LIBMTP_destroy_track_t(track);
            continue;
        }
        Song s;
        QString trackFilename=QString::fromUtf8(track->filename);
        s.id=track->item_id;
        s.file=encodePath(track, folder.value().path+trackFilename, storageNames.count()>1 ? storageNames[track->storage_id] : QString());
        s.album=QString::fromUtf8(track->album);
        s.artist=QString::fromUtf8(track->artist);
        s.albumartist=s.artist; // TODO: ALBUMARTIST: Read from 'track' when libMTP supports album artist!
        QString composer=QString::fromUtf8(track->composer);
        if (!composer.isEmpty()) {
            s.setComposer(composer);
        }
        s.year=QString::fromUtf8(track->date).mid(0, 4).toUInt();
        s.title=QString::fromUtf8(track->title);
        s.genres[0]=QString::fromUtf8(track->genre);
        s.track=track->tracknumber;
        s.time=(track->duration/1000.0)+0.5;
        s.size=track->filesize;
        s.fillEmptyFields();
        s.populateSorts();
        #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
        if (getAlbumArtistFromPath) {
            QStringList folderParts=(*folder).path.split('/', QString::SkipEmptyParts);
            if (folderParts.length()>=3) {
                // Path should be "Music/${AlbumArtist}/${Album}"
                int artistPath=1;
                int albumPath=2;
                // BubbleUPNP will download to "Music/Artist/${AlbumArtist}/${Album}" if it is configured to
                // download to "Music" and "Preserve folder structure" (At least this is the folder structure
                // from MiniDLNA...)
                if (folderParts.length()>=4 && QLatin1String("Artist")==folderParts.at(1)) {
                    artistPath++;
                    albumPath++;
                }
                MtpFolder folder(folderParts.at(artistPath), folderParts.at(albumPath));
                folders.insert(track->parent_id, folder);
                if (folder.album==s.album && Song::isVariousArtists(folder.artist)) {
                    s.albumartist=folder.artist;
                }
            }
        }
        #endif
        #ifdef MTP_TRACKNUMBER_FROM_FILENAME
        if (0==s.track) {
            int space=trackFilename.indexOf(' ');
            if (space>0 && space<=3) {
                s.track=trackFilename.mid(0, space).toInt();
            }
        }
        #endif
        if (!artistItem || (supprtAlbumArtistTag ? s.albumArtistOrComposer()!=artistItem->data() : s.artist!=artistItem->data())) {
            artistItem = library->artist(s);
        }
        if (!albumItem || albumItem->parentItem()!=artistItem || s.albumName()!=albumItem->data()) {
            albumItem = artistItem->album(s);
        }
        MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
        albumItem->append(songItem);

        #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
        // Store AlbumName->Artists/Songs mapping
        MtpAlbum &al=albumMap[s.album];
        al.artists.insert(s.artist);
        al.songs.append(songItem);
        al.folder=track->parent_id;
        #endif
        LIBMTP_destroy_track_t(track);
    }

    while (tracks) {
        LIBMTP_track_t *track=tracks;
        tracks=tracks->next;
        LIBMTP_destroy_track_t(track);
    }

    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Tracks parse:" << timer.elapsed();
    timer.restart();
    #endif
    #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
    // Use Album map to determine 'AlbumArtist' tag for various artist albums, and
    // albums that have tracks where artist is set to '${artist} and somebodyelse'
    QMap<QString, MtpAlbum>::ConstIterator it=albumMap.constBegin();
    QMap<QString, MtpAlbum>::ConstIterator end=albumMap.constEnd();
    for (; it!=end; ++it) {
        if (abortRequested) {
            return;
        }
        if ((*it).artists.count()>1) {
            QSet<quint16> tracks;
            QString shortestArtist;
            bool duplicateTrackNumbers=false;
            for (const MusicLibraryItemSong *s: (*it).songs) {
                if (tracks.contains(s->track())) {
                    duplicateTrackNumbers=true;
                    break;
                } else {
                    if (shortestArtist.isEmpty() || s->song().artist.length()<shortestArtist.length()) {
                        shortestArtist=s->song().artist;
                    }
                    tracks.insert(s->track());
                }
            }
            // If an album has mutiple tracks with the same track number, then we probably have X albums
            // by X artists - in which case we proceeed no further.
            if (!duplicateTrackNumbers) {
                MtpFolder &f=folders[(*it).folder];
                // Now, check to see if all artists contain 'shortestArtist'. If so then use 'shortestArtist' as the album
                // artist. This is probably due to songs which have artist set to '${artist} and somebodyelse'
                QString albumArtist=shortestArtist;
                for (const QString &artist: (*it).artists) {
                    if (!artist.contains(shortestArtist)) {
                        // Found an artist that did not contain 'shortestArtist', so use 'Various Artists' for album artist
                        albumArtist=!f.artist.isEmpty() && f.album==it.key() ? f.artist : Song::variousArtists();
                        break;
                    }
                }

                // Now move songs to correct artist/album...
                for (MusicLibraryItemSong *s: (*it).songs) {
                    if (s->song().albumartist==albumArtist) {
                        continue;
                    }
                    Song song=s->song();
                    song.albumartist=albumArtist;
                    artistItem=library->artist(song);
                    albumItem=artistItem->album(song);
                    MusicLibraryItemSong *songItem = new MusicLibraryItemSong(song, albumItem);
                    albumItem->append(songItem);
                    MusicLibraryItemAlbum *prevAlbum=(MusicLibraryItemAlbum *)s->parentItem();
                    prevAlbum->remove(s);
                    if (0==prevAlbum->childCount()) {
                        // Album no longer has any songs, so remove!
                        MusicLibraryItemArtist *prevArtist=(MusicLibraryItemArtist *)prevAlbum->parentItem();
                        prevArtist->remove(prevAlbum);
                        if (0==prevArtist->childCount()) {
                            // Artist no longer has any albums, so remove!
                            library->remove(prevArtist);
                        }
                    }
                }
            }
        }
    }
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "AlbumArtist detection:" << timer.elapsed();
    timer.restart();
    #endif
    #endif
    if (abortRequested) {
        return;
    }
    #ifdef TIME_MTP_OPERATIONS
    qWarning() << "Grouping:" << timer.elapsed();
    qWarning() << "TOTAL update:" <<totalTimer.elapsed();
    #endif
    emit libraryUpdated();
}

void MtpConnection::setMusicFolder(Storage &store)
{
    if (0==store.musicFolderId) {
        store.musicFolderId=getFolder(constMusicFolder+QLatin1Char('/'), store.id);
        if (0==store.musicFolderId) {
            store.musicFolderId=createFolder(constMusicFolder, constMusicFolder+QLatin1Char('/'), 0, store.id);
        }
        if (0!=store.musicFolderId) {
            store.musicPath=getPath(store.musicFolderId);
        }
    }
}

void MtpConnection::updateFilesAndFolders()
{
    folderMap.clear();
    for (const Storage &st: storage) {
        if (abortRequested) {
            return;
        }
        listFolder(st.id, constRootFolder, 0);
    }
}

void MtpConnection::listFolder(uint32_t storage, uint32_t parentDir, Folder *f)
{
    LIBMTP_file_t *files=LIBMTP_Get_Files_And_Folders(device, storage, parentDir);

    while (files && !abortRequested) {
        LIBMTP_file_t *file=files;
        files = files->next;

        QString name=QString::fromUtf8(file->filename);
        if (LIBMTP_FILETYPE_FOLDER==file->filetype) {
            bool isMusic=constRootFolder!=parentDir || 0==name.compare(constMusicFolder, Qt::CaseInsensitive);
            if (isMusic) {
                QMap<uint32_t, Folder>::ConstIterator it=folderMap.find(file->parent_id);
                QString path;
                if (it!=folderMap.constEnd()) {
                    path=it.value().path+name+QLatin1Char('/');
                } else {
                    path=name+QLatin1Char('/');
                }

                QMap<uint32_t, Folder>::iterator entry=folderMap.insert(file->item_id, Folder(path, file->item_id, file->parent_id, file->storage_id));
                if (folderMap.contains(file->parent_id)) {
                    folderMap[file->parent_id].folders.insert(file->item_id);
                }
                listFolder(storage, file->item_id, &(entry.value()));
            }
        } else if (f && constRootFolder!=parentDir) {
            if (name.endsWith(".jpg", Qt::CaseInsensitive) || name.endsWith(".png", Qt::CaseInsensitive) || QLatin1String("albumart.pamp")==name) {
                f->covers.insert(file->item_id, File(name, file->filesize, file->item_id));
            } else {
                f->files.insert(file->item_id, File(name, file->filesize, file->item_id));
            }
        }
        LIBMTP_destroy_file_t(file);
    }
}

void MtpConnection::updateStorage()
{
    uint64_t sizeCalc=0;
    uint64_t usedCalc=0;
    if (device && LIBMTP_ERROR_NONE==LIBMTP_Get_Storage(device, LIBMTP_STORAGE_SORTBY_MAXSPACE)) {
        LIBMTP_devicestorage_struct *s=device->storage;
        while (s) {
            QString volumeIdentifier=QString::fromUtf8(s->VolumeIdentifier);
            if (volumeIdentifier.isEmpty()) {
                volumeIdentifier=QString::fromUtf8(s->StorageDescription);
            }
            if (volumeIdentifier.isEmpty()) {
                volumeIdentifier="ID:"+QString::number(s->id);
            }
            QList<Storage>::Iterator it=storage.begin();
            QList<Storage>::Iterator end=storage.end();
            for ( ;it!=end; ++it) {
                if ((*it).volumeIdentifier==volumeIdentifier) {
                    // We know about this storage ID, so update its size...
                    (*it).size=s->MaxCapacity;
                    (*it).used=s->MaxCapacity-s->FreeSpaceInBytes;
                    break;
                }
            }

            if (it==end) {
                // Unknown storage ID, so add to list!
                Storage store;
                store.id=s->id;
                store.description=QString::fromUtf8(s->StorageDescription);
                store.volumeIdentifier=QString::fromUtf8(s->VolumeIdentifier);
                store.size=s->MaxCapacity;
                store.used=s->MaxCapacity-s->FreeSpaceInBytes;
                storage.append(store);
            }
            sizeCalc+=s->MaxCapacity;
            usedCalc+=s->MaxCapacity-s->FreeSpaceInBytes;
            s=s->next;
        }
    }
    size=sizeCalc;
    used=usedCalc;
}

QList<DeviceStorage> MtpConnection::getStorageList() const
{
    QList<DeviceStorage> s;
    QList<Storage>::ConstIterator it=storage.constBegin();
    QList<Storage>::ConstIterator end=storage.constEnd();
    for ( ;it!=end; ++it) {
        DeviceStorage store;
        store.size=(*it).size;
        store.used=(*it).used;
        store.description=(*it).description;
        store.volumeIdentifier=(*it).volumeIdentifier;
        s.append(store);
    }

    return s;
}

MtpConnection::Storage & MtpConnection::getStorage(const QString &volumeIdentifier)
{
    QList<Storage>::Iterator first=storage.begin();
    if (!volumeIdentifier.isEmpty()) {
        QList<Storage>::Iterator it=first;
        QList<Storage>::Iterator end=storage.end();
        for ( ;it!=end; ++it) {
            if ((*it).volumeIdentifier==volumeIdentifier) {
                return *it;
            }
        }
    }

    return *first;
}

MtpConnection::Storage & MtpConnection::getStorage(uint32_t id)
{
    QList<Storage>::Iterator first=storage.begin();
    QList<Storage>::Iterator it=first;
    QList<Storage>::Iterator end=storage.end();
    for ( ;it!=end; ++it) {
        if ((*it).id==id) {
            return *it;
        }
    }

    return *first;
}

uint32_t MtpConnection::createFolder(const QString &name, const QString &fullPath, uint32_t parentId, uint32_t storageId)
{
    char *nameCopy = qstrdup(name.toUtf8().constData());
    uint32_t newFolderId=LIBMTP_Create_Folder(device, nameCopy, parentId, storageId);
    delete nameCopy;
    if (0==newFolderId)  {
        return 0;
    }

    folderMap.insert(newFolderId, Folder(fullPath, newFolderId, parentId, storageId));
    return newFolderId;
}

uint32_t MtpConnection::getFolder(const QString &path, uint32_t storageId)
{
    QMap<uint32_t, Folder>::ConstIterator it=folderMap.constBegin();
    QMap<uint32_t, Folder>::ConstIterator end=folderMap.constEnd();

    for (; it!=end; ++it) {
        if (storageId==(*it).storageId && 0==(*it).path.compare(path, Qt::CaseInsensitive)) {
            return (*it).id;
        }
    }
    return 0;
}

QString MtpConnection::getPath(uint32_t folderId)
{
    return folderMap.contains(folderId) ? folderMap[folderId].path : QString();
}

uint32_t MtpConnection::checkFolderStructure(const QStringList &dirs, Storage &store)
{
    setMusicFolder(store);
    QString path;
    uint32_t parentId=store.musicFolderId;

    for (const QString &d: dirs) {
        path+=d+QChar('/');
        uint32_t folderId=getFolder(path, store.id);
        if (0==folderId) {
            folderId=createFolder(d, path, parentId, store.id);
            if (0==folderId) {
                return parentId;
            } else {
                parentId=folderId;
            }
        } else {
            parentId=folderId;
        }
    }
    return parentId;
}

static char * createString(const QString &str)
{
    return str.isEmpty() ? qstrdup("") : qstrdup(str.toUtf8());
}

static LIBMTP_filetype_t mtpFileType(const QString &f)
{
    if (f.endsWith(".mp3", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_MP3;
    }
    if (f.endsWith(".ogg", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_OGG;
    }
    if (f.endsWith(".wma", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_WMA;
    }
    if (f.endsWith(".m4a", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_M4A; // LIBMTP_FILETYPE_MP4
    }
    if (f.endsWith(".aac", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_AAC;
    }
    if (f.endsWith(".flac", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_FLAC;
    }
    if (f.endsWith(".wav", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_WAV;
    }
    if (f.endsWith(".jpg", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_JPEG;
    }
    if (f.endsWith(".png", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_PNG;
    }
    return LIBMTP_FILETYPE_UNDEF_AUDIO;
}

static QTemporaryFile * saveImageToTemp(const QImage &img, const QString &name)
{
    QTemporaryFile *temp=new QTemporaryFile();

    int index=name.lastIndexOf('.');
    if (index>0) {
        temp=new QTemporaryFile("cantata_XXXXXX"+name.mid(index));
    } else {
        temp=new QTemporaryFile("cantata_XXXXXX");
    }
    img.save(temp);
    temp->close();
    return temp;
}

void MtpConnection::putSong(const Song &s, bool fixVa, const DeviceOptions &opts, bool overwrite, bool copyCover)
{
    int status=Device::Failed;
    bool fixedVa=false;
    bool embedCoverImage=Device::constEmbedCover==opts.coverName;
    bool copiedCover=false;
    LIBMTP_track_t *meta=0;
    QString destName;
    Storage store=getStorage(opts.volumeId);
    uint32_t folderId=0;

    copyCover=copyCover && !embedCoverImage && Device::constNoCover!=opts.coverName;
    if (device) {
        meta=LIBMTP_new_track_t();
        meta->item_id=0;
        Song song=s;
        QString fileName=song.file;
        QTemporaryFile *temp=0;

        if (fixVa || embedCoverImage) {
            // Need to 'workaround' broken various artists handling, and/or embedding cover, so write to a temporary file first...
            temp=Device::copySongToTemp(song);
            if (temp) {
                if (embedCoverImage) {
                    Device::embedCover(song.file, song, opts.coverMaxSize);
                }
                if (fixVa && Device::fixVariousArtists(temp->fileName(), song, true)) {
                    fixedVa=true;
                }
                song.file=temp->fileName();
            }
        }
        struct stat statBuf;
        if (0==stat(QFile::encodeName(song.file).constData(), &statBuf)) {
            meta->filesize=statBuf.st_size;
            meta->modificationdate=statBuf.st_mtime;
        }

        // Check if storage has enough space, if not try to find one that does!
        if (store.freeSpace()<meta->filesize) {
            QList<Storage>::Iterator it=storage.begin();
            QList<Storage>::Iterator end=storage.end();
            for ( ;it!=end; ++it) {
                if ((*it).freeSpace()>=meta->filesize) {
                    store=*it;
                    break;
                }
            }
        }

        meta->parent_id=folderId=store.musicFolderId;
        meta->storage_id=store.id;
        destName=store.musicPath+opts.createFilename(song);
        QStringList dirs=destName.split('/', QString::SkipEmptyParts);
        if (dirs.count()>1) {
            destName=dirs.takeLast();
            meta->parent_id=folderId=checkFolderStructure(dirs, store);
        }
        Folder &folder=folderMap[folderId];
        QMap<uint32_t, File>::ConstIterator it=folder.files.constBegin();
        QMap<uint32_t, File>::ConstIterator end=folder.files.constEnd();
        for (; it!=end; ++it) {
            if ((*it).name==destName) {
                if (!overwrite || 0!=LIBMTP_Delete_Object(device, (*it).id)) {
                    status=Device::SongExists;
                }
                break;
            }
        }

        if (status!=Device::SongExists) {
            meta->title=createString(song.title);
            meta->artist=createString(song.artist);
            meta->composer=createString(song.composer());
            meta->genre=createString(song.genres[0]);
            meta->album=createString(song.album);
            meta->date=createString(QString().sprintf("%4d0101T0000.0", song.year));
            meta->filename=createString(destName);
            meta->tracknumber=song.track;
            meta->duration=song.time*1000;
            meta->rating=0;
            meta->usecount=0;
            meta->filetype=mtpFileType(song.file);
            meta->next=0;
            switch (LIBMTP_Send_Track_From_File(device, fileName.toUtf8(), meta, &progressMonitor, this)) {
            case LIBMTP_ERROR_NONE:         status=Device::Ok;      break;
            case LIBMTP_ERROR_STORAGE_FULL: status=Device::NoSpace; break;
            default:                        status=Device::Failed;  break;
            }
        }
        if (temp) {
            // Delete the temp file...
            temp->remove();
            delete temp;
        }
    }

    if (Device::Ok==status) {
        Folder &folder=folderMap[folderId];
        // LibMTP seems to reset parent_id to 0 - but we NEED the correct value for encodePath
        meta->parent_id=folderId;
        if (copyCover) {
            QMap<uint32_t, File>::ConstIterator it=folder.covers.constBegin();
            QMap<uint32_t, File>::ConstIterator end=folder.covers.constEnd();

            for (; it!=end; ++it) {
                if (it.value().name==opts.coverName) {
                    copiedCover=true;
                    copyCover=false;
                    break;
                }
            }
        }
        // Send cover, as a plain file...
        if (copyCover) {
            QString srcFile;

            // If we have transcoded a song, the song.file will point to /tmp!!!
            Song orig = s;
            if (orig.hasExtraField(constOrigFileName)) {
                orig.file=orig.extraField(constOrigFileName);
            }
            Covers::Image coverImage=Covers::self()->getImage(orig);
            QTemporaryFile *temp=0;
            if (!coverImage.img.isNull() && !coverImage.fileName.isEmpty()) {
                if (opts.coverMaxSize && (coverImage.img.width()>(int)opts.coverMaxSize || coverImage.img.height()>(int)opts.coverMaxSize)) {
                    temp=saveImageToTemp(coverImage.img.scaled(QSize(opts.coverMaxSize, opts.coverMaxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation), opts.coverName);
                } else if (!coverImage.fileName.endsWith(".jpg", Qt::CaseInsensitive) || !QFile::exists(coverImage.fileName)) {
                    temp=saveImageToTemp(coverImage.img, opts.coverName);
                } else {
                    srcFile=coverImage.fileName;
                }
                if (temp) {
                    srcFile=temp->fileName();
                }
            }

            if (!srcFile.isEmpty()) {
                LIBMTP_file_t *fileMeta=LIBMTP_new_file_t();
                fileMeta->item_id=0;
                fileMeta->parent_id=folderId;
                fileMeta->storage_id=store.id;
                fileMeta->filename=createString(opts.coverName);
                struct stat statBuf;
                if (0==stat(QFile::encodeName(srcFile).constData(), &statBuf)) {
                    fileMeta->filesize=statBuf.st_size;
                    fileMeta->modificationdate=statBuf.st_mtime;
                }
                meta->filetype=mtpFileType(opts.coverName);

                if (0==LIBMTP_Send_File_From_File(device, srcFile.toUtf8(), fileMeta, 0, 0)) {
                    folder.covers.insert(fileMeta->item_id, File(opts.coverName, statBuf.st_size, fileMeta->item_id));
                    copiedCover=true;
                }
                LIBMTP_destroy_file_t(fileMeta);
            }
            if (temp) {
                temp->remove();
                delete temp;
            }
        }
        folder.files.insert(meta->item_id, File(destName, meta->filesize, meta->item_id));
        updateStorage();
    }
    emit putSongStatus(status,
                       meta ? encodePath(meta, destName, storage.count()>1 ? store.description : QString()) : QString(),
                       fixedVa, copiedCover);
    if (meta) {
        LIBMTP_destroy_track_t(meta);
    }
}

MtpConnection::File MtpConnection::getCoverDetils(const Song &s)
{
    File cover;
    Path path=decodePath(s.file);
    if (path.ok() && folderMap.contains(path.parent) && !folderMap[path.parent].covers.isEmpty()) {
        QMap<uint32_t, File> &covers=folderMap[path.parent].covers;
        QMap<uint32_t, File>::ConstIterator it=covers.constBegin();
        QMap<uint32_t, File>::ConstIterator end=covers.constEnd();

        for (; it!=end; ++it) {
            if (it.value().size>cover.size) {
                cover=it.value();
            }
        }
    }

    return cover;
}

void MtpConnection::getSong(const Song &song, const QString &dest, bool fixVa, bool copyCover)
{
    bool copiedSong=device && 0==LIBMTP_Get_File_To_File(device, song.id, dest.toUtf8(), &progressMonitor, this);
    bool copiedCover=false;

    if (copiedSong && !abortRequested && copyCover) {
        QString destDir=Utils::getDir(dest);

        if (QDir(destDir).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable).isEmpty()) {
            File cover=getCoverDetils(song);

            if (0!=cover.id) {
                QString fileName=QString(destDir+Covers::albumFileName(song)+(cover.name.endsWith(".jpg", Qt::CaseInsensitive) ? ".jpg" : ".png"));
                QByteArray fileNameUtf8=fileName.toUtf8();
                copiedCover=0==LIBMTP_Get_File_To_File(device, cover.id, fileNameUtf8.constData(), 0, 0);
                if (copiedCover) {
                    Utils::setFilePerms(fileName);
                }
            }
        }
    }

    if (copiedSong && fixVa && !abortRequested) {
        Song s(song);
        Device::fixVariousArtists(dest, s, false);
    }
    emit getSongStatus(copiedSong, copiedCover);
}

void MtpConnection::delSong(const Song &song)
{
    Path path=decodePath(song.file);
    bool deletedSong=device && path.ok() && 0==LIBMTP_Delete_Object(device, path.id);
    if (deletedSong) {
        folderMap[path.parent].files.remove(path.id);
        #ifdef MTP_CLEAN_ALBUMS
        // Remove track from album. Remove album (and cover) if no tracks.
        LIBMTP_album_t *album=getAlbum(song);
        if (album) {
            if (0==album->no_tracks || (1==album->no_tracks && album->tracks[0]==(uint32_t)song.id)) {
                LIBMTP_Delete_Object(device, album->album_id);
                updateAlbums();
            } else if (album->no_tracks>1) {
                // Remove track from album...
                uint32_t *tracks = (uint32_t *)malloc((album->no_tracks-1)*sizeof(uint32_t));
                if (tracks) {
                    bool found=false;
                    for (uint32_t i=0, j=0; i<album->no_tracks && j<(album->no_tracks-1); ++i) {
                        if (album->tracks[i]!=(uint32_t)song.id) {
                            tracks[j++]=album->tracks[i];
                        } else {
                            found=true;
                        }
                    }
                    if (found) {
                        album->no_tracks--;
                        free(album->tracks);
                        album->tracks = tracks;
                        LIBMTP_Update_Album(device, album);
                    } else {
                        free(tracks);
                    }
                }
            }
        }
        #endif
        updateStorage();
    }
    emit delSongStatus(deletedSong);
}

bool MtpConnection::removeFolder(uint32_t folderId)
{
    QMap<uint32_t, Folder>::iterator folder=folderMap.find(folderId);
    if (folderMap.end()!=folder && (*folder).folders.isEmpty() && (*folder).files.isEmpty()) {
        // Delete any cover files...
        QList<uint32_t> toRemove=(*folder).covers.keys();
        for (uint32_t cover: toRemove) {
            if (0==LIBMTP_Delete_Object(device, cover)) {
                (*folder).covers.remove(cover);
            }
        }

        // Delete folder, if it is now empty...
        if ((*folder).covers.isEmpty() && 0==LIBMTP_Delete_Object(device, folderId)) {
            if (folderMap.contains((*folder).parentId)) {
                folderMap[(*folder).parentId].folders.remove(folderId);
            }
            folderMap.remove(folderId);
            return true;
        }
    }
    return false;
}

void MtpConnection::cleanDirs(const QSet<QString> &dirs)
{
    for (const QString &d: dirs) {
        Path path=decodePath(d);
        Storage &store=getStorage(path.storage);
        if (0==store.musicFolderId) {
            continue;
        }
        uint32_t folderId=path.parent;
        while (0!=folderId && folderId!=store.musicFolderId) {
            QMap<uint32_t, Folder>::iterator it=folderMap.find(folderId);
            if (it!=folderMap.end()) {
                if (removeFolder(folderId)) {
                    folderId=(*it).parentId;
                    folderMap.erase(it);
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    updateStorage();
    emit cleanDirsStatus(true);
}

void MtpConnection::getCover(const Song &song)
{
    File c=getCoverDetils(song);

    COVER_DBUG << c.name << c.id;

    if (0!=c.id) {
        QByteArray data;
        if (0==LIBMTP_Get_File_To_Handler(device, c.id, fileReceiver, &data, 0, 0)) {
            QImage img;
            if (img.loadFromData(data)) {
                COVER_DBUG << "loaded cover";
                emit cover(song, img);
            }
        }
    }
}

void MtpConnection::stop()
{
    if (thread) {
        disconnectFromDevice(false);
        thread->stop();
        thread=0;
    }
}

#ifdef MTP_CLEAN_ALBUMS
void MtpConnection::updateAlbums()
{
    while (albums) {
        LIBMTP_album_t *album=albums;
        albums=albums->next;
        LIBMTP_destroy_album_t(album);
    }
    albums=LIBMTP_Get_Album_List(device);
}

LIBMTP_album_t * MtpConnection::getAlbum(const Song &song)
{
    LIBMTP_album_t *al=albums;
    LIBMTP_album_t *possible=0;

    while (al) {
        if (QString::fromUtf8(al->name)==song.album) {
            // For some reason, MTP sometimes leaves blank albums behind.
            // So, when looking for an album if we find one with the same name, but no tracks - then save this as a possibility...
            if (0==al->no_tracks) {
                QString aa(QString::fromUtf8(al->artist));
                if (aa.isEmpty() || aa==song.albumArtist()) {
                    possible=al;
                }
            } else {
                for (uint32_t i=0; i<al->no_tracks; ++i) {
                    if (al->tracks[i]==(uint32_t)song.id) {
                        return al;
                    }
                }
            }
        }
        al=al->next;
    }

    return possible;
}
#endif

void MtpConnection::destroyData()
{
    folderMap.clear();
    if (library) {
        delete library;
        library=0;
    }
    
    #ifdef MTP_CLEAN_ALBUMS
    while (albums) {
        LIBMTP_album_t *album=albums;
        albums=albums->next;
        LIBMTP_destroy_album_t(album);
    }
    #endif
}

QString cfgKey(Solid::Device &dev, const QString &serial)
{
    QString key=QLatin1String("MTP-")+dev.vendor()+QChar('-')+dev.product()+QChar('-')+serial;
    key.replace('/', '_');
    return key;
}

MtpDevice::MtpDevice(MusicLibraryModel *m, Solid::Device &dev, unsigned int busNum, unsigned int devNum)
    : Device(m, dev,
             #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
             true
             #else
             false
             #endif
             )
    , pmp(dev.as<Solid::PortableMediaPlayer>())
    , tempFile(0)
    , mtpUpdating(false)
{
    static bool registeredTypes=false;
    if (!registeredTypes) {
        qRegisterMetaType<QSet<QString> >("QSet<QString>");
        qRegisterMetaType<DeviceOptions >("DeviceOptions");
        registeredTypes=true;
    }

    connection=new MtpConnection(data(), busNum, devNum, supportsAlbumArtistTag());
    connect(this, SIGNAL(updateLibrary(const DeviceOptions &)), connection, SLOT(updateLibrary(const DeviceOptions &)));
    connect(connection, SIGNAL(libraryUpdated()), this, SLOT(libraryUpdated()));
    connect(connection, SIGNAL(progress(int)), this, SLOT(emitProgress(int)));
    connect(this, SIGNAL(putSong(const Song &, bool, const DeviceOptions &, bool, bool)), connection, SLOT(putSong(const Song &, bool, const DeviceOptions &, bool, bool)));
    connect(connection, SIGNAL(putSongStatus(int, const QString &, bool, bool)), this, SLOT(putSongStatus(int, const QString &, bool, bool)));
    connect(this, SIGNAL(getSong(const Song &, const QString &, bool, bool)), connection, SLOT(getSong(const Song &, const QString &, bool, bool)));
    connect(connection, SIGNAL(getSongStatus(bool, bool)), this, SLOT(getSongStatus(bool, bool)));
    connect(this, SIGNAL(delSong(const Song &)), connection, SLOT(delSong(const Song &)));
    connect(connection, SIGNAL(delSongStatus(bool)), this, SLOT(delSongStatus(bool)));
    connect(this, SIGNAL(cleanMusicDirs(const QSet<QString> &)), connection, SLOT(cleanDirs(const QSet<QString> &)));
    connect(this, SIGNAL(getCover(const Song &)), connection, SLOT(getCover(const Song &)));
    connect(connection, SIGNAL(cleanDirsStatus(bool)), this, SLOT(cleanDirsStatus(bool)));
    connect(connection, SIGNAL(statusMessage(const QString &)), this, SLOT(setStatusMessage(const QString &)));
    connect(connection, SIGNAL(deviceDetails(const QString &)), this, SLOT(deviceDetails(const QString &)));
    connect(connection, SIGNAL(updatePercentage(int)), this, SLOT(updatePercentage(int)));
    connect(connection, SIGNAL(cover(const Song &, const QImage &)), this, SIGNAL(cover(const Song &, const QImage &)));
    opts.fixVariousArtists=false;
    opts.coverName=constMtpDefaultCover;
    QTimer::singleShot(0, this, SLOT(rescan(bool)));
    defaultName=data();
    if (!opts.name.isEmpty()) {
        DBUG << "setName" << opts.name;
        setData(opts.name);
    }
    icn=MonoIcon::icon(FontAwesome::mobilephone, Utils::monoIconColor());
}

MtpDevice::~MtpDevice()
{
    stop();
}

void MtpDevice::deviceDetails(const QString &s)
{
    DBUG << s;
    if ((s!=serial || serial.isEmpty()) && solidDev.isValid()) {
        serial=s;
        QString configKey=cfgKey(solidDev, serial);
        opts.load(configKey);
        configured=Configuration().hasGroup(configKey);
        if (!opts.name.isEmpty() && opts.name!=defaultName) {
            DBUG << "setName" << opts.name;
            setData(opts.name);
            emit renamed();
        }
    }
}

bool MtpDevice::isConnected() const
{
    return solidDev.isValid() && pmp && pmp->isValid() && connection->isConnected();
}

void MtpDevice::stop()
{
    abortJob();
    deleteTemp();
    if (connection) {
        disconnect(connection, SIGNAL(libraryUpdated()), this, SLOT(libraryUpdated()));
        disconnect(connection, SIGNAL(progress(int)), this, SLOT(emitProgress(int)));
        disconnect(connection, SIGNAL(putSongStatus(int, const QString &, bool, bool)), this, SLOT(putSongStatus(int, const QString &, bool, bool)));
        disconnect(connection, SIGNAL(getSongStatus(bool, bool)), this, SLOT(getSongStatus(bool, bool)));
        disconnect(connection, SIGNAL(delSongStatus(bool)), this, SLOT(delSongStatus(bool)));
        disconnect(connection, SIGNAL(cleanDirsStatus(bool)), this, SLOT(cleanDirsStatus(bool)));
        disconnect(connection, SIGNAL(statusMessage(const QString &)), this, SLOT(setStatusMessage(const QString &)));
        disconnect(connection, SIGNAL(deviceDetails(const QString &)), this, SLOT(deviceDetails(const QString &)));
        disconnect(connection, SIGNAL(updatePercentage(int)), this, SLOT(updatePercentage(int)));
        disconnect(connection, SIGNAL(cover(const Song &, const QImage &)), this, SIGNAL(cover(const Song &, const QImage &)));
        metaObject()->invokeMethod(connection, "stop", Qt::QueuedConnection);
        connection->deleteLater();
        connection=0;
    }
}

void MtpDevice::configure(QWidget *parent)
{
    if (!isIdle()) {
        return;
    }

    DevicePropertiesDialog *dlg=new DevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const DeviceOptions &)), SLOT(saveProperties(const QString &, const DeviceOptions &)));
    if (!configured) {
        connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
    }
    DeviceOptions o=opts;
    if (o.name.isEmpty()) {
        o.name=data();
    }
    dlg->show(QString(), o, connection->getStorageList(), DevicePropertiesWidget::Prop_Name|DevicePropertiesWidget::Prop_CoversAll|DevicePropertiesWidget::Prop_Va|DevicePropertiesWidget::Prop_Transcoder);
}

void MtpDevice::rescan(bool full)
{
    Q_UNUSED(full)
    if (mtpUpdating || !solidDev.isValid()) {
        return;
    }
    if (childCount()) {
        update=new MusicLibraryItemRoot();
        applyUpdate();
    }
    mtpUpdating=true;
    emit updating(solidDev.udi(), true);
    emit updateLibrary(opts);
}

void MtpDevice::addSong(const Song &s, bool overwrite, bool copyCover)
{
    requestAbort(false);
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    needToFixVa=opts.fixVariousArtists && s.isVariousArtists();

    if (!overwrite) {
        Song check=s;

        if (needToFixVa) {
            Device::fixVariousArtists(QString(), check, true);
        }
        if (songExists(check)) {
            emit actionStatus(SongExists);
            return;
        }
    }

    if (!QFile::exists(s.file)) {
        emit actionStatus(SourceFileDoesNotExist);
        return;
    }
    currentSong=s;

    Encoders::Encoder encoder;

    if (!opts.transcoderCodec.isEmpty()) {
        encoder=Encoders::getEncoder(opts.transcoderCodec);
        if (encoder.codec.isEmpty()) {
            emit actionStatus(CodecNotAvailable);
            return;
        }
    }

    transcoding = !opts.transcoderCodec.isEmpty() &&
            (DeviceOptions::TW_IfDifferent!=opts.transcoderWhen || encoder.isDifferent(s.file)) &&
            (DeviceOptions::TW_IfLossess!=opts.transcoderWhen || Device::isLossless(s.file));

    if (transcoding) {
        deleteTemp();
        tempFile=new QTemporaryFile("cantata_XXXXXX."+encoder.extension);
        tempFile->setAutoRemove(false);

        if (!tempFile->open()) {
            deleteTemp();
            emit actionStatus(FailedToCreateTempFile);
            return;
        }
        QString destFile=tempFile->fileName();
        tempFile->close();
        if (QFile::exists(destFile)) {
            QFile::remove(destFile);
        }
        transcoding=true;
        TranscodingJob *job=new TranscodingJob(encoder, opts.transcoderValue, s.file, destFile);
        job->setProperty("overwrite", overwrite);
        job->setProperty("copyCover", copyCover);
        connect(job, SIGNAL(result(int)), SLOT(transcodeSongResult(int)));
        connect(job, SIGNAL(percent(int)), SLOT(transcodePercent(int)));
        job->start();
        currentSong.setExtraField(constOrigFileName, currentSong.file);
        currentSong.file=destFile;
    } else {
        emit putSong(currentSong, needToFixVa, opts, overwrite, copyCover);
    }
}

void MtpDevice::copySongTo(const Song &s, const QString &musicPath, bool overwrite, bool copyCover)
{
    requestAbort(false);
    transcoding=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    needToFixVa=opts.fixVariousArtists && s.isVariousArtists();

    if (!overwrite) {
        Song check=s;

        if (needToFixVa) {
            Device::fixVariousArtists(QString(), check, false);
        }
        if (MpdLibraryModel::self()->songExists(check)) {
            emit actionStatus(SongExists);
            return;
        }
    }

    if (!songExists(s)) {
        emit actionStatus(SongDoesNotExist);
        return;
    }

    QString baseDir=MPDConnection::self()->getDetails().dir;
    currentDestFile=baseDir+musicPath;
    QDir dir(Utils::getDir(currentDestFile));
    if (!dir.exists() && !Utils::createWorldReadableDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    emit getSong(s, currentDestFile, needToFixVa, copyCover);
}

void MtpDevice::removeSong(const Song &s)
{
    requestAbort(false);
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    if (!songExists(s)) {
        emit actionStatus(SongDoesNotExist);
        return;
    }

    currentSong=s;
    emit delSong(s);
}

void MtpDevice::cleanDirs(const QSet<QString> &dirs)
{
    requestAbort(false);
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }
    emit cleanMusicDirs(dirs);
}

Covers::Image MtpDevice::requestCover(const Song &song)
{
    COVER_DBUG << song.file;
    requestAbort(false);
    if (isConnected()) {
        COVER_DBUG << "Get cover from connection";
        emit getCover(song);
    }
    return Covers::Image();
}

void MtpDevice::putSongStatus(int status, const QString &file, bool fixedVa, bool copiedCover)
{
    deleteTemp();
    if (jobAbortRequested) {
        return;
    }
    if (Ok!=status) {
        emit actionStatus(status);
    } else {
        currentSong.file=file;
        if (needToFixVa && fixedVa) {
            currentSong.fixVariousArtists();
        }
        #ifndef MTP_FAKE_ALBUMARTIST_SUPPORT
        else if (!opts.fixVariousArtists) { // TODO: ALBUMARTIST: Remove when libMTP supports album artist!
            currentSong.albumartist=currentSong.artist;
        }
        #endif
        addSongToList(currentSong);
        emit actionStatus(Ok, copiedCover);
    }
}

void MtpDevice::transcodeSongResult(int status)
{
    TranscodingJob *job=qobject_cast<TranscodingJob *>(sender());
    if (!job) {
        return;
    }
    FileJob::finished(job);
    if (jobAbortRequested) {
        deleteTemp();
        return;
    }
    if (Ok!=status) {
        emit actionStatus(status);
    } else {
        emit putSong(currentSong, needToFixVa, opts, job->property("overwrite").toBool(), job->property("copyCover").toBool());
    }
}

void MtpDevice::transcodePercent(int percent)
{
    if (jobAbortRequested) {
        FileJob *job=qobject_cast<FileJob *>(sender());
        if (job) {
            job->stop();
        }
        return;
    }
    emit progress(percent/2);
}

void MtpDevice::emitProgress(int percent)
{
    if (jobAbortRequested) {
        return;
    }
    emit progress(transcoding ? (50+(percent/2)) : percent);
}

void MtpDevice::getSongStatus(bool ok, bool copiedCover)
{
    if (jobAbortRequested) {
        return;
    }
    if (!ok) {
        emit actionStatus(Failed);
    } else {
        currentSong.file=currentDestFile.mid(MPDConnection::self()->getDetails().dir.length());
        QString origPath;
        if (MPDConnection::self()->isMopidy()) {
            origPath=currentSong.file;
            currentSong.file=Song::encodePath(currentSong.file);
        }
        if (needToFixVa) {
            currentSong.revertVariousArtists();
        }
        Utils::setFilePerms(currentDestFile);
//        MusicLibraryModel::self()->addSongToList(currentSong);
//        DirViewModel::self()->addFileToList(origPath.isEmpty() ? currentSong.file : origPath,
//                                            origPath.isEmpty() ? QString() : currentSong.file);
        emit actionStatus(Ok, copiedCover);
    }
}

void MtpDevice::delSongStatus(bool ok)
{
    if (jobAbortRequested) {
        return;
    }
    if (!ok) {
        emit actionStatus(Failed);
    } else {
        removeSongFromList(currentSong);
        emit actionStatus(Ok);
    }
}

void MtpDevice::cleanDirsStatus(bool ok)
{
    emit actionStatus(ok ? Ok : Failed);
}

double MtpDevice::usedCapacity()
{
    if (!isConnected()) {
        return -1.0;
    }

    return connection->capacity()>0 ? (connection->usedSpace()*1.0)/(connection->capacity()*1.0) : -1.0;
}

QString MtpDevice::capacityString()
{
    if (!isConnected()) {
        return tr("Not Connected");
    }

    return tr("%1 free").arg(Utils::formatByteSize(connection->capacity()-connection->usedSpace()));
}

qint64 MtpDevice::freeSpace()
{
    if (!isConnected()) {
        return 0;
    }

    return connection->capacity()-connection->usedSpace();
}

void MtpDevice::libraryUpdated()
{
    if (update) {
        delete update;
    }
    update=connection->takeLibrary();
    setStatusMessage(QString());
    emit updating(id(), false);
    mtpUpdating=false;
}

void MtpDevice::saveProperties(const QString &, const DeviceOptions &newOpts)
{
    if (configured && opts==newOpts) {
        return;
    }

    QString newName=newOpts.name.isEmpty() ? defaultName : newOpts.name;
    bool diffName=opts.name!=newName;
    opts=newOpts;
    if (diffName) {
        DBUG << "setName" << newName;
        setData(newName);
    }
    saveProperties();
    if (diffName) {
        emit renamed();
    }
}

void MtpDevice::saveProperties()
{
    if (solidDev.isValid()) {
        configured=true;
        opts.save(cfgKey(solidDev, serial), false, true, false); // Dont save fileame scheme - cant be changed!
        emit configurationChanged();
    }
}

void MtpDevice::saveOptions()
{
    if (solidDev.isValid()) {
        opts.save(cfgKey(solidDev, serial), false, true, false); // Dont save fileame scheme - cant be changed!
    }
}

void MtpDevice::deleteTemp()
{
    if (tempFile) {
        tempFile->remove();
        delete tempFile;
        tempFile=0;
    }
}

#include "moc_mtpdevice.cpp"
