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

#include "remotekiodevice.h"
#include "mpdparseutils.h"
#include "tags.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "dirviewmodel.h"
#include "encoders.h"
#include "transcodingjob.h"
#include "covers.h"
#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "localize.h"
#include <KDE/KTemporaryFile>
#include <KDE/KConfigGroup>
#include <KDE/KIO/ListJob>
#include <KDE/KIO/FileJob>
#include <KDE/KIO/NetAccess>
#include <QtGui/QApplication>
#include <QtXml/QXmlStreamReader>
#include <QtXml/QXmlStreamWriter>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QDebug>

static const QLatin1String constCantataCacheFile("/.cache.xml");

static const int constSize=5*1024;
static const QStringList constExtensions=QStringList() << QLatin1String(".mp3")
                                                       << QLatin1String(".ogg")
                                                       << QLatin1String(".flac")
                                                       << QLatin1String(".mp4")
                                                       << QLatin1String(".m4a");

KioScanner::KioScanner(QObject *parent)
    : QObject(parent)
    , terminated(false)
    , count(0)
    , library(0)
    , artistItem(0)
    , albumItem(0)
{
}

KioScanner::~KioScanner()
{
    QMap<QString, KTemporaryFile *>::ConstIterator it(tempFiles.constBegin());
    QMap<QString, KTemporaryFile *>::ConstIterator end(tempFiles.constEnd());
    for (; it!=end; ++it) {
        delete it.value();
    }
    stop(false);
}

void KioScanner::scan(const KUrl &url, const QSet<Song> &existingSongs)
{
    timer.start();
    stop();
    count=0;
    top=url;
    existing=existingSongs;
    terminated=false;
    listDir(top);
}

void KioScanner::stop(bool killJobs)
{
    foreach (KJob *job, activeJobs) {
        disconnect(job, SIGNAL(data(KIO::Job *, const QByteArray &)), this, SLOT(fileData(KIO::Job *, const QByteArray &)));
        disconnect(job, SIGNAL(result(KJob *)), this, SLOT(fileResult(KJob *)));
        disconnect(job, SIGNAL(open(KIO::Job *)), this, SLOT(fileOpen(KIO::Job *)));
        if (killJobs) {
            job->kill();
        }
    }
    jobData.clear();
    dirData.clear();
    activeJobs.clear();
    delete library;
    library=0;
    artistItem=0;
    albumItem=0;
    terminated=true;
}

MusicLibraryItemRoot * KioScanner::takeLibrary()
{
    MusicLibraryItemRoot *lib=library;
    library=0;
    return lib;
}

void KioScanner::listDir(const KUrl &u)
{
    if (terminated) {
        return;
    }
    qWarning() << "listDir" << u.url();
    int topParts=top.path(KUrl::RemoveTrailingSlash).split('/', QString::SkipEmptyParts).count();
    int uParts=u.path(KUrl::RemoveTrailingSlash).split('/', QString::SkipEmptyParts).count();

    if ((uParts-topParts)<3) {
        KIO::ListJob *job=KIO::listDir(u, KIO::HideProgressInfo, false);
        dirData[job]=u;
        connect(job, SIGNAL(entries(KIO::Job *, const KIO::UDSEntryList&)), this, SLOT(dirEntries(KIO::Job *, const KIO::UDSEntryList&)));
        connect(job, SIGNAL(result(KJob *)), this, SLOT(dirResult(KJob *)));
        activeJobs.insert(job);
    }
}

void KioScanner::dirEntries(KIO::Job *job, const KIO::UDSEntryList &list)
{
    if (terminated || !activeJobs.contains(job)) {
        return;
    }
    qWarning() << dirData[job].url() << "numEntries" << list.count();

    foreach (const KIO::UDSEntry &e, list) {
        QString name(e.stringValue(KIO::UDSEntry::UDS_NAME));
        if (QLatin1String(".")==name || QLatin1String("..")==name) {
            continue;
        }
        KUrl u(dirData[job]);
        u.addPath(name);
        if (e.isDir()) {
            dirs.append(u);
        } else {
            files.append(u);
        }
    }
    processNextEntry();
}

