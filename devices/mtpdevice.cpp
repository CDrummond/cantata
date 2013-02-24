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

//static void logAlbum(LIBMTP_album_t *alb)
//{
//    QList<uint32_t> tracks;

//    for (uint32_t i=0; i<alb->no_tracks; ++i) {
//        tracks.append(alb->tracks[i]);
//    }

//    qWarning() << "Album" << QString::fromUtf8(alb->name) << alb->album_id << alb->no_tracks << "TRACKS:" << tracks;
//}

MtpConnection::MtpConnection(MtpDevice *p)
    : device(0)
    , folders(0)
    , albums(0)
    , tracks(0)
    , library(0)
    , musicFolderId(0)
    //, albumsFolderId(0)
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
    musicFolderId=0;
    //albumsFolderId=0;
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
    updateCapacity();
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

    musicFolderId=device->default_music_folder;
    #if 0
    albumsFolderId=device->default_album_folder;
    supportedTypes.clear();

    uint16_t *list = 0;
    uint16_t length = 0;

    if (0==LIBMTP_Get_Supported_Filetypes(device, &list, &length) && list && length) {
        for (uint16_t i=0 ; i<length ; ++i) {
            supportedTypes.insert(list[i]);
        }
        free(list);
    }
    #endif

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
    emit statusMessage(i18n("Updating tracks..."));
    lastUpdate=0;
    tracks=LIBMTP_Get_Tracklisting_With_Callback(device, &trackListMonitor, this);
    updateAlbums();
    LIBMTP_track_t *track=tracks;
    QMap<int, Folder>::ConstIterator folderEnd=folderMap.constEnd();
    if (0!=musicFolderId) {
        QMap<int, Folder>::ConstIterator musicFolder=folderMap.find(musicFolderId);
        if (musicFolder!=folderEnd) {
            musicPath=musicFolder.value().path;
        }
    }

    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    while (track && !abortRequested()) {
        QMap<int, Folder>::ConstIterator it=folderMap.find(track->parent_id);
        Song s;
        s.id=track->item_id;
        if (it!=folderEnd) {
            s.file=it.value().path+QString::fromUtf8(track->filename);
        } else {
            s.file=track->filename;
        }
        s.album=QString::fromUtf8(track->album);
        s.artist=QString::fromUtf8(track->artist);
        s.albumartist=s.artist; // TODO: ALBUMARTIST: Read from 'track' when libMTP supports album artist!
        s.year=QString::fromUtf8(track->date).mid(0, 4).toUInt();
        s.title=QString::fromUtf8(track->title);
        s.genre=QString::fromUtf8(track->genre);
        s.track=track->tracknumber;
        s.time=(track->duration/1000.0)+0.5;
        s.fillEmptyFields();
        trackMap.insert(track->item_id, track);

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
        track=track->next;
    }
    if (!abortRequested()) {
        if (MPDParseUtils::groupSingle()) {
            library->groupSingleTracks();
        }
        if (MPDParseUtils::groupMultiple()) {
            library->groupMultipleArtists();
        }
        emit libraryUpdated();
    }
}

uint32_t MtpConnection::getMusicFolderId()
{
    return musicFolderId ? musicFolderId
                         : folders
                            ? getFolderId("Music", folders)
                            : 0;
}

//uint32_t MtpConnection::getAlbumsFolderId()
//{
//    return albumsFolderId ? albumsFolderId
//                          : folders
//                            ? getFolderId("Albums", folders)
//                            : 0;
//}

uint32_t MtpConnection::getFolderId(const char *name, LIBMTP_folder_t *f)
{
    if (!f) {
        return 0;
    }

    if (!strcasecmp(name, f->name)) {
        return f->folder_id;
    }

    uint32_t i;

//     if ((i=getFolderId(name, f->child))) {
//         return i;
//     }
    if ((i=getFolderId(name, f->sibling))) {
        return i;
    }

    return 0;
}

void MtpConnection::updateFolders()
{
    folderMap.clear();
    if (folders) {
        LIBMTP_destroy_folder_t(folders);
        folders=0;
    }
    folders=LIBMTP_Get_Folder_List(device);
    parseFolder(folders);
}

