/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemroot.h"
#include "dirviewmodel.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "covers.h"
#include "song.h"
#include "encoders.h"
#include "transcodingjob.h"
#include "utils.h"
#include "mpdparseutils.h"
#include "localize.h"
#include "filejob.h"
#include "settings.h"
#include <QThread>
#include <QTimer>
#include <QDir>
#include <QTemporaryFile>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMimeType>
#include <solid/genericinterface.h>
#else
#include "solid-lite/genericinterface.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <QDebug>

#define MTP_FAKE_ALBUMARTIST_SUPPORT

#ifdef MTP_DEBUG
#include <QDebug>

void displayFolders(LIBMTP_folder_t *folder, const QString &parent="/")
{
    if (!folder) {
        return;
    }
    qWarning() << "FOLDER:" << parent+folder->name << "ID:" << folder->folder_id << "PID:" << folder->parent_id << "STOR:" << folder->storage_id;
    if (folder->child) {
        displayFolders(folder->child, parent+folder->name+"/");
    }
    if (folder->sibling) {
        displayFolders(folder->sibling, parent);
    }
}

void displayFiles(LIBMTP_file_t *file)
{
    if (!file) {
        return;
    }

    qWarning() << "FILE:" << file->filename << "ID:" << file->item_id << "PID:" << file->parent_id << "STOR:" << file->storage_id;
    if (file->next) {
        displayFiles(file->next);
    }
}

void displayAlbums(LIBMTP_album_t *album)
{
    if (!album) {
        return;
    }

    qWarning() << "ALBUM_NAME:" << album->name << "ARTIST:" << album->artist << "ID:" << album->album_id << "PID:" << album->parent_id << "STOR:" << album->storage_id;
    if (album->next) {
        displayAlbums(album->next);
    }
}

void displayTracks(LIBMTP_track_t *track)
{
    if (!track) {
        return;
    }

    qWarning() << "TRACK_NUMBER:" << track->tracknumber << "TRACK_TITLE:" << track->title << "ARTIST:" << track->artist << "ALBUM:" << track->album << "FILENAME:" << track->filename << "ID:" << track->item_id << "PID:" << track->parent_id << "STOR:" << track->storage_id;
    if (track->next) {
        displayTracks(track->next);
    }
}
#endif

static int progressMonitor(uint64_t const processed, uint64_t const total, void const * const data)
{
    ((MtpConnection *)data)->emitProgress((int)(((processed*1.0)/(total*1.0)*100.0)+0.5));
    return ((MtpConnection *)data)->abortRequested() ? -1 : 0;
}

static int trackListMonitor(uint64_t const processed, uint64_t const total, void const * const data)
{
    Q_UNUSED(total)
    ((MtpConnection *)data)->trackListProgress(processed);
    return ((MtpConnection *)data)->abortRequested() ? -1 : 0;
}

MtpConnection::MtpConnection(MtpDevice *p)
    : device(0)
    , library(0)
    , dev(p)
    , lastUpdate(0)
{
    size=0;
    used=0;
    LIBMTP_Init();
}

MtpConnection::~MtpConnection()
{
    disconnectFromDevice(false);
}

MusicLibraryItemRoot * MtpConnection::takeLibrary()
{
    MusicLibraryItemRoot *lib=library;
    library=0;
    return lib;
}

bool MtpConnection::abortRequested() const
{
    return dev->abortRequested();
}

void MtpConnection::emitProgress(int percent)
{
    emit progress(percent);
}

void MtpConnection::trackListProgress(uint64_t count)
{
    int t=time(NULL);
    if ((t-lastUpdate)>=2 || 0==(count%5)) {
        lastUpdate=t;
        emit songCount((int)count);
    }
}