void KioScanner::dirResult(KJob *job)
{
    if (terminated) {
        return;
    }
    if (job->error()) {
        emit status(job->errorString());
        stop();
    } else {
        activeJobs.remove(job);
        processNextEntry();
    }
}

void KioScanner::fileData(KIO::Job *job, const QByteArray &data)
{
    if (terminated || !activeJobs.contains(job)) {
        return;
    }
    jobData[job].data+=data;
    qWarning() << "Received" << data.count() << jobData[job].data.count() << jobData[job].url.url();
    if (jobData[job].data.size()>=constSize || ((qobject_cast<KIO::FileJob *>(job)) && jobData[job].data.size()>=(int)((KIO::FileJob *)job)->size())) {
        QString extension=jobData[job].url.fileName();
        int pos=extension.lastIndexOf('.');
        if (pos>0) {
            extension=extension.mid(pos+1);
            if (!tempFiles.contains(extension)) {
                tempFiles[extension]=new KTemporaryFile;
                tempFiles[extension]->setSuffix("."+extension);
                tempFiles[extension]->open();
                tempFiles[extension]->setAutoRemove(true);
            }

            QFile f(tempFiles[extension]->fileName());
            if (f.open(QIODevice::WriteOnly)) {
                f.write(jobData[job].data);
                f.close();
                Song s=Tags::read(tempFiles[extension]->fileName());
                if (!s.isEmpty()) {
                    s.file=jobData[job].url.path().mid(top.path().length()+1);
                    addSong(s);
                }
            }
        }
        processNextEntry();
        job->kill(KJob::EmitResult);
        jobData.remove(job);
    }
}

void KioScanner::fileResult(KJob *job)
{
    if (terminated) {
        return;
    }
    activeJobs.remove(job);
    // Perhaps slave does not support open...
    if (KIO::ERR_UNSUPPORTED_ACTION==job->error() && 0==jobData[job].data.size() && qobject_cast<KIO::FileJob *>(job)) {
        KIO::TransferJob *j=KIO::get(jobData[job].url, KIO::NoReload, KIO::HideProgressInfo);
        connect(j, SIGNAL(data(KIO::Job *, const QByteArray &)), this, SLOT(fileData(KIO::Job *, const QByteArray &)));
        connect(j, SIGNAL(result(KJob *)), this, SLOT(fileResult(KJob *)));
        jobData[j].url=jobData[job].url;
        jobData.remove(job);
        activeJobs.insert(j);
        processNextEntry();
    } else if (job->error() && KJob::KilledJobError!=job->error()) {
        emit status(job->errorString());
        stop();
    } else {
        jobData.remove(job);
        processNextEntry();
    }
}

void KioScanner::fileOpen(KIO::Job *job)
{
    if (terminated || !activeJobs.contains(job)) {
        return;
    }
    if (0==job->error()) {
        connect(job, SIGNAL(data(KIO::Job *, const QByteArray &)), this, SLOT(fileData(KIO::Job *, const QByteArray &)));
        ((KIO::FileJob *)job)->read(constSize);
    }
}

void KioScanner::processNextEntry()
{
    if (terminated) {
        return;
    }
    for (; activeJobs.count()<12 && files.count();) {
        KUrl f=files.takeAt(0);

        QString fName=f.fileName();

        foreach (const QString &ext, constExtensions) {
            if (fName.endsWith(ext, Qt::CaseInsensitive)) {
                Song song;
                song.file=f.path().mid(top.path().length()+1);
                QSet<Song>::iterator it=existing.find(song);
                if (existing.end()!=it) {
                    addSong(*it);
                    existing.erase(it);
                    continue;
                }
                KIO::FileJob *job=KIO::open(f, QIODevice::ReadOnly);
                jobData[job].url=f;
                connect(job, SIGNAL(open(KIO::Job *)), this, SLOT(fileOpen(KIO::Job *)));
                connect(job, SIGNAL(result(KJob *)), this, SLOT(fileResult(KJob *)));
                job->start();
                activeJobs.insert(job);
                break;
            }
        }
    }

    for (; activeJobs.count()<12 && dirs.count(); ) {
        listDir(dirs.takeAt(0));
    }

    if (activeJobs.isEmpty()) {
        if (MPDParseUtils::groupSingle()) {
            library->groupSingleTracks();
        }
        if (MPDParseUtils::groupMultiple()) {
            library->groupMultipleArtists();
        }
        qWarning() << "SCAN TOOK" << timer.elapsed();
        emit status(QString());
    }
}

