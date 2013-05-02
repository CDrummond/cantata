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

#define MTP_FAKE_ALBUMARTIST_SUPPORT
#define MTP_TRACKNUMBER_FROM_FILENAME

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

static uint16_t fileReceiver(void *params, void *priv, uint32_t sendlen, unsigned char *data, uint32_t *putlen)
{
    Q_UNUSED(params)
    QByteArray *byteData=(QByteArray *)priv;
    (*byteData)+=QByteArray((char *)data, (int)sendlen);
    *putlen = sendlen;
    return LIBMTP_HANDLER_RETURN_OK;
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
    thread=new QThread();
    moveToThread(thread);
    thread->start();
}

MtpConnection::~MtpConnection()
{
    disconnectFromDevice(false);
    Utils::stopThread(thread);
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
    if ((t-lastUpdate)>=2 || 0==(count%10)) {
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
    QMap<uint32_t, MtpFolder> folders;
    bool getAlbumArtistFromPath=dev->options().scheme.startsWith(DeviceOptions::constAlbumArtist+QChar('/')+DeviceOptions::constAlbumTitle+QChar('/'));
    QString va=i18n("Various Artists");
    #endif

    while (track && !abortRequested()) {
        QMap<uint32_t, Folder>::ConstIterator it=folderMap.find(track->parent_id);
        if (it==folderEnd) {
            // We only care about tracks in the music folder...
            track=track->next;
            continue;
        }
        Song s;
        QString trackFilename=QString::fromUtf8(track->filename);
        s.id=track->item_id;
        s.file=encodePath(track, it.value().path+trackFilename, storageNames.count()>1 ? storageNames[track->storage_id] : QString());
        s.album=QString::fromUtf8(track->album);
        s.artist=QString::fromUtf8(track->artist);
        s.albumartist=s.artist; // TODO: ALBUMARTIST: Read from 'track' when libMTP supports album artist!
        s.year=QString::fromUtf8(track->date).mid(0, 4).toUInt();
        s.title=QString::fromUtf8(track->title);
        s.genre=QString::fromUtf8(track->genre);
        s.track=track->tracknumber;
        s.time=(track->duration/1000.0)+0.5;
        s.fillEmptyFields();
        #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
        if (getAlbumArtistFromPath) {
            QStringList folderParts=(*it).path.split('/', QString::SkipEmptyParts);
            if (folderParts.length()>=3) {
                MtpFolder folder(folderParts.at(1), folderParts.at(2));
                folders.insert(track->parent_id, folder);
                if (folder.album==s.album && (folder.artist==QLatin1String("Various Artists") || folder.artist==va)) {
                    s.albumartist=folder.artist;
                }
            }
        }
        #endif
        #ifdef MTP_TRACKNUMBER_FROM_FILENAME
        if (0==s.track) {
            int space=trackFilename.indexOf(' ');
            if (space>0) {
                s.track=trackFilename.mid(0, space).toInt();
            }
        }
        #endif
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
        al.folder=track->parent_id;
        #endif
        track=track->next;
    }
    if (tracks) {
        LIBMTP_destroy_track_t(tracks);
    }
    if (!abortRequested()) {
        #ifdef MTP_FAKE_ALBUMARTIST_SUPPORT
        // Use Album map to determine 'AlbumArtist' tag for various artist albums, and
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
                    MtpFolder &f=folders[(*it).folder];
                    // Now, check to see if all artists contain 'shortesArtist'. If so then use 'shortesArtist' as the album
                    // artist. This ir probably due to songs which have artist set to '${artist} and somebodyelse'
                    QString albumArtist=shortestArtist;
                    foreach (const QString &artist, (*it).artists) {
                        if (!artist.contains(shortestArtist)) {
                            // Found an artist that did not contain 'shortestArtist', so use 'Various Artists' for album artist
                            albumArtist=!f.artist.isEmpty() && f.album==it.key() ? f.artist : i18n("Various Artists");
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
    QSet<uint32_t> folders=folderMap.keys().toSet();
    while (file) {
        if (folders.contains(file->parent_id)) {
            Folder &folder=folderMap[file->parent_id];
            if (LIBMTP_FILETYPE_FOLDER==file->filetype) {
                folder.folders.insert(file->item_id);
            } else {
                QString name=QString::fromUtf8(file->filename);
                if (name.endsWith(".jpg", Qt::CaseInsensitive) || name.endsWith(".png", Qt::CaseInsensitive) || QLatin1String("albumart.pamp")==name) {
                    folder.covers.insert(file->item_id, File(name, file->filesize, file->item_id));
                } else {
                    folder.files.insert(file->item_id, File(name, file->filesize, file->item_id));
                }
            }
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
        if (folderMap.contains(folder->parent_id)) {
            folderMap[folder->parent_id].folders.insert(folder->folder_id);
        }
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

static LIBMTP_filetype_t mtpFileType(const QString &f)
{
    #ifdef ENABLE_KDE_SUPPORT
    KMimeType::Ptr mime=KMimeType::findByPath(f);

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
    if (mime->is("image/jpeg")) {
        return LIBMTP_FILETYPE_JPEG;
    }
    if (mime->is("image/png")) {
        return LIBMTP_FILETYPE_PNG;
    }
    #else
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
    #endif
    return LIBMTP_FILETYPE_UNDEF_AUDIO;
}

static QTemporaryFile * saveImageToTemp(const QImage &img, const QString &name)
{
    QTemporaryFile *temp=new QTemporaryFile();

    int index=name.lastIndexOf('.');
    if (index>0) {
        temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX"+name.mid(index));
    } else {
        temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX");
    }
    img.save(temp);
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
        destName=store.musicPath+dev->opts.createFilename(song);
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
            meta->composer=createString(QString());
            meta->genre=createString(song.genre);
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
            Covers::Image coverImage=Covers::self()->getImage(s);
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

    if (copiedSong && !abortRequested() && copyCover) {
        QString destDir=Utils::getDir(dest);

        if (QDir(destDir).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable).isEmpty()) {
            File cover=getCoverDetils(song);

            if (0!=cover.id) {
                QString mpdCover=MPDConnection::self()->getDetails().coverName;
                if (mpdCover.isEmpty()) {
                    mpdCover="cover";
                }
                QString fileName=QString(destDir+mpdCover+(cover.name.endsWith(".jpg", Qt::CaseInsensitive) ? ".jpg" : ".png"));
                QByteArray fileNameUtf8=fileName.toUtf8();
                copiedCover=0==LIBMTP_Get_File_To_File(device, cover.id, fileNameUtf8.constData(), 0, 0);
                if (copiedCover) {
                    Utils::setFilePerms(fileName);
                }
            }
        }
    }

    if (copiedSong && fixVa && !abortRequested()) {
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
    }
    emit delSongStatus(deletedSong);
}

bool MtpConnection::removeFolder(uint32_t folderId)
{
    QMap<uint32_t, Folder>::iterator folder=folderMap.find(folderId);
    if (folderMap.end()!=folder && (*folder).folders.isEmpty() && (*folder).files.isEmpty()) {
        // Delete any cover files...
        QList<uint32_t> toRemove=(*folder).covers.keys();
        foreach (uint32_t cover, toRemove) {
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
    foreach (const QString &d, dirs) {
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

    emit cleanDirsStatus(true);
}

void MtpConnection::getCover(const Song &song)
{
    File c=getCoverDetils(song);

    if (0!=c.id) {
        QByteArray data;
        if (0==LIBMTP_Get_File_To_Handler(device, c.id, fileReceiver, &data, 0, 0)) {
            QImage img;
            if (img.loadFromData(data)) {
                emit cover(song, img);
            }
        }
    }
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

    connection=new MtpConnection(this);
    connect(this, SIGNAL(updateLibrary()), connection, SLOT(updateLibrary()));
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
    deleteTemp();
    connection->deleteLater();
    connection=0;
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
    dlg->show(QString(), opts, connection->getStorageList(), DevicePropertiesWidget::Prop_CoversAll|DevicePropertiesWidget::Prop_Va|DevicePropertiesWidget::Prop_Transcoder);
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

void MtpDevice::addSong(const Song &s, bool overwrite, bool copyCover)
{
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
    if (!opts.transcoderCodec.isEmpty()) {
        Encoders::Encoder encoder=Encoders::getEncoder(opts.transcoderCodec);
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
            job->setProperty("overwrite", overwrite);
            job->setProperty("copyCover", copyCover);
            connect(job, SIGNAL(result(int)), SLOT(transcodeSongResult(int)));
            connect(job, SIGNAL(percent(int)), SLOT(transcodePercent(int)));
            job->start();
            currentSong.file=destFile;
            return;
        }
    }
    transcoding=false;
    emit putSong(currentSong, needToFixVa, opts, overwrite, copyCover);
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

    currentMpdDir=baseDir;
    currentDestFile=baseDir+musicPath;
    QDir dir(Utils::getDir(currentDestFile));
    if (!dir.exists() && !Utils::createDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    emit getSong(s, currentDestFile, needToFixVa, copyCover);
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
        currentSong.file=currentDestFile.mid(currentMpdDir.length());
        if (needToFixVa) {
            currentSong.revertVariousArtists();
        }
        Utils::setFilePerms(currentDestFile);
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
        emit configurationChanged();
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