void MtpConnection::connectToDevice()
{
    device=0;
    storage.clear();
    defaultMusicFolder=0;
    LIBMTP_raw_device_t *rawDevices=0;
    int numDev=-1;
    emit statusMessage(i18n("Connecting to device..."));
    if (LIBMTP_ERROR_NONE!=LIBMTP_Detect_Raw_Devices(&rawDevices, &numDev) || numDev<=0) {
        emit statusMessage(i18n("No devices found"));
        return;
    }

    LIBMTP_mtpdevice_t *mptDev=0;
    for (int i = 0; i < numDev; i++) {
        if (0!=dev->busNum && 0!=dev->devNum) {
            if (rawDevices[i].bus_location==dev->busNum && rawDevices[i].devnum==dev->devNum) {
                mptDev = LIBMTP_Open_Raw_Device(&rawDevices[i]);
                break;
            }
        } else if ((mptDev = LIBMTP_Open_Raw_Device(&rawDevices[i]))) {
            break;
        }
    }

    size=0;
    used=0;
    device=mptDev;
    updateStorage();
    free(rawDevices);
    if (!device) {
        emit statusMessage(i18n("No devices found"));
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
    emit statusMessage(i18n("Connected to device"));
}

void MtpConnection::disconnectFromDevice(bool showStatus)
{
    if (device) {
        destroyData();
        LIBMTP_Release_Device(device);
        device=0;
        if (showStatus) {
            emit statusMessage(i18n("Disconnected from device"));
        }
    }
}

#ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
struct MtpAlbum {
    QSet<QString> artists;
    QList<MusicLibraryItemSong *> songs;
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

void MtpConnection::updateLibrary()
{
    if (!isConnected()) {
        connectToDevice();
    }

    destroyData();

    if (!isConnected()) {
        emit libraryUpdated();
        return;
    }

    library = new MusicLibraryItemRoot;
    emit statusMessage(i18n("Updating folders..."));
    updateFolders();
    if (folderMap.isEmpty()) {
        destroyData();
        emit libraryUpdated();
        return;
    }
    emit statusMessage(i18n("Updating files..."));
    updateFiles();
    emit statusMessage(i18n("Updating tracks..."));
    #ifdef MTP_DEBUG
    displayFolders(LIBMTP_Get_Folder_List(device));
    displayFiles(LIBMTP_Get_Filelisting_With_Callback(device, 0, 0));
    displayAlbums(LIBMTP_Get_Album_List(device));
    displayTracks(LIBMTP_Get_Tracklisting_With_Callback(device, 0, 0));
    #endif
    lastUpdate=0;
    LIBMTP_track_t *tracks=LIBMTP_Get_Tracklisting_With_Callback(device, &trackListMonitor, this);
    LIBMTP_track_t *track=tracks;
    QMap<uint32_t, Folder>::ConstIterator folderEnd=folderMap.constEnd();
    QList<Storage>::Iterator it=storage.begin();
    QList<Storage>::Iterator end=storage.end();
    QMap<int, QString> storageNames;
    for (; it!=end; ++it) {
        setMusicFolder(*it);
        storageNames[(*it).id]=(*it).description;
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
    QMap<QString, MtpAlbum> albums;
    #endif

    while (track && !abortRequested()) {
        QMap<uint32_t, Folder>::ConstIterator it=folderMap.find(track->parent_id);
        if (it==folderEnd) {
            // We only care about tracks in the music folder...
            track=track->next;
            continue;
        }
        Song s;
        s.id=track->item_id;
        //if (it!=folderEnd) {
            s.file=encodePath(track, it.value().path+QString::fromUtf8(track->filename), storageNames.count()>1 ? storageNames[track->storage_id] : QString());
        //} else {
        //    s.file=encodePath(track, track->filename, storageNames.count()>1 ? storageNames[track->storage_id] : QString());
        //}
        s.album=QString::fromUtf8(track->album);
        s.artist=QString::fromUtf8(track->artist);
        s.albumartist=s.artist; // TODO: ALBUMARTIST: Read from 'track' when libMTP supports album artist!
        s.year=QString::fromUtf8(track->date).mid(0, 4).toUInt();
        s.title=QString::fromUtf8(track->title);
        s.genre=QString::fromUtf8(track->genre);
        s.track=track->tracknumber;
        s.time=(track->duration/1000.0)+0.5;
        s.fillEmptyFields();

        if (!artistItem || (dev->supportsAlbumArtistTag() ? s.albumArtist()!=artistItem->data() : s.album!=artistItem->data())) {
            artistItem = library->artist(s);
        }
        if (!albumItem || albumItem->parentItem()!=artistItem || s.album!=albumItem->data()) {
            albumItem = artistItem->album(s);
        }
        MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);
        albumItem->append(songItem);
        albumItem->addGenre(s.genre);
        artistItem->addGenre(s.genre);
        library->addGenre(s.genre);

        #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
        // Store AlbumName->Artists/Songs mapping
        MtpAlbum &al=albums[s.album];
        al.artists.insert(s.artist);
        al.songs.append(songItem);
        #endif
        track=track->next;
    }
    if (tracks) {
        LIBMTP_destroy_track_t(tracks);
    }
    if (!abortRequested()) {
        #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
        // Use Album map to determine 'AlbumAritst' tag for various artist albums, and
        // albums that have tracks where artist is set to '${artist} and somebodyelse'
        QMap<QString, MtpAlbum>::ConstIterator it=albums.constBegin();
        QMap<QString, MtpAlbum>::ConstIterator end=albums.constEnd();
        for (; it!=end; ++it) {
            if ((*it).artists.count()>1) {
                QSet<quint16> tracks;
                QString shortestArtist;
                bool duplicateTrackNumbers=false;
                foreach (MusicLibraryItemSong *s, (*it).songs) {
                    if (tracks.contains(s->track())) {
                        duplicateTrackNumbers=true;
                        break;
                    } else {
                        if (shortestArtist.isEmpty() || s->song().artist.length()<shortestArtist.length()) {
                            shortestArtist=s->song().artist.length();
                        }
                        tracks.insert(s->track());
                    }
                }

                // If an album has mutiple tracks with the same track number, then we probably have X albums
                // by X artists - in which case we proceeed no further.
                if (!duplicateTrackNumbers) {
                    // Now, check to see if all artists contain 'shortesArtist'. If so then use 'shortesArtist' as the album
                    // artist. This ir probably due to songs which have artist set to '${artist} and somebodyelse'
                    QString albumArtist=shortestArtist;
                    foreach (const QString &artist, (*it).artists) {
                        if (!artist.contains(shortestArtist)) {
                            // Found an artist that did not contain 'shortestArtist', so use 'Various Artists' for album artist
                            albumArtist=i18n("Various Artists");
                            break;
                        }
                    }

                    // Now move songs to correct artist/album...
                    foreach (MusicLibraryItemSong *s, (*it).songs) {
                        if (s->song().albumartist==albumArtist) {
                            continue;
                        }
                        Song song=s->song();
                        song.albumartist=albumArtist;
                        artistItem=library->artist(song);
                        albumItem=artistItem->album(song);
                        MusicLibraryItemSong *songItem = new MusicLibraryItemSong(song, albumItem);
                        albumItem->append(songItem);
                        albumItem->updateGenres();
                        artistItem->updateGenres();
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
        #endif

        if (MPDParseUtils::groupSingle()) {
            library->groupSingleTracks();
        }
        if (MPDParseUtils::groupMultiple()) {
            library->groupMultipleArtists();
        }
        emit libraryUpdated();
    }
}

void MtpConnection::setMusicFolder(Storage &store)
{
    if (0==store.musicFolderId) {
        store.musicFolderId=getFolder("Music/", store.id);
        if (0==store.musicFolderId) {
            store.musicFolderId=createFolder("Music", "Music/", 0, store.id);
        }
        if (0!=store.musicFolderId) {
            store.musicPath=getPath(store.musicFolderId);
        }
    }
}

void MtpConnection::updateFolders()
{
    folderMap.clear();
    LIBMTP_folder_t *folders=LIBMTP_Get_Folder_List(device);
    parseFolder(folders);
    if (folders) {
        LIBMTP_destroy_folder_t(folders);
    }
}

void MtpConnection::updateFiles()
{
    LIBMTP_file_t *files=LIBMTP_Get_Filelisting_With_Callback(device, 0, 0);
    LIBMTP_file_t *file=files;
    QMap<uint32_t, Folder>::iterator end=folderMap.end();
    while (file) {
        QMap<uint32_t, Folder>::iterator it=folderMap.find(file->parent_id);
        if (it!=end) {
            if (LIBMTP_FILETYPE_FOLDER!=file->filetype) {
                QString name=QString::fromUtf8(file->filename);
                if (name.endsWith(".jpg", Qt::CaseInsensitive) || name.endsWith(".png", Qt::CaseInsensitive)) {
                    (*it).covers.insert(file->item_id);
                }
            }
            (*it).children.insert(file->item_id);
        }
        file=file->next;
    }
    if (files) {
        LIBMTP_destroy_file_t(files);
    }
}

void MtpConnection::updateStorage()
{
    size=0;
    used=0;
    if (device && LIBMTP_ERROR_NONE==LIBMTP_Get_Storage(device, LIBMTP_STORAGE_SORTBY_MAXSPACE)) {
        LIBMTP_devicestorage_struct *s=device->storage;
        while (s) {
            QString volumeIdentifier=QString::fromUtf8(s->VolumeIdentifier);
            QList<Storage>::Iterator it=storage.begin();
            QList<Storage>::Iterator end=storage.end();
            for ( ;it!=end; ++it) {
                if ((*it).volumeIdentifier==volumeIdentifier) {
                    (*it).size=s->MaxCapacity;
                    (*it).used=s->MaxCapacity-s->FreeSpaceInBytes;
                    break;
                }
            }

            if (it==end) {
                Storage store;
                store.id=s->id;
                store.description=QString::fromUtf8(s->StorageDescription);
                store.volumeIdentifier=QString::fromUtf8(s->VolumeIdentifier);
                store.size=s->MaxCapacity;
                store.used=s->MaxCapacity-s->FreeSpaceInBytes;
                storage.append(store);
            }
            size+=s->MaxCapacity;
            used+=s->MaxCapacity-s->FreeSpaceInBytes;
            s=s->next;
        }
    }
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

    foreach (const QString &d, dirs) {
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

void MtpConnection::parseFolder(LIBMTP_folder_t *folder)
{
    if (!folder) {
        return;
    }

    QMap<uint32_t, Folder>::ConstIterator it=folderMap.find(folder->parent_id);
    QString path;
    if (it!=folderMap.constEnd()) {
        path=it.value().path+QString::fromUtf8(folder->name)+QChar('/');
    } else {
        path=QString::fromUtf8(folder->name)+QChar('/');
    }
    bool isMusic=path.startsWith(QLatin1String("Music/"), Qt::CaseInsensitive);
    if (isMusic) {
        folderMap.insert(folder->folder_id, Folder(path, folder->folder_id, folder->parent_id, folder->storage_id));
    }
    // Only recurse into music folder...
    if (folder->child && isMusic) {
        parseFolder(folder->child);
    }
    if (folder->sibling) {
        parseFolder(folder->sibling);
    }
}

static char * createString(const QString &str)
{
    return str.isEmpty() ? qstrdup("") : qstrdup(str.toUtf8());
}

static LIBMTP_filetype_t mtpFileType(const Song &s)
{
    #ifdef ENABLE_KDE_SUPPORT
    KMimeType::Ptr mime=KMimeType::findByPath(s.file);

    if (mime->is("audio/mpeg")) {
        return LIBMTP_FILETYPE_MP3;
    }
    if (mime->is("audio/ogg")) {
        return LIBMTP_FILETYPE_OGG;
    }
    if (mime->is("audio/x-ms-wma")) {
        return LIBMTP_FILETYPE_WMA;
    }
    if (mime->is("audio/mp4")) {
        return LIBMTP_FILETYPE_M4A; // LIBMTP_FILETYPE_MP4
    }
    if (mime->is("audio/aac")) {
        return LIBMTP_FILETYPE_AAC;
    }
    if (mime->is("audio/flac")) {
        return LIBMTP_FILETYPE_FLAC;
    }
    if (mime->is("audio/x-wav")) {
        return LIBMTP_FILETYPE_WAV;
    }
    #else
    if (s.file.endsWith(".mp3", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_MP3;
    }
    if (s.file.endsWith(".ogg", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_OGG;
    }
    if (s.file.endsWith(".wma", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_WMA;
    }
    if (s.file.endsWith(".m4a", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_M4A; // LIBMTP_FILETYPE_MP4
    }
    if (s.file.endsWith(".aac", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_AAC;
    }
    if (s.file.endsWith(".flac", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_FLAC;
    }
    if (s.file.endsWith(".wav", Qt::CaseInsensitive)) {
        return LIBMTP_FILETYPE_WAV;
    }
    #endif
    return LIBMTP_FILETYPE_UNDEF_AUDIO;
}

void MtpConnection::putSong(const Song &s, bool fixVa, const DeviceOptions &opts)
{
    bool added=false;
    bool fixedVa=false;
    bool embedCoverImage=Device::constEmbedCover==opts.coverName;
    LIBMTP_track_t *meta=0;
    QString destName;
    Storage store=getStorage(opts.volumeId);

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
        qulonglong spaceRequired=meta->filesize+8192;
        if (store.freeSpace()<spaceRequired) {
            QList<Storage>::Iterator it=storage.begin();
            QList<Storage>::Iterator end=storage.end();
            for ( ;it!=end; ++it) {
                if ((*it).freeSpace()>=spaceRequired) {
                    store=*it;
                    break;
                }
            }
        }

        meta->parent_id=store.musicFolderId;
        meta->storage_id=store.id;
        destName=store.musicPath+dev->opts.createFilename(song);
        QStringList dirs=destName.split('/', QString::SkipEmptyParts);
        if (dirs.count()>1) {
            destName=dirs.takeLast();
            meta->parent_id=checkFolderStructure(dirs, store);
        }

        meta->title=createString(song.title);
        meta->artist=createString(song.artist);
        meta->composer=createString(QString());
        meta->genre=createString(song.genre);
        meta->album=createString(song.album);
        meta->date=createString(QString().sprintf("%4d0101T0000.0", song.year));
        meta->filename=createString(destName);
        meta->tracknumber=song.track;
        meta->duration=song.time*1000;
        meta->rating=0;
        meta->usecount=0;
        meta->filetype=mtpFileType(song);
        meta->next=0;
        added=0==LIBMTP_Send_Track_From_File(device, fileName.toUtf8(), meta, &progressMonitor, this);
        if (temp) {
            // Delete the temp file...
            temp->remove();
            delete temp;
        }
    }

    if (added) {
        folderMap[meta->parent_id].children.insert(meta->item_id);
    }
    emit putSongStatus(added, meta ? meta->item_id : 0,
                       meta ? encodePath(meta, destName, storage.count()>1 ? store.description : QString()) : QString(),
                       fixedVa);
    if (meta) {
        LIBMTP_destroy_track_t(meta);
    }
}

static bool saveFile(const QString &name, char *data, uint64_t size)
{
    QFile f(name);

    if (f.open(QIODevice::WriteOnly)) {
        f.write(data, size);
        f.close();
        Utils::setFilePerms(name);
        return true;
    }
    return false;
}

void MtpConnection::getSong(const Song &song, const QString &dest, bool fixVa, bool copyCover)
{
    bool copiedSong=device && 0==LIBMTP_Get_File_To_File(device, song.id, dest.toUtf8(), &progressMonitor, this);
    bool copiedCover=false;
    /*
    if (copiedSong && !abortRequested() && copyCover) {
        QString destDir=Utils::getDir(dest);

        if (QDir(destDir).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable).isEmpty()) {
            LIBMTP_album_t *album=getAlbum(song);
            if (album) {
                LIBMTP_filesampledata_t *albumart = LIBMTP_new_filesampledata_t();
                QImage img;
                if (0==LIBMTP_Get_Representative_Sample(device, album->album_id, albumart)) {
                    img.loadFromData((unsigned char *)albumart->data, (int)albumart->size);
                    if (!img.isNull()) {
                        QString mpdCover=MPDConnection::self()->getDetails().coverName;
                        if (mpdCover.isEmpty()) {
                            mpdCover="cover";
                        }
                        if (LIBMTP_FILETYPE_JPEG==albumart->filetype) {
                            copiedCover=saveFile(QString(destDir+mpdCover+".jpg"), albumart->data, albumart->size);
                        } else if (LIBMTP_FILETYPE_PNG==albumart->filetype) {
                            copiedCover=saveFile(QString(destDir+mpdCover+".png"), albumart->data, albumart->size);
                        } else {
                            copiedCover=img.save(QString(destDir+mpdCover+".png"));
                            if (copiedCover) {
                                Utils::setFilePerms(destDir+mpdCover);
                            }
                        }
                    }
                }
                LIBMTP_destroy_filesampledata_t(albumart);
            }
        }
    }
    */
    if (copiedSong && fixVa && !abortRequested()) {
        Song s(song);
        Device::fixVariousArtists(dest, s, false);
    }
    emit getSongStatus(copiedSong, copiedCover);
}

void MtpConnection::delSong(const Song &song)
{
    Path path=decodePath(song.file);
    emit delSongStatus(device && path.ok() && 0==LIBMTP_Delete_Object(device, path.id));
}

bool MtpConnection::removeFolder(uint32_t storageId, uint32_t folderId)
{
    QMap<uint32_t, Folder>::iterator folder=folderMap.find(folderId);

    if (folderMap.end()!=folder) {
        if (!(*folder).children.isEmpty()) {
            // If we only have covers left, then remove them!
            if ((*folder).children.count()==(*folder).covers.count()) {
                QSet<uint32_t> covers=(*folder).covers;
                foreach (uint32_t cover, (*folder).covers) {
                    if (0==LIBMTP_Delete_Object(device, cover)) {
                        covers.remove(cover);
                        (*folder).children.remove(cover);
                    }
                }
                (*folder).covers=covers;
            }
        }

        if ((*folder).children.isEmpty() && 0==LIBMTP_Delete_Object(device, folderId)) {
            folderMap.remove(folderId);
            return true;
        }
    }
    return false;
}

void MtpConnection::cleanDirs(const QSet<QString> &dirs)
{
    foreach (const QString &d, dirs) {
        Path path=decodePath(d);
        Storage &store=getStorage(path.storage);
        uint32_t folderId=path.parent;
        while (0!=folderId && folderId!=store.musicFolderId) {
            QMap<uint32_t, Folder>::iterator it=folderMap.find(folderId);
            if (it!=folderMap.end()) {
                if (removeFolder(store.id, folderId)) {
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

    emit cleanDirsStatus(true);
}

void MtpConnection::getCover(const Song &song)
{
    /*
    LIBMTP_album_t *album=getAlbum(song);
    if (album) {
        QImage img=getCover(album);
        if (!img.isNull()) {
            albumsWithCovers.insert(album);
            emit cover(song, img);
        }
    }
    */
}

void MtpConnection::destroyData()
{
    folderMap.clear();
    if (library) {
        delete library;
        library=0;
    }
}

QString cfgKey(Solid::Device &dev, const QString &serial)
{
    QString key=QLatin1String("MTP-")+dev.vendor()+QChar('-')+dev.product()+QChar('-')+serial;
    key.replace('/', '_');
    return key;
}

MtpDevice::MtpDevice(DevicesModel *m, Solid::Device &dev)
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
    , busNum(0)
    , devNum(0)
{
    static bool registeredTypes=false;
    if (!registeredTypes) {
        qRegisterMetaType<QSet<QString> >("QSet<QString>");
        qRegisterMetaType<DeviceOptions >("DeviceOptions");
        registeredTypes=true;
    }

    Solid::GenericInterface *iface = dev.as<Solid::GenericInterface>();
    if (iface) {
        QMap<QString, QVariant> properties = iface->allProperties();
        busNum = properties.value(QLatin1String("BUSNUM")).toInt();
        devNum = properties.value(QLatin1String("DEVNUM")).toInt();
    }

    thread=new QThread(this);
    connection=new MtpConnection(this);
    connection->moveToThread(thread);
    thread->start();
    connect(this, SIGNAL(updateLibrary()), connection, SLOT(updateLibrary()));
    connect(connection, SIGNAL(libraryUpdated()), this, SLOT(libraryUpdated()));
    connect(connection, SIGNAL(progress(int)), this, SLOT(emitProgress(int)));
    connect(this, SIGNAL(putSong(const Song &, bool, const DeviceOptions &)), connection, SLOT(putSong(const Song &, bool, const DeviceOptions &)));
    connect(connection, SIGNAL(putSongStatus(bool, int, const QString &, bool)), this, SLOT(putSongStatus(bool, int, const QString &, bool)));
    connect(this, SIGNAL(getSong(const Song &, const QString &, bool, bool)), connection, SLOT(getSong(const Song &, const QString &, bool, bool)));
    connect(connection, SIGNAL(getSongStatus(bool, bool)), this, SLOT(getSongStatus(bool, bool)));
    connect(this, SIGNAL(delSong(const Song &)), connection, SLOT(delSong(const Song &)));
    connect(connection, SIGNAL(delSongStatus(bool)), this, SLOT(delSongStatus(bool)));
    connect(this, SIGNAL(cleanMusicDirs(const QSet<QString> &)), connection, SLOT(cleanDirs(const QSet<QString> &)));
    connect(this, SIGNAL(getCover(const Song &)), connection, SLOT(getCover(const Song &)));
    connect(connection, SIGNAL(cleanDirsStatus(bool)), this, SLOT(cleanDirsStatus(bool)));
    connect(connection, SIGNAL(statusMessage(const QString &)), this, SLOT(setStatusMessage(const QString &)));
    connect(connection, SIGNAL(deviceDetails(const QString &)), this, SLOT(deviceDetails(const QString &)));
    connect(connection, SIGNAL(songCount(int)), this, SLOT(songCount(int)));
    connect(connection, SIGNAL(cover(const Song &, const QImage &)), this, SIGNAL(cover(const Song &, const QImage &)));
    opts.fixVariousArtists=false;
    opts.coverName=Device::constEmbedCover;
    QTimer::singleShot(0, this, SLOT(rescan(bool)));
}

MtpDevice::~MtpDevice()
{
    stop();
}

void MtpDevice::deviceDetails(const QString &s)
{
    if ((s!=serial || serial.isEmpty()) && solidDev.isValid()) {
        serial=s;
        QString configKey=cfgKey(solidDev, serial);
        opts.load(configKey);
        #ifndef ENABLE_KDE_SUPPORT
        QSettings cfg;
        #endif
        configured=HAS_GROUP(configKey);
    }
}

bool MtpDevice::isConnected() const
{
    return solidDev.isValid() && pmp && pmp->isValid() && connection->isConnected();
}

void MtpDevice::stop()
{
    jobAbortRequested=true;
    Utils::stopThread(thread);
    thread=0;
    deleteTemp();
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
    dlg->show(QString(), opts, connection->getStorageList(), DevicePropertiesWidget::Prop_CoversBasic|DevicePropertiesWidget::Prop_Va|DevicePropertiesWidget::Prop_Transcoder);
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
    emit updateLibrary();
}

int MtpDevice::getSongId(const Song &s)
{
    // Tracks are associated with Albums, but to find an existing MTP album we need to know the ID
    // of one of its tracks. Tracks copied FROM the library, will have no valid ID - but if the album
    // is already on the device, we can use the song id of an existing track to locate it...
    foreach (MusicLibraryItem *artist, m_childItems) {
        if (artist->data()==s.artist || artist->data()==s.albumartist) {
            foreach (MusicLibraryItem *album, static_cast<MusicLibraryItemContainer *>(artist)->childItems()) {
                if (album->data()==s.album) {
                    MusicLibraryItemContainer *al=static_cast<MusicLibraryItemContainer *>(album);
                    return currentSong.id=static_cast<MusicLibraryItemSong *>(al->childItems().at(0))->song().id;
                }
            }
        }
    }
    return 0;
}

void MtpDevice::addSong(const Song &s, bool overwrite, bool copyCover)
{
    Q_UNUSED(copyCover)
    jobAbortRequested=false;
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
    currentSong.id=getSongId(currentSong);

    if (!opts.transcoderCodec.isEmpty()) {
        encoder=Encoders::getEncoder(opts.transcoderCodec);
        if (encoder.codec.isEmpty()) {
            emit actionStatus(CodecNotAvailable);
            return;
        }

        if (!opts.transcoderWhenDifferent || encoder.isDifferent(s.file)) {
            deleteTemp();
            tempFile=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX"+encoder.extension);
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
            connect(job, SIGNAL(result(int)), SLOT(transcodeSongResult(int)));
            connect(job, SIGNAL(percent(int)), SLOT(transcodePercent(int)));
            job->start();
            currentSong.file=destFile;
            return;
        }
    }
    transcoding=false;
    emit putSong(currentSong, needToFixVa, opts);
}

void MtpDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite, bool copyCover)
{
    jobAbortRequested=false;
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
        if (MusicLibraryModel::self()->songExists(check)) {
            emit actionStatus(SongExists);
            return;
        }
    }

    if (!songExists(s)) {
        emit actionStatus(SongDoesNotExist);
        return;
    }

    currentBaseDir=baseDir;
    currentMusicPath=musicPath;
    QString dest(currentBaseDir+currentMusicPath);
    QDir dir(Utils::getDir(dest));
    if (!dir.exists() && !Utils::createDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    emit getSong(s, currentBaseDir+currentMusicPath, needToFixVa, copyCover);
}

void MtpDevice::removeSong(const Song &s)
{
    jobAbortRequested=false;
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
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }
    emit cleanMusicDirs(dirs);
}

void MtpDevice::requestCover(const Song &song)
{
    jobAbortRequested=false;
    if (isConnected()) {
        emit getCover(song);
    }
}

void MtpDevice::putSongStatus(bool ok, int id, const QString &file, bool fixedVa)
{
    deleteTemp();
    if (jobAbortRequested) {
        return;
    }
    if (!ok) {
        emit actionStatus(Failed);
    } else {
        currentSong.id=id;
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
        emit actionStatus(Ok);
    }
}

void MtpDevice::transcodeSongResult(int status)
{
    FileJob::finished(sender());
    if (jobAbortRequested) {
        deleteTemp();
        return;
    }
    if (Ok!=status) {
        emit actionStatus(status);
    } else {
        emit putSong(currentSong, needToFixVa, DeviceOptions(Device::constNoCover));
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
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
        if (needToFixVa) {
            currentSong.revertVariousArtists();
        }
        Utils::setFilePerms(currentBaseDir+currentSong.file);
        MusicLibraryModel::self()->addSongToList(currentSong);
        DirViewModel::self()->addFileToList(currentSong.file);
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
        return i18n("Not Connected");
    }

    return i18n("%1 free").arg(Utils::formatByteSize(connection->capacity()-connection->usedSpace()));
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
    emit updating(udi(), false);
    mtpUpdating=false;
}

void MtpDevice::saveProperties(const QString &, const DeviceOptions &newOpts)
{
    if (configured && opts==newOpts) {
        return;
    }
    opts=newOpts;
    saveProperties();
}

void MtpDevice::saveProperties()
{
    if (solidDev.isValid()) {
        configured=true;
        opts.save(cfgKey(solidDev, serial));
    }
}

void MtpDevice::saveOptions()
{
    if (solidDev.isValid()) {
        opts.save(cfgKey(solidDev, serial));
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