void KioScanner::addSong(const Song &s)
{
    if (!library) {
        library = new MusicLibraryItemRoot;
    }
    if (!artistItem || s.albumArtist()!=artistItem->data()) {
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

    count++;
//     if (0!=count) {
        emit songCount(count);
//     }
}

RemoteKioDevice::RemoteKioDevice(DevicesModel *m, const QString &cover, const DeviceOptions &options, const RemoteFsDevice::Details &d)
    : Device(m, d.name)
    , scanned(false)
    , scanner(0)
    , details(d)
    , tempFile(0)
{
    coverFileName=cover;
    opts=options;
    details.url.setPath(MPDParseUtils::fixPath(details.url.path()));
}

RemoteKioDevice::RemoteKioDevice(DevicesModel *m, const RemoteFsDevice::Details &d)
    : Device(m, d.name)
    , scanned(false)
    , scanner(0)
    , details(d)
    , tempFile(0)
{
    details.url.setPath(MPDParseUtils::fixPath(details.url.path()));
    QString key=udi();
    opts.load(key);
    KConfigGroup grp(KGlobal::config(), key);
    opts.useCache=grp.readEntry("useCache", true);
    coverFileName=grp.readEntry("coverFileName", "cover.jpg");
    configured=KGlobal::config()->hasGroup(key);
}

RemoteKioDevice::~RemoteKioDevice()
{
    delete tempFile;
}

bool RemoteKioDevice::isConnected() const
{
    return scanned;
}

void RemoteKioDevice::configure(QWidget *parent)
{
    if (isRefreshing()) {
        return;
    }

    RemoteDevicePropertiesDialog *dlg=new RemoteDevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const DeviceOptions &, RemoteFsDevice::Details)),
            SLOT(saveProperties(const QString &, const DeviceOptions &, RemoteFsDevice::Details)));
    if (!configured) {
        connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
    }
    dlg->show(coverFileName, opts, details,
              DevicePropertiesWidget::Prop_All-(DevicePropertiesWidget::Prop_Folder+DevicePropertiesWidget::Prop_AutoScan),
              false, isConnected());
}

void RemoteKioDevice::rescan(bool full)
{
    // If this is the first scan (scanned=false) and we are set to use cache, attempt to load that before scanning
    if (!isRefreshing() && (scanned || !opts.useCache || !readCache())) {
        scanned=true;
        removeCache();
        startScanner(full);
    }
}

