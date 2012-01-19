/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "covers.h"
#include "song.h"
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KUrl>
#include <KDE/KMimeType>
#include <QtCore/QDebug>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

MtpConnection::MtpConnection(MtpDevice *p)
    : device(0)
    , folders(0)
    , tracks(0)
    , library(0)
    , musicFolderId(0)
    , dev(p)
{
    size=0;
    used=0;
    LIBMTP_Init();
}

MtpConnection::~MtpConnection()
{
    disconnectFromDevice();
}

MusicLibraryItemRoot * MtpConnection::takeLibrary()
{
    MusicLibraryItemRoot *lib=library;
    library=0;
    return lib;
}

void MtpConnection::connectToDevice()
{
    device=0;
    musicFolderId=0;
    musicFolderStorageId=0;
    LIBMTP_raw_device_t *rawDevices;
    int numDev;

    emit statusMessage(i18n("Connecting to device..."));
    if (LIBMTP_ERROR_NONE!=LIBMTP_Detect_Raw_Devices(&rawDevices, &numDev) || 0==numDev) {
        qWarning() << "failed to detect raw dev";
        emit statusMessage(i18n("No devices found"));
        return;
    }

    LIBMTP_mtpdevice_t *dev=0;
    for (int i = 0; i < numDev; i++) {
        if ((dev = LIBMTP_Open_Raw_Device(&rawDevices[i]))) {
            break;
        }
    }

    size=0;
    used=0;
    if (dev && LIBMTP_ERROR_NONE==LIBMTP_Get_Storage(dev, LIBMTP_STORAGE_SORTBY_NOTSORTED) && dev->storage) {
        size=dev->storage->MaxCapacity;
        used=size-dev->storage->FreeSpaceInBytes;
    }
    device=dev;
    free(rawDevices);
    if (!device) {
        emit statusMessage(i18n("No devices found"));
        return;
    }

    musicFolderId=device->default_music_folder;
    emit statusMessage(i18n("Connected to device"));
}