void MtpConnection::updateAlbums()
{
    QSet<uint32_t> covers;
    foreach (LIBMTP_album_t *alb, albumsWithCovers) {
        covers.insert(alb->album_id);
    }

    albumsWithCovers.clear();
    if (albums) {
        LIBMTP_album_t *alb=albums;
        while (alb) {
            LIBMTP_album_t *a=alb;
            alb=alb->next;
            LIBMTP_destroy_album_t(a);
        }
    }
    albums=LIBMTP_Get_Album_List(device);
    LIBMTP_album_t *alb=albums;
    while (alb) {
//        logAlbum(alb);
        if (covers.contains(alb->album_id)) {
            albumsWithCovers.insert(alb);
            covers.remove(alb->album_id);
            if (covers.isEmpty()) {
                break;
            }
        }
        alb=alb->next;
    }
}

void MtpConnection::updateCapacity()
{
    if (device && LIBMTP_ERROR_NONE==LIBMTP_Get_Storage(device, LIBMTP_STORAGE_SORTBY_NOTSORTED) && device->storage) {
        size=device->storage->MaxCapacity;
        used=size-device->storage->FreeSpaceInBytes;
    }
}

uint32_t MtpConnection::createFolder(const char *name, uint32_t parentId)
{
    QMap<int, Folder>::ConstIterator it=folderMap.find(parentId);
    uint32_t storageId=0;
    if (it!=folderMap.constEnd()) {
        storageId=it.value().entry->storage_id;
    }
    char *nameCopy = qstrdup(name);
    uint32_t newFolderId=LIBMTP_Create_Folder(device, nameCopy, parentId, storageId);
    delete nameCopy;
    if (0==newFolderId)  {
        return 0;
    }
    updateFolders();
    return newFolderId;
}

uint32_t MtpConnection::getFolder(const QString &path)
{
    QMap<int, Folder>::ConstIterator it=folderMap.constBegin();
    QMap<int, Folder>::ConstIterator end=folderMap.constEnd();

    for (; it!=end; ++it) {
        if (it.value().path==path) {
            return it.value().entry->folder_id;
        }
    }
    return 0;
}

MtpConnection::Folder * MtpConnection::getFolderEntry(const QString &path)
{
    QMap<int, Folder>::ConstIterator it=folderMap.constBegin();
    QMap<int, Folder>::ConstIterator end=folderMap.constEnd();

    for (; it!=end; ++it) {
        if (it.value().path==path) {
            return const_cast<MtpConnection::Folder *>(&(it.value()));
        }
    }
    return 0;
}