void RemoteKioDevice::addSong(const Song &s, bool overwrite)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    needToFixVa=opts.fixVariousArtists && s.isVariousArtists();
    overWrite=overwrite;

    if (!overwrite) {
        Song check=s;

        if (needToFixVa) {
            Device::fixVariousArtists(QString(), check, false);
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

    if (Device::constNoCover!=coverFileName) {
        currentBaseDir=MPDParseUtils::getDir(currentSong.file);
    }

    if (!opts.transcoderCodec.isEmpty()) {
        encoder=Encoders::getEncoder(opts.transcoderCodec);
        if (encoder.codec.isEmpty()) {
            emit actionStatus(CodecNotAvailable);
            return;
        }

        if (!opts.transcoderWhenDifferent || encoder.isDifferent(s.file)) {
            deleteTemp();
            QString destFile=createTemp(encoder.extension);

            if (!destFile.isEmpty()) {
                emit actionStatus(FailedToCreateTempFile);
                return;
            }
            transcoding=true;
            TranscodingJob *job=new TranscodingJob(encoder.params(opts.transcoderValue, s.file, destFile));
            connect(job, SIGNAL(result(KJob *)), SLOT(transcodeSongResult(KJob *)));
            connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(transcodePercent(KJob *, unsigned long)));
            job->start();
            currentSong.file=destFile;
            return;
        }
    }
    transcoding=false;
    if (needToFixVa) {
        // Need to copy file to a temp location , and then modify tags...
        deleteTemp();
        QString destFile=createTemp(QString()); //???
        if (!destFile.isEmpty()) {
            emit actionStatus(FailedToCreateTempFile);
            return;
        }
        QFile::copy(currentSong.file, destFile);
        currentSong.file=destFile;
        Device::fixVariousArtists(destFile, currentSong, true);
    }

    KUrl dest(details.url);
    dest.addPath(opts.createFilename(currentSong));
    KIO::FileCopyJob *job=KIO::file_copy(KUrl(s.file), dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
    connect(job, SIGNAL(result(KJob *)), SLOT(addSongResult(KJob *)));
    connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(jobProgress(KJob *, unsigned long)));
}

void RemoteKioDevice::copySongTo(const Song &s, const QString &baseDir, const QString &musicPath, bool overwrite)
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

    if (!overwrite && QFile::exists(baseDir+musicPath)) {
        emit actionStatus(FileExists);
        return;
    }

    currentBaseDir=baseDir;
    currentMusicPath=musicPath;
    KUrl dest(currentBaseDir+currentMusicPath);
    QDir dir(dest.directory());
    if (!dir.exists() && !Utils::createDir(dir.absolutePath(), baseDir)) {
        emit actionStatus(DirCreationFaild);
        return;
    }

    currentSong=s;
    KUrl url(details.url);
    url.addPath(s.file);
    KIO::FileCopyJob *job=KIO::file_copy(url, dest, -1, KIO::HideProgressInfo|(overwrite ? KIO::Overwrite : KIO::DefaultFlags));
    connect(job, SIGNAL(result(KJob *)), SLOT(copySongToResult(KJob *)));
    connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(jobProgress(KJob *, unsigned long)));
}

void RemoteKioDevice::removeSong(const Song &s)
{
    jobAbortRequested=false;
    if (!isConnected()) {
        emit actionStatus(NotConnected);
        return;
    }

    currentSong=s;
    KUrl url(details.url);
    url.addPath(s.file);
    KIO::SimpleJob *job=KIO::file_delete(url, KIO::HideProgressInfo);
    connect(job, SIGNAL(result(KJob *)), SLOT(removeSongResult(KJob *)));
}

void RemoteKioDevice::transcodeSongResult(KJob *job)
{
    if (jobAbortRequested) {
        deleteTemp();
        return;
    }
    if (job->error()) {
        emit actionStatus(TranscodeFailed);
    } else {
        if (needToFixVa) {
            Device::fixVariousArtists(tempFile->fileName(), currentSong, true);
        }
        KUrl dest(details.url);
        dest.addPath(opts.createFilename(currentSong));
        KIO::FileCopyJob *job=KIO::file_copy(KUrl(tempFile->fileName()), dest, -1, KIO::HideProgressInfo|(overWrite ? KIO::Overwrite : KIO::DefaultFlags));
        connect(job, SIGNAL(result(KJob *)), SLOT(addSongResult(KJob *)));
        connect(job, SIGNAL(percent(KJob *, unsigned long)), SLOT(jobProgress(KJob *, unsigned long)));
    }
}

void RemoteKioDevice::addSongResult(KJob *job)
{
    deleteTemp();
    if (job->error()) {
        emit actionStatus(transcoding ? TranscodeFailed : Failed);
    } else {
        currentSong.file=opts.createFilename(currentSong);
        addSongToList(currentSong);
        if (Device::constNoCover!=coverFileName) {
            KUrl destUrl(details.url);
            destUrl.addPath(MPDParseUtils::getDir(currentSong.file));
            Covers::copyCover(currentSong, KUrl(currentBaseDir), destUrl, coverFileName);
        }
        emit actionStatus(Ok);
    }
}