void MtpConnection::disconnectFromDevice()
{
    if (device) {
        destroyData();
        LIBMTP_Release_Device(device);
        device=0;
        emit statusMessage(i18n("Disconnected from device"));
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
    emit statusMessage(i18n("Updating tracks..."));
    tracks=LIBMTP_Get_Tracklisting_With_Callback(device, 0, 0);
    LIBMTP_track_t *track=tracks;
    QMap<int, Folder>::ConstIterator folderEnd=folderMap.constEnd();
    QMap<int, Folder>::ConstIterator musicFolder=folderMap.find(musicFolderId);
    if (musicFolder!=folderEnd) {
        musicFolderStorageId=musicFolder.value().entry->storage_id;
        musicPath=musicFolder.value().path;
    }
    while (track) {
        QMap<int, Folder>::ConstIterator it=folderMap.find(track->parent_id);
        Song s;
        s.id=track->item_id;
        if (it!=folderEnd) {
            s.file=it.value().path+track->filename;
        } else {
            s.file=track->filename;
        }
        s.album=track->album;
        s.artist=track->artist;
        s.albumartist=s.artist; // libMTP does not support album artist :-(
        // Use LIBMTP_Get_File_To_File_Descriptor to pass to TagLib!!!
        s.year=QString::fromUtf8(track->date).mid(0, 4).toUInt();
        s.title=track->title;
        s.genre=track->genre;
        s.track=track->tracknumber;
        s.fillEmptyFields();
        trackMap.insert(track->item_id, track);
        MusicLibraryItemArtist *artistItem = library->artist(s);
        MusicLibraryItemAlbum *albumItem = artistItem->album(s);
        MusicLibraryItemSong *songItem = new MusicLibraryItemSong(s, albumItem);

        albumItem->append(songItem);
        albumItem->addGenre(s.genre);
        artistItem->addGenre(s.genre);
        songItem->addGenre(s.genre);
        library->addGenre(s.genre);
        track=track->next;
    }
    emit libraryUpdated();
}

uint32_t MtpConnection::getMusicFolderId()
{
    return musicFolderId ? musicFolderId
                         : folders
                            ? getFolderId("Music", folders)
                            : 0;
}

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
    folderMap.empty();
    if (folders) {
        LIBMTP_destroy_folder_t(folders);
        folders=0;
    }
    folders=LIBMTP_Get_Folder_List(device);
    parseFolder(folders);
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
    delete(nameCopy);
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

    QMap<int, Folder>::ConstIterator it=folderMap.find(folder->parent_id);
    if (it!=folderMap.constEnd()) {
        folderMap.insert(folder->folder_id, Folder(it.value().path+folder->name+QChar('/'), folder));
    } else {
        folderMap.insert(folder->folder_id, Folder(QString(folder->name)+QChar('/'), folder));
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
    return str.isEmpty() ? qstrdup("") : qstrdup( str.toUtf8());
}

static LIBMTP_filetype_t mtpFileType(const Song &s)
{
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
    return LIBMTP_FILETYPE_UNDEF_AUDIO;
}

void MtpConnection::putSong(const Song &song)
{
    bool added=false;
    LIBMTP_track_t *meta=0;
    if (device) {
        meta=LIBMTP_new_track_t();
        meta->parent_id=musicFolderId;
        meta->storage_id=musicFolderStorageId;

        QString destName=musicPath+dev->nameOpts.createFilename(song);
        QStringList dirs=destName.split('/');
        if (dirs.count()>2) {
            destName=dirs.takeLast();
            meta->parent_id=checkFolderStructure(dirs);
        }
        meta->item_id=0;
        meta->title=createString(song.title);
        meta->artist=createString(song.artist);
        meta->composer=createString(QString());
        meta->genre=createString(song.genre);
        meta->album=createString(song.album);
        meta->date=createString(QString::number(song.year));
        meta->filename=createString(destName);
        meta->tracknumber=song.track;
        meta->duration=song.size;
        meta->rating=0;
        meta->usecount=0;

        struct stat statBuf;
        if (0==stat(QFile::encodeName(song.file).constData(), &statBuf)) {
            meta->filesize=statBuf.st_size;
            meta->modificationdate=statBuf.st_mtime;
        }
        meta->filetype=mtpFileType(song);
        meta->next=0;
        added=0==LIBMTP_Send_Track_From_File(device, song.file.toUtf8(), meta, 0, 0);
    }
    if (added) {
        trackMap.insert(meta->item_id, meta);
    } else if (meta) {
        LIBMTP_destroy_track_t(meta);
    }
    emit putSongStatus(added, meta ? meta->item_id : 0, meta ? meta->filename : 0);
}

void MtpConnection::getSong(const Song &song, const QString &dest)
{
    emit getSongStatus(device && 0==LIBMTP_Get_File_To_File(device, song.id, dest.toUtf8(), 0, 0));
}

void MtpConnection::delSong(const Song &song)
{
    // TODO: After delete, need to check if all songs of this album are gone, if so delete the folder, etc.
    bool deleted=device && trackMap.contains(song.id) && 0==LIBMTP_Delete_Object(device, song.id);
    if (deleted) {
        LIBMTP_destroy_track_t(trackMap[song.id]);
        trackMap.remove(song.id);
    }
    emit delSongStatus(deleted);
}

void MtpConnection::destroyData()
{
    folderMap.empty();
    if (folders) {
        LIBMTP_destroy_folder_t(folders);
        folders=0;
    }
    trackMap.empty();
    if (tracks) {
        LIBMTP_destroy_track_t(tracks);
        tracks=0;
    }
    if (library) {
        delete library;
        library=0;
    }
}

MtpDevice::MtpDevice(DevicesModel *m, Solid::Device &dev)
    : Device(m, dev)
    , pmp(dev.as<Solid::PortableMediaPlayer>())
    , mtpUpdating(false)
{
    thread=new QThread(this);
    connection=new MtpConnection(this);
    connection->moveToThread(thread);
    thread->start();
    connect(this, SIGNAL(updateLibrary()), connection, SLOT(updateLibrary()));
    connect(connection, SIGNAL(libraryUpdated()), this, SLOT(libraryUpdated()));
    connect(this, SIGNAL(putSong(const Song &)), connection, SLOT(putSong(const Song &)));
    connect(connection, SIGNAL(putSongStatus(bool, int, const QString &)), this, SLOT(putSongStatus(bool, int, const QString &)));
    connect(this, SIGNAL(getSong(const Song &, const QString &)), connection, SLOT(getSong(const Song &, const QString &)));
    connect(connection, SIGNAL(getSongStatus(bool)), this, SLOT(getSongStatus(bool)));
    connect(this, SIGNAL(delSong(const Song &)), connection, SLOT(delSong(const Song &)));
    connect(connection, SIGNAL(delSongStatus(bool)), this, SLOT(delSongStatus(bool)));
    connect(connection, SIGNAL(statusMessage(const QString &)), this, SLOT(setStatusMessage(const QString &)));
    QTimer::singleShot(0, this, SLOT(rescan()));
}

struct Thread : public QThread
{
    static void sleep() { QThread::msleep(100); }
};

MtpDevice::~MtpDevice()
{
    thread->quit();
    for(int i=0; i<10 && thread->isRunning(); ++i)
        Thread::sleep();
    thread->deleteLater();
    thread=0;
}

bool MtpDevice::isConnected() const
{
    return pmp && connection->isConnected();
}

void MtpDevice::rescan()
{
    if (mtpUpdating) {
        return;
    }
    mtpUpdating=true;
    emit updating(solidDev.udi(), true);
    emit updateLibrary();
}

void MtpDevice::addSong(const Song &s, bool overwrite)
{
    qWarning() << "ADD SONG";
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    if (!overwrite && songExists(s)) {
        emit actionStatus(SongExists);
        return;
    }

    if (!QFile::exists(s.file)) {
        emit actionStatus(SourceFileDoesNotExist);
        return;
    }
    qWarning() << "EMIT PUT SONG";

    emit putSong(s);
}

void MtpDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite)
{
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    if (!overwrite && MusicLibraryModel::self()->songExists(s)) {
        emit actionStatus(SongExists);
        return;
    }

    if (!songExists(s)) {
        emit actionStatus(SongDoesNotExist);
        return;
    }

    currentBaseDir=baseDir;
    currentMusicPath=musicPath;
    KUrl dest(currentBaseDir+currentMusicPath);
    QDir dir(dest.directory());
    if (!dir.exists() && !dir.mkpath( "." )) {
        emit actionStatus(DirCreationFaild);
    }

    currentSong=s;
    emit getSong(s, musicPath+musicPath);
}