uint32_t MtpConnection::checkFolderStructure(const QStringList &dirs)
{
    uint32_t parentId=getMusicFolderId();
    QString path;
    foreach (const QString &d, dirs) {
        path+=d+QChar('/');
        uint32_t folderId=getFolder(path);
        if (0==folderId) {
            folderId=createFolder(d.toUtf8().constData(), parentId);
            if (0==folderId) {
                return parentId;
            } else {
                updateFolders();
                parentId=getFolder(path);
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

    QMap<int, Folder>::ConstIterator it=folderMap.find(folder->parent_id);
    if (it!=folderMap.constEnd()) {
        folderMap.insert(folder->folder_id, Folder(it.value().path+QString::fromUtf8(folder->name)+QChar('/'), folder));
    } else {
        folderMap.insert(folder->folder_id, Folder(QString::fromUtf8(folder->name)+QChar('/'), folder));
    }
    if (folder->child) {
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
    if (device) {
        meta=LIBMTP_new_track_t();
        meta->parent_id=musicFolderId;
        meta->storage_id=0;

        Song song=s;
        QString destName=musicPath+dev->opts.createFilename(song);
        QStringList dirs=destName.split('/', QString::SkipEmptyParts);
        if (dirs.count()>2) {
            destName=dirs.takeLast();
            meta->parent_id=checkFolderStructure(dirs);
        }
        meta->item_id=0;
        QString fileName=song.file;
        QTemporaryFile *temp=0;

        if (fixVa || embedCoverImage) {
            // Need to 'workaround' broken various artists handling, so write to a temporary file first...
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

        struct stat statBuf;
        if (0==stat(QFile::encodeName(fileName).constData(), &statBuf)) {
            meta->filesize=statBuf.st_size;
            meta->modificationdate=statBuf.st_mtime;
        }
        meta->filetype=mtpFileType(song);
        meta->next=0;
        added=0==LIBMTP_Send_Track_From_File(device, fileName.toUtf8(), meta, &progressMonitor, this);
        if (temp) {
            // Delete the temp file...
            temp->remove();
            delete temp;
        }

        // TODO: adding of covers does not seem to work 100% reliably, and slows the copy process down.
        // If we're not adding covers, not much point in creating albums...
        #if 0
        if (added) {
            // Add song to album...
            qWarning() << "Get album" << song.id;
            LIBMTP_album_t *album=getAlbum(song);
            if (!album) {
                qWarning() << "TRACK ADDED, ALBUM NOT FOUND, SO GET ALBUM LIST";
                updateAlbums();
                qWarning() << "UPDATED ALBUMS";
                song.id=meta->item_id;
                album=getAlbum(song);
            }
            if (album) {
                qWarning() << "Album exists" << album->no_tracks << album->album_id;
                uint32_t *tracks = (uint32_t *)malloc((album->no_tracks+1)*sizeof(uint32_t));
                if (tracks) {
                    album->no_tracks++;
                    if (album->tracks) {
                        memcpy(tracks, album->tracks, (album->no_tracks-1)*sizeof(uint32_t));
                        free(album->tracks);
                    }
                    tracks[album->no_tracks-1]=meta->item_id;
                    album->tracks = tracks;
                    qWarning() << "Update album";
                    LIBMTP_Update_Album(device, album);
                    logAlbum(album);
                    qWarning() << "Album updated";
                }
            } else {
                album=LIBMTP_new_album_t();
                album->tracks=(uint32_t *)malloc(sizeof(uint32_t));
                album->tracks[0]=meta->item_id;
                album->no_tracks=1;
                album->album_id=0;
                album->parent_id=getAlbumsFolderId();
                album->storage_id=0;
                album->name=createString(song.album);
                album->artist=createString(song.albumArtist());
                album->composer=0;
                album->genre=createString(song.genre);
                album->next=0;
                qWarning() << "Create new album";
                logAlbum(album);
                if (0==LIBMTP_Create_New_Album(device, album)) {
                    qWarning() << "Album created" << album->album_id;
                    LIBMTP_album_t *al=albums;
                    if (!albums) {
                        albums=album;
                    } else {
                        while (al) {
                            if (!al->next) {
                                al->next=album;
                                break;
                            }
                            al=al->next;
                        }
                    }
                } else {
                    qWarning() << "Failed to create album";
                    LIBMTP_destroy_album_t(album);
                    album=0;
                }
            }

            if (album) {
                Covers::Image image=Covers::self()->getImage(s);
                if (!image.img.isNull()) {
                    // First check if album already has cover...
                    if (!albumsWithCovers.contains(album) && getCover(album).isNull()) {
                        QTemporaryFile *temp=0;
                        // Cantata covers are either JPG or PNG. Assume device supports JPG...
                        if (image.fileName.isEmpty() || (image.fileName.endsWith(".png ") && !supportedTypes.contains(LIBMTP_FILETYPE_PNG))) {
                            temp=new QTemporaryFile(QDir::tempPath()+"/cantata_XXXXXX.jpg");
                            temp->setAutoRemove(true);
                            if (!image.img.save(temp, "JPG")) {
                                image.fileName=QString();
                            } else {
                                image.fileName=temp->fileName();
                            }
                        }
                        struct stat statbuff;

                        if (-1!=stat(QFile::encodeName(image.fileName).constData(), &statbuff)) {
                            uint64_t filesize = (uint64_t) statbuff.st_size;
                            if (filesize>0) {
                                char *imagedata = (char *)malloc(filesize);
                                if (imagedata) {
                                    FILE *fd = fopen(QFile::encodeName(image.fileName).constData(), "r");
                                    if (fd) {
                                        fread(imagedata, filesize, 1, fd);
                                        fclose(fd);

                                        LIBMTP_filesampledata_t *albumart=LIBMTP_new_filesampledata_t();
                                        albumart->data = imagedata;
                                        albumart->size = filesize;
                                        albumart->filetype = image.fileName.endsWith(".png") ? LIBMTP_FILETYPE_PNG : LIBMTP_FILETYPE_JPEG;
                                        qWarning() << "Send cover";
                                        int rv=0;
                                        if (0==(rv=LIBMTP_Send_Representative_Sample(device, album->album_id, albumart))) {
                                            albumsWithCovers.insert(album);
                                        }
                                        albumart->data=0;
                                        LIBMTP_destroy_filesampledata_t(albumart);
                                    }
                                    free(imagedata);
                                }
                            }
                        }
                        if (temp) {
                            delete temp;
                        }
                    }
                }
            }
        }
        #endif
    }
    if (added) {
        trackMap.insert(meta->item_id, meta);
        updateCapacity();
    } else if (meta) {
        LIBMTP_destroy_track_t(meta);
    }
    emit putSongStatus(added, meta ? meta->item_id : 0, meta ? meta->filename : 0, fixedVa);
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
    if (copiedSong && fixVa && !abortRequested()) {
        Song s(song);
        Device::fixVariousArtists(dest, s, false);
    }
    emit getSongStatus(copiedSong, copiedCover);
}

void MtpConnection::delSong(const Song &song)
{
    bool deleted=device && trackMap.contains(song.id) && 0==LIBMTP_Delete_Object(device, song.id);
    if (deleted) {
        LIBMTP_destroy_track_t(trackMap[song.id]);
        trackMap.remove(song.id);
        // Remove track from album. Remove album (and cover) if no tracks.
        LIBMTP_album_t *album=getAlbum(song);
        if (album) {
            if (0==album->no_tracks || (1==album->no_tracks && album->tracks[0]==(uint32_t)song.id)) {
                if (!getCover(album).isNull()) {
                    LIBMTP_filesampledata_t *albumart = LIBMTP_new_filesampledata_t();
                    albumart->data = NULL;
                    albumart->size = 0;
                    albumart->filetype = LIBMTP_FILETYPE_UNKNOWN;
                    LIBMTP_Send_Representative_Sample(device, album->album_id, albumart);
                    LIBMTP_destroy_filesampledata_t(albumart);
                }
                albumsWithCovers.remove(album);
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
//                        logAlbum(album);
                    } else {
                        free(tracks);
                    }
                }
            }
        }
        updateCapacity();
    }
    emit delSongStatus(deleted);
}

void MtpConnection::cleanDirs(const QSet<QString> &dirs)
{
    foreach (const QString &path, dirs) {
        QStringList parts=path.split("/", QString::SkipEmptyParts);
        while (parts.length()>1) {
            // Find ID of this folder...
            uint32_t id=getFolder(parts.join("/")+QChar('/'));
            if (0==id || musicFolderId==id) {
                break;
            }

            QMap<int, Folder>::Iterator folder=folderMap.find(id);
            if (folder!=folderMap.end()) {
                if (0==LIBMTP_Delete_Object(device, id)) {
                    folderMap.erase(folder);
                } else {
                    break;
                }
                parts.takeLast();
            } else {
                break;
            }
        }
    }
    emit cleanDirsStatus(true);
}

QImage MtpConnection::getCover(LIBMTP_album_t *album)
{
    LIBMTP_filesampledata_t *albumart = LIBMTP_new_filesampledata_t();
    QImage img;
    if (0==LIBMTP_Get_Representative_Sample(device, album->album_id, albumart)) {
        img.loadFromData((unsigned char *)albumart->data, (int)albumart->size);
    }
    LIBMTP_destroy_filesampledata_t(albumart);
    return img;
}

void MtpConnection::getCover(const Song &song)
{
    LIBMTP_album_t *album=getAlbum(song);
    if (album) {
        QImage img=getCover(album);
        if (!img.isNull()) {
            albumsWithCovers.insert(album);
            emit cover(song, img);
        }
    }
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

void MtpConnection::destroyData()
{
    folderMap.clear();
    if (folders) {
        LIBMTP_destroy_folder_t(folders);
        folders=0;
    }

    albumsWithCovers.clear();
    if (albums) {
        LIBMTP_album_t *alb=albums;
        while (alb) {
            LIBMTP_album_t *a=alb;
            alb=alb->next;
            LIBMTP_destroy_album_t(a);
        }
        albums=0;
    }

    trackMap.clear();
    if (tracks) {
        LIBMTP_destroy_track_t(tracks);
        tracks=0;
    }

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
    : Device(m, dev, false)
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
    dlg->show(QString(), opts, DevicePropertiesWidget::Prop_CoversBasic|DevicePropertiesWidget::Prop_Va|DevicePropertiesWidget::Prop_Transcoder);
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
        } else if (!opts.fixVariousArtists) { // TODO: ALBUMARTIST: Remove when libMTP supports album artist!
            currentSong.albumartist=currentSong.artist;
        }
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