void RemoteKioDevice::copySongToResult(KJob *job)
{
    if (jobAbortRequested) {
        if (0!=job->percent() && 100!=job->percent() && QFile::exists(currentBaseDir+currentMusicPath)) {
            QFile::remove(currentBaseDir+currentMusicPath);
        }
        return;
    }
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        KUrl u(details.url);
        u.addPath(MPDParseUtils::getDir(currentSong.file));
        currentSong.file=currentMusicPath; // MPD's paths are not full!!!
        Covers::copyCover(currentSong, u, KUrl(currentBaseDir+MPDParseUtils::getDir(currentMusicPath)), QString());
        if (needToFixVa) {
            Device::fixVariousArtists(currentBaseDir+currentSong.file, currentSong, false);
        }
        Utils::setFilePerms(currentBaseDir+currentSong.file);
        MusicLibraryModel::self()->addSongToList(currentSong);
        DirViewModel::self()->addFileToList(currentSong.file);
        emit actionStatus(Ok);
    }
}

void RemoteKioDevice::removeSongResult(KJob *job)
{
    if (jobAbortRequested) {
        return;
    }
    if (job->error()) {
        emit actionStatus(Failed);
    } else {
        removeSongFromList(currentSong);
        emit actionStatus(Ok);
    }
}

void RemoteKioDevice::transcodePercent(KJob *job, unsigned long percent)
{
    if (jobAbortRequested) {
        job->kill(KJob::EmitResult); // emit result so that temp file can be removed!
        return;
    }
    emit progress(percent/2);
}

void RemoteKioDevice::jobProgress(KJob *job, unsigned long percent)
{
    Q_UNUSED(job)
    if (jobAbortRequested) {
        return;
    }
    emit progress(transcoding ? (50+(percent/2)) : percent);
}

void RemoteKioDevice::cleanDirs(const QSet<QString> &dirs)
{
    // TODO:!!!!
    Q_UNUSED(dirs)
//     KUrl u(details.url);
//     u.addPath(dir);
//     Utils::cleanDir(u, details.url, coverFileName);
}

double RemoteKioDevice::usedCapacity()
{
    return -1.0;
}

QString RemoteKioDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    return i18n("Capacity Unknown");
}

qint64 RemoteKioDevice::freeSpace()
{
    return 0;
}

void RemoteKioDevice::saveOptions()
{
    opts.save(udi());
}

void RemoteKioDevice::saveProperties()
{
    saveProperties(coverFileName, opts, details);
}

void RemoteKioDevice::saveProperties(const QString &newCoverFileName, const DeviceOptions &newOpts, RemoteFsDevice::Details newDetails)
{
    if (configured && opts==newOpts && newCoverFileName==coverFileName && details==newDetails) {
        return;
    }

    configured=true;
    RemoteFsDevice::Details oldDetails=details;
    newDetails.url.setPath(MPDParseUtils::fixPath(newDetails.url.path()));

    if (opts.useCache!=newOpts.useCache || newDetails.url!=oldDetails.url) { // Cache/url settings changed
        if (opts.useCache && newDetails.url==oldDetails.url) {
            saveCache();
        } else if (opts.useCache && !newOpts.useCache) {
            removeCache();
        }
    }

    bool newName=!oldDetails.name.isEmpty() && oldDetails.name!=newDetails.name;

    opts=newOpts;
    details=newDetails;
    coverFileName=newCoverFileName;
    QString key=udi();
    details.save(key);
    opts.save(key);
    KConfigGroup grp(KGlobal::config(), key);
    grp.writeEntry("useCache", opts.useCache);
    grp.writeEntry("coverFileName", coverFileName);

    if (newName) {
        setData(details.name);
        RemoteFsDevice::renamed(oldDetails.name, details.name);
        emit udiChanged(RemoteFsDevice::createUdi(oldDetails.name), RemoteFsDevice::createUdi(details.name));
        m_itemData=details.name;
        setStatusMessage(QString());
    }
}