void MtpDevice::removeSong(const Song &s)
{
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

void MtpDevice::cleanDir(const QString &dir)
{
    Q_UNUSED(dir)
}

void MtpDevice::putSongStatus(bool ok, int id, const QString &file)
{
    if (!ok) {
        emit actionStatus(Failed);
    } else {
        currentSong.id=id;
        currentSong.file=file;
//         Covers::copyCover(currentSong, sourceDir, MPDParseUtils::getDir(currentSong.file), false);
        addSongToList(currentSong);
        emit actionStatus(Ok);
    }
}

void MtpDevice::getSongStatus(bool ok)
{
    if (!ok) {
        emit actionStatus(Failed);
    } else {
//         QString sourceDir=MPDParseUtils::getDir(currentSong.file);
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
// TODO: Get covers???
//         Covers::copyCover(currentSong, sourceDir, currentBaseDir+MPDParseUtils::getDir(currentMusicPath), true);
        MusicLibraryModel::self()->addSongToList(currentSong);
        emit actionStatus(Ok);
    }
}

void MtpDevice::delSongStatus(bool ok)
{
    if (!ok) {
        emit actionStatus(Failed);
    } else {
        removeSongFromList(currentSong);
        emit actionStatus(Ok);
    }
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

    return i18n("%1 Free", KGlobal::locale()->formatByteSize(connection->capacity()-connection->usedSpace()), 1);
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
    emit updating(solidDev.udi(), false);
    mtpUpdating=false;
}