bool RemoteKioDevice::readCache()
{
    if (opts.useCache) {
        KUrl cache=cacheUrl();
        if (KIO::NetAccess::exists(cache, KIO::NetAccess::SourceSide, QApplication::activeWindow())) {
            KIO::Job *job = KIO::get(cache, KIO::NoReload, KIO::HideProgressInfo);
            QByteArray data;
            if (KIO::NetAccess::synchronousRun(job, QApplication::activeWindow(), &data) && !data.isEmpty()) {
                QXmlStreamReader reader(data);
                MusicLibraryItemRoot *root=new MusicLibraryItemRoot;
                if (root->fromXML(reader)) {
                    update=root;
                    scanned=true;
                    QTimer::singleShot(0, this, SLOT(cacheRead()));
                    return true;
                }
                delete root;
            }
        }
    }

    return false;
}

void RemoteKioDevice::cacheRead()
{
    setStatusMessage(QString());
    emit updating(udi(), false);
}

void RemoteKioDevice::startScanner(bool fullScan)
{
    stopScanner();
    scanner=new KioScanner(this);
    connect(scanner, SIGNAL(status(const QString&)), this, SLOT(libraryUpdated(const QString&)), Qt::QueuedConnection);
    connect(scanner, SIGNAL(songCount(int)), this, SLOT(songCount(int)));
    QSet<Song> existingSongs;
    if (!fullScan) {
        existingSongs=allSongs();
    }
    scanner->scan(details.url, existingSongs);
    setStatusMessage(i18n("Updating..."));
    emit updating(udi(), true);
}

void RemoteKioDevice::stopScanner(bool showStatus)
{
    // Scan for music in a separate thread...
    if (scanner) {
        disconnect(scanner, SIGNAL(status(const QString&)), this, SLOT(libraryUpdated(const QString&)));
        disconnect(scanner, SIGNAL(songCount(int)), this, SLOT(songCount(int)));
        scanner->deleteLater();
        scanner->stop();
        scanner=0;
        if (showStatus) {
            setStatusMessage(QString());
            emit updating(udi(), false);
        }
    }
}

void RemoteKioDevice::libraryUpdated(const QString &errorMsg)
{
    if (scanner) {
        if (errorMsg.isEmpty()) {
            if (update) {
                delete update;
            }
            update=scanner->takeLibrary();
            if (opts.useCache && update) {
                saveCache();
            }
        } else {
            emit error(errorMsg);
        }
        stopScanner();
    }
}

KUrl RemoteKioDevice::cacheUrl()
{
    KUrl u(details.url);
    u.addPath(constCantataCacheFile);

    return u;
}

void RemoteKioDevice::saveCache()
{
    if (opts.useCache) {
        QByteArray data;
        QXmlStreamWriter writer(&data);
        update->toXML(writer);
        KIO::Job *job = KIO::storedPut(data, cacheUrl(), -1, KIO::DefaultFlags);
        KIO::NetAccess::synchronousRun(job, QApplication::activeWindow());
    }
}

void RemoteKioDevice::removeCache()
{
    KUrl cache=cacheUrl();
    if (KIO::NetAccess::exists(cache, KIO::NetAccess::SourceSide, QApplication::activeWindow())) {
        KIO::NetAccess::del(cache, QApplication::activeWindow());
    }
}

void RemoteKioDevice::deleteTemp()
{
    if (tempFile) {
        tempFile->remove();
        delete tempFile;
        tempFile=0;
    }
}

QString RemoteKioDevice::createTemp(const QString &ext)
{
    deleteTemp();
    tempFile=new KTemporaryFile();
    if (!ext.isEmpty()) {
        tempFile->setSuffix("."+encoder.extension);
    }
    tempFile->setAutoRemove(false);

    if (!tempFile->open()) {
        deleteTemp();
        return QString();
    }
    QString destFile=tempFile->fileName();
    tempFile->close();
    if (QFile::exists(destFile)) {
        QFile::remove(destFile);
    }
    return destFile;
}
