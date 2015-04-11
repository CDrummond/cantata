/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "covers.h"
#include "mpd-interface/song.h"
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include "network/networkaccessmanager.h"
#include "settings.h"
#include "config.h"
#include "devices/deviceoptions.h"
#include "support/thread.h"
#ifdef ENABLE_ONLINE_SERVICES
#include "online/soundcloudservice.h"
#include "online/podcastservice.h"
#include "online/jamendoservice.h"
#include "models/onlineservicesmodel.h"
#endif
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/device.h"
#include "models/devicesmodel.h"
#endif
#ifdef TAGLIB_FOUND
#include "tags/tags.h"
#endif
#include "support/globalstatic.h"
#include "widgets/icons.h"
#include <QFile>
#include <QDir>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QTextStream>
#include <qglobal.h>
#include <QIcon>
#include <QImage>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QXmlStreamReader>
#include <QTimer>
#include <QApplication>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#endif

GLOBAL_STATIC(Covers, instance)

#include <QDebug>
static bool debugIsEnabled=false;
#define DBUG_CLASS(CLASS) if (debugIsEnabled) qWarning() << CLASS << QThread::currentThread()->objectName() << __FUNCTION__
#define DBUG DBUG_CLASS(metaObject()->className())
void Covers::enableDebug()
{
    debugIsEnabled=true;
}

bool Covers::debugEnabled()
{
    return debugIsEnabled;
}

const QLatin1String Covers::constLastFmApiKey("5a854b839b10f8d46e630e8287c2299b");
const QLatin1String Covers::constCoverDir("covers/");
const QLatin1String Covers::constScaledCoverDir("covers-scaled/");
const QLatin1String Covers::constCddaCoverDir("cdda/");
const QLatin1String Covers::constFileName("cover");
const QLatin1String Covers::constArtistImage("artist");
const QLatin1String Covers::constComposerImage("composer");
const QString Covers::constCoverInTagPrefix=QLatin1String("{tag}");

static const char * constExtensions[]={".jpg", ".png", 0};
static bool saveInMpdDir=true;
static bool fetchCovers=true;
static QString constNoCover=QLatin1String("{nocover}");

#if QT_VERSION >= 0x050100
static double devicePixelRatio=1.0;
// Only scale images to device pixel ratio if un-scaled size is less then 300pixels.
static const int constRetinaScaleMaxSize=300;
#endif

static QImage scale(const Song &song, const QImage &img, int size)
{
    if (song.isArtistImageRequest() || song.isComposerImageRequest()) {
        QImage scaled=img.scaled(QSize(size, size), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (scaled.width()>size || scaled.height()>size) {
            scaled=scaled.copy((scaled.width()-size)/2, 0, size, size);
        }
        return scaled;
    }
    return img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

static bool canSaveTo(const QString &dir)
{
    QString mpdDir=MPDConnection::self()->getDetails().dir;
    return !dir.isEmpty() && !mpdDir.isEmpty() && !mpdDir.startsWith(QLatin1String("http://")) && QDir(mpdDir).exists() && dir.startsWith(mpdDir);
}

static const QString typeFromRaw(const QByteArray &raw)
{
    if (Covers::isJpg((raw))) {
        return constExtensions[0];
    } else if (Covers::isPng(raw)) {
        return constExtensions[1];
    }
    return QString();
}

static QString save(const QString &mimeType, const QString &extension, const QString &filePrefix, const QImage &img, const QByteArray &raw)
{
    if (!mimeType.isEmpty() && extension==mimeType) {
        if (QFile::exists(filePrefix+mimeType)) {
            return filePrefix+mimeType;
        }

        QFile f(filePrefix+mimeType);
        if (f.open(QIODevice::WriteOnly) && raw.size()==f.write(raw)) {
            if (!MPDConnection::self()->getDetails().dir.isEmpty() && filePrefix.startsWith(MPDConnection::self()->getDetails().dir)) {
                Utils::setFilePerms(filePrefix+mimeType);
            }
            return filePrefix+mimeType;
        }
    }

    if (extension!=mimeType) {
        if (QFile::exists(filePrefix+extension)) {
            return filePrefix+extension;
        }

        if (img.save(filePrefix+extension)) {
            if (!MPDConnection::self()->getDetails().dir.isEmpty() && filePrefix.startsWith(MPDConnection::self()->getDetails().dir)) {
                Utils::setFilePerms(filePrefix+mimeType);
            }
            return filePrefix+extension;
        }
    }

    return QString();
}

static QImage loadImage(const QString &fileName)
{
    QImage img(fileName);
    #ifndef ENABLE_UBUNTU
    if (img.isNull()) {
        // Failed to load, perhaps extension is wrong? If so try PNG for .jpg, and vice versa...
        QFile f(fileName);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray header=f.read(10);
            f.reset();
            img.load(&f, typeFromRaw(header).toLatin1());
            if (!img.isNull()) {
                DBUG_CLASS("Covers") << fileName << "has wrong extension!";
            }
        }
    }
    #endif
    return img;
}


static inline bool isOnlineServiceImage(const Song &s)
{
    #ifdef ENABLE_ONLINE_SERVICES
    return OnlineService::showLogoAsCover(s);
    #else
    Q_UNUSED(s)
    return false;
    #endif
}

#ifdef ENABLE_ONLINE_SERVICES
static Covers::Image serviceImage(const Song &s)
{
    Covers::Image img;
    if (SoundCloudService::constName==s.onlineService()) {
        img.fileName=SoundCloudService::iconPath();
    } else if (PodcastService::constName==s.onlineService()) {
        img.fileName=PodcastService::iconPath();
    }
    if (!img.fileName.isEmpty()) {
        img.img=loadImage(img.fileName);
        if (!img.img.isNull()) {
            DBUG_CLASS("Covers") << s.onlineService();
        }
    }
    return img;
}
#endif

static inline QString albumKey(const Song &s)
{
    if (Song::SingleTracks==s.type) {
        return QLatin1String("-single-tracks-");
    }
    if (s.isStandardStream()) {
        return QLatin1String("-stream-");
    }
    #ifdef ENABLE_ONLINE_SERVICES
    if (isOnlineServiceImage(s)) {
        return s.onlineService();
    }
    #endif
    return "{"+s.albumArtist()+"}{"+s.albumId()+"}";
}

static inline QString artistKey(const Song &s)
{
    return "{"+s.albumArtist()+"}";
}

static inline QString composerKey(const Song &s)
{
    return "{"+s.composer()+"}";
}

static inline QString songKey(const Song &s)
{
    return s.isArtistImageRequest() ? artistKey(s) : s.isComposerImageRequest() ? composerKey(s) : albumKey(s);
}

static inline QString cacheKey(const Song &song, int size)
{
    if (song.isArtistImageRequest() && song.isVariousArtists()) {
        return QLatin1String("va")+QString::number(size);
    } else if (Song::SingleTracks==song.type) {
        return QLatin1String("single")+QString::number(size);
    } else if (song.isStandardStream()) {
        return QLatin1String("str")+QString::number(size);
    }
    #ifdef ENABLE_ONLINE_SERVICES
    else if (isOnlineServiceImage(song)) {
        return song.onlineService()+QString::number(size);
    }
    #endif

    return songKey(song)+QString::number(size);
}

static const QLatin1String constScaledFormat(".jpg");
static bool cacheScaledCovers=true;

static QString getScaledCoverName(const Song &song, int size, bool createDir)
{
    if (song.isArtistImageRequest()) {
        QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/'), createDir);
        return dir.isEmpty() ? QString() : (dir+Covers::encodeName(song.albumArtist())+constScaledFormat);
    }

    if (song.isComposerImageRequest()) {
        QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/'), createDir);
        return dir.isEmpty() ? QString() : (dir+Covers::encodeName(song.composer())+constScaledFormat);
    }

    QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/')+Covers::encodeName(song.albumArtist()), createDir);
    return dir.isEmpty() ? QString() : (dir+Covers::encodeName(song.albumId())+constScaledFormat);
}

static void clearScaledCache(const Song &song)
{
    QString dirName=Utils::cacheDir(Covers::constScaledCoverDir, false);
    if (dirName.isEmpty()) {
        return;
    }

    QDir d(dirName);
    if (!d.exists()) {
        return;
    }

    DBUG_CLASS("Covers") << song.file << song.artist << song.albumartist << song.album;
    QStringList sizeDirNames=d.entryList(QStringList() << "*", QDir::Dirs|QDir::NoDotAndDotDot);

    if (song.isArtistImageRequest() || song.isComposerImageRequest()) {
        bool artistImage=song.isArtistImageRequest();

        for (int i=0; constExtensions[i]; ++i) {
            QString fileName=Covers::encodeName(artistImage ? song.artist : song.composer())+constExtensions[i];
            foreach (const QString &sizeDirName, sizeDirNames) {
                QString fname=dirName+sizeDirName+QLatin1Char('/')+fileName;
                if (QFile::exists(fname)) {
                    QFile::remove(fname);
                }
            }
        }
    } else {
        QString subDir=Covers::encodeName(song.artist);
        for (int i=0; constExtensions[i]; ++i) {
            QString fileName=Covers::encodeName(song.album)+constExtensions[i];
            foreach (const QString &sizeDirName, sizeDirNames) {
                QString fname=dirName+sizeDirName+QLatin1Char('/')+subDir+QLatin1Char('/')+fileName;
                if (QFile::exists(fname)) {
                    QFile::remove(fname);
                }
            }
        }
    }
}

static QImage loadScaledCover(const Song &song, int size)
{
    QString fileName=getScaledCoverName(song, size, false);
    if (!fileName.isEmpty()) {
        if (QFile::exists(fileName)) {
            QImage img(fileName, "JPG");
            if (!img.isNull() && (img.width()==size || img.height()==size)) {
                DBUG_CLASS("Covers") << song.albumArtist() << song.albumId() << size << "scaled cover found" << fileName;
                return img;
            }
        } else { // Remove any previous PNG scaled cover...
            fileName=Utils::changeExtension(fileName, ".png");
            if (QFile::exists(fileName)) {
                QFile::remove(fileName);
            }
        }
    }
    return QImage();
}

bool Covers::isJpg(const QByteArray &data)
{
    return data.size()>9 && /*data[0]==0xFF && data[1]==0xD8 && data[2]==0xFF*/ data[6]=='J' && data[7]=='F' && data[8]=='I' && data[9]=='F';
}

bool Covers::isPng(const QByteArray &data)
{
    return data.size()>4 && /*data[0]==0x89 &&*/ data[1]=='P' && data[2]=='N' && data[3]=='G';
}

const char * Covers::imageFormat(const QByteArray &data)
{
    return isJpg(data) ? "JPG" : (isPng(data) ? "PNG" : 0);
}

QString Covers::encodeName(QString name)
{
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    #if defined Q_OS_WIN
    name.replace(QLatin1Char('?'), QLatin1Char('_'));
    name.replace(QLatin1Char(':'), QLatin1Char('_'));
    name.replace(QLatin1Char('<'), QLatin1Char('_'));
    name.replace(QLatin1Char('>'), QLatin1Char('_'));
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    name.replace(QLatin1Char('*'), QLatin1Char('_'));
    name.replace(QLatin1Char('|'), QLatin1Char('_'));
    name.replace(QLatin1Char('\"'), QLatin1Char('_'));
    #elif defined Q_OS_MAC
    name.replace(QLatin1Char(':'), QLatin1Char('_'));
    if (name.startsWith(QLatin1Char('.'))) {
        name[0]=QLatin1Char('_');
    }
    if (name.length()>255) {
        name=name.left(255);
    }
    #else // Linux
    #ifdef ENABLE_UBUNTU
    // qrc:/ does not seem to like ?
    name.replace(QLatin1Char('?'), QLatin1Char('_'));
    name.replace(QLatin1Char(':'), QLatin1Char('_'));
    name.replace(QLatin1Char('%'), QLatin1Char('_'));
    #endif // ENABLE_UBUNTU
    if (name.startsWith(QLatin1Char('.'))) {
        name[0]=QLatin1Char('_');
    }
    #endif
    return name;
}

QString Covers::albumFileName(const Song &song)
{
    QString coverName=MPDConnection::self()->getDetails().coverName;
    if (coverName.isEmpty()) {
        coverName=Covers::constFileName;
    }
    #ifndef ENABLE_UBUNTU
    else if (coverName.contains(QLatin1Char('%'))) {
        coverName.replace(DeviceOptions::constAlbumArtist, encodeName(song.albumArtist()));
        coverName.replace(DeviceOptions::constTrackArtist, encodeName(song.albumArtist()));
        coverName.replace(DeviceOptions::constAlbumTitle, encodeName(song.album));
        coverName.replace(QLatin1String("%"), QLatin1String(""));
    }
    #endif
    return coverName;
}

QString Covers::artistFileName(const Song &song)
{
    QString coverName=MPDConnection::self()->getDetails().coverName;
    if (coverName.contains(QLatin1Char('%'))) {
        return encodeName(song.albumArtist());
    }
    return constArtistImage;
}

QString Covers::composerFileName(const Song &song)
{
    QString coverName=MPDConnection::self()->getDetails().coverName;
    if (coverName.contains(QLatin1Char('%'))) {
        return encodeName(song.composer());
    }
    return constComposerImage;
}

QString Covers::fixArtist(const QString &artist)
{
    if (artist.isEmpty()) {
        return artist;
    }

    static QMap<QString, QString> artistMap;
    static bool initialised=false;
    if (!initialised) {
        initialised=true;
        QStringList dirs=QStringList() << Utils::dataDir() << CANTATA_SYS_CONFIG_DIR;
        foreach (const QString &dir, dirs) {
            if (dir.isEmpty()) {
                continue;
            }

            QFile f(dir+QLatin1String("/tag_fixes.xml"));
            if (f.open(QIODevice::ReadOnly)) {
                QXmlStreamReader doc(&f);
                while (!doc.atEnd()) {
                    doc.readNext();
                    if (doc.isStartElement() && QLatin1String("artist")==doc.name()) {
                        QString from=doc.attributes().value("from").toString();
                        QString to=doc.attributes().value("to").toString();
                        if (!from.isEmpty() && !to.isEmpty() && from!=to && !artistMap.contains(from)) {
                            artistMap.insert(from, to);
                        }
                    }
                }
            }
        }
    }

    QMap<QString, QString>::ConstIterator it=artistMap.find(artist);
    return it==artistMap.constEnd() ? artist : it.value();
}

const QSize Covers::constMaxSize(600, 600);

//bool Covers::isCoverFile(const QString &file)
//{
//    return standardNames().contains(file);
//}

static bool fExists(const QString &dir, const QString &file)
{
    return QFile::exists(dir+file);
}

static bool fCopy(const QString &sDir, const QString &sFile, const QString &dDir, const QString &dFile)
{
    return QFile::copy(sDir+sFile, dDir+dFile);
}

bool Covers::copyImage(const QString &sourceDir, const QString &destDir, const QString &coverFile, const QString &destName, unsigned short maxSize)
{
    QImage img(sourceDir+coverFile);
    bool ok=false;
    if (maxSize>0 && (img.width()>maxSize || img.height()>maxSize)) { // Need to scale image...
        img=img.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ok=img.save(destDir+destName);
    } else if (destName.right(4)!=coverFile.right(4)) { // Diff extensions, so need to convert image type...
        ok=img.save(destDir+destName);
    } else { // no scaling, and same image type, so we can just copy...
        ok=fCopy(sourceDir, coverFile, destDir, destName);
    }
    Utils::setFilePerms(destDir+destName);
    return ok;
}

bool Covers::copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name, unsigned short maxSize)
{
    // First, check if dir already has a cover file!
    QStringList names=standardNames();
    for (int e=0; constExtensions[e]; ++e) {
        names+=song.album+constExtensions[e];
    }

    QString mpdCover=albumFileName(song);

    for (int e=0; constExtensions[e]; ++e) {
        if (!names.contains(mpdCover+constExtensions[e])) {
            names.prepend(mpdCover+constExtensions[e]);
        }
    }

    for (int e=0; constExtensions[e]; ++e) {
        names+=song.albumArtist()+QLatin1String(" - ")+song.album+constExtensions[e];
    }
    foreach (const QString &coverFile, names) {
        if (fExists(destDir, coverFile)) {
            return true;
        }
    }

    // No cover found, try to copy from source folder
    foreach (const QString &coverFile, names) {
        if (fExists(sourceDir, coverFile)) {
            QString destName(name);
            if (destName.isEmpty()) { // copying into mpd dir, so we want cover.jpg/png...
                if (standardNames().at(0)!=coverFile) { // source is not 'cover.xxx'
                    QString ext(coverFile.endsWith(constExtensions[0]) ? constExtensions[0] : constExtensions[1]);
                    destName=mpdCover+ext;
                } else {
                    destName=coverFile;
                }
            }
            copyImage(sourceDir, destDir, coverFile, destName, maxSize);
            return true;
        }
    }

    QString destName(name);
    if (!destName.isEmpty()) {
        // Copying ONTO a device
        // None in source folder. Do we have a cached cover?
        QString artist=encodeName(song.albumArtist());
        QString album=encodeName(song.album);
        QString dir(Utils::cacheDir(constCoverDir+artist, false));
        for (int e=0; constExtensions[e]; ++e) {
            if (QFile::exists(dir+album+constExtensions[e])) {
                copyImage(dir, destDir, album+constExtensions[e], destName, maxSize);
                return true;
            }
        }
    }

    return false;
}

const QStringList & Covers::standardNames()
{
    static QStringList *coverFileNames;
    if (!coverFileNames) {
        coverFileNames=new QStringList();
        QStringList fileNames;
        fileNames << Covers::constFileName << QLatin1String("AlbumArt") << QLatin1String("folder");
        foreach (const QString &fileName, fileNames) {
            for (int e=0; constExtensions[e]; ++e) {
                *coverFileNames << fileName+constExtensions[e];
            }
        }
    }

    return *coverFileNames;
}

CoverDownloader::CoverDownloader()
    : manager(0)
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

void CoverDownloader::stop()
{
    thread->stop();
}

void CoverDownloader::download(const Song &song)
{
    DBUG << song.file << song.artist << song.albumartist << song.album;
    #ifdef ENABLE_ONLINE_SERVICES
    if (song.isFromOnlineService()) {
        QString serviceName=song.onlineService();
        QString imageUrl=song.extraField(Song::OnlineImageUrl);
        // Jamendo image URL is just the album ID!
        if (!imageUrl.isEmpty() && !imageUrl.startsWith("http:/") && imageUrl.length()<15 && serviceName==JamendoService::constName) {
            imageUrl=JamendoService::imageUrl(imageUrl);
        }

        Job job(song, QString());
        job.type=JobHttpJpg;
        DBUG << "Online image url" << imageUrl;
        if (!imageUrl.isEmpty()) {
            NetworkJob *j=network()->get(imageUrl);
            jobs.insert(j, job);
            connect(j, SIGNAL(finished()), this, SLOT(onlineJobFinished()));
        } else {
            failed(job);
        }
        return;
    }
    #endif

    if (jobs.end()!=findJob(Job(song, QString()))) {
        return;
    }

    QString dirName;
    QString fileName=song.filePath();
    bool haveAbsPath=fileName.startsWith(Utils::constDirSep);

    if (haveAbsPath || !MPDConnection::self()->getDetails().dir.isEmpty()) {
        dirName=fileName.endsWith(Utils::constDirSep) ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+fileName
                                                      : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+fileName);
    }

    Job job(song, dirName);

    if (!MPDConnection::self()->getDetails().dir.isEmpty() && MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http://"))) {
        downloadViaHttp(job, JobHttpJpg);
    } else if (fetchCovers) {
        downloadViaLastFm(job);
    } else {
        failed(job);
    }
}

bool CoverDownloader::downloadViaHttp(Job &job, JobType type)
{
    QUrl u;
    QString coverName=job.song.isArtistImageRequest()
                            ? Covers::artistFileName(job.song)
                            : job.song.isComposerImageRequest()
                                ? Covers::composerFileName(job.song)
                                : Covers::albumFileName(job.song);
    coverName+=constExtensions[JobHttpJpg==type ? 0 : 1];
    QString dir=Utils::getDir(job.filePath);
    if (job.song.isArtistImageRequest() || job.song.isComposerImageRequest()) {
        if (job.level) {
            QStringList parts=dir.split(Utils::constDirSep, QString::SkipEmptyParts);
            if (parts.size()<job.level) {
                return false;
            }
            dir=QString();
            for (int i=0; i<(parts.length()-job.level); ++i) {
                dir+=parts.at(i)+Utils::constDirSep;
            }
        }
        job.level++;
    }
    #if QT_VERSION < 0x050000
    u.setEncodedUrl(MPDConnection::self()->getDetails().dir.toLatin1()+QUrl::toPercentEncoding(dir, Utils::constDirSepCharStr)+coverName.toLatin1());
    #else
    u=QUrl(MPDConnection::self()->getDetails().dir+dir+coverName.toLatin1());
    #endif

    job.type=type;
    NetworkJob *j=network()->get(u);
    connect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
    jobs.insert(j, job);
    DBUG << u.toString();
    return true;
}

void CoverDownloader::downloadViaLastFm(Job &job)
{
    QUrl url("https://ws.audioscrobbler.com/2.0/");
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("method", job.song.isArtistImageRequest() ? "artist.getInfo" : "album.getInfo");
    query.addQueryItem("api_key", Covers::constLastFmApiKey);
    query.addQueryItem("autocorrect", "1");
    query.addQueryItem("artist", Covers::fixArtist(job.song.albumArtist()));
    if (!job.song.isArtistImageRequest()) {
        query.addQueryItem("album", job.song.album);
    }
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    NetworkJob *j = network()->get(url);
    connect(j, SIGNAL(finished()), this, SLOT(lastFmCallFinished()));
    job.type=JobLastFm;
    jobs.insert(j, job);
    DBUG << url.toString();
}

void CoverDownloader::lastFmCallFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    QHash<NetworkJob *, Job>::Iterator it(jobs.find(reply));
    QHash<NetworkJob *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        Job job=it.value();
        jobs.erase(it);
        QString url;

        if(reply->ok()) {
            QXmlStreamReader doc(reply->readAll());
            QString largeUrl;
            bool inSection=false;
            bool isArtistImage=job.song.isArtistImageRequest();

            while (!doc.atEnd()) {
                doc.readNext();

                if (doc.isStartElement()) {
                    if (!inSection && QLatin1String(isArtistImage ? "artist" : "album")==doc.name()) {
                        inSection=true;
                    } else if (inSection && QLatin1String("image")==doc.name()) {
                        QString size=doc.attributes().value("size").toString();
                        if (QLatin1String("extralarge")==size) {
                            url = doc.readElementText();
                        } else if (QLatin1String("large")==size) {
                            largeUrl = doc.readElementText();
                        }
                        if (!url.isEmpty() && !largeUrl.isEmpty()) {
                            break;
                        }
                    }
                } else if (doc.isEndElement() && inSection && QLatin1String(isArtistImage ? "artist" : "album")==doc.name()) {
                    break;
                }
            }

            if (url.isEmpty() && !largeUrl.isEmpty()) {
                url=largeUrl;
            }
        }

        if (!url.isEmpty()) {
            QUrl u;
            #if QT_VERSION < 0x050000
            u.setEncodedUrl(url.toLatin1());
            #else
            u=QUrl(url);
            #endif
            NetworkJob *j=network()->get(QNetworkRequest(u));
            connect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
            DBUG << "download" << u.toString();
            jobs.insert(j, job);
        } else {
            failed(job);
        }
    }
    reply->deleteLater();
}

void CoverDownloader::jobFinished()
{
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (!reply) {
        return;
    }

    DBUG << "status" << reply->error() << reply->errorString();

    QHash<NetworkJob *, Job>::Iterator it(jobs.find(reply));
    QHash<NetworkJob *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        QByteArray data=reply->ok() ? reply->readAll() : QByteArray();
        Covers::Image img;
        img.img= data.isEmpty() ? QImage() : QImage::fromData(data, Covers::imageFormat(data));
        Job job=it.value();

        if (!img.img.isNull() && img.img.size().width()<32) {
            img.img = QImage();
        }

        jobs.remove(it.key());
        if (img.img.isNull() && JobLastFm!=job.type) {
            if (JobHttpJpg==job.type) {
                if (!job.level || !downloadViaHttp(job, JobHttpJpg)) {
                    job.level=0;
                    downloadViaHttp(job, JobHttpPng);
                }
            } else if (fetchCovers && JobHttpPng==job.type && (!job.level || !downloadViaHttp(job, JobHttpPng)) && !job.song.isComposerImageRequest()) {
                downloadViaLastFm(job);
            } else {
                failed(job);
            }
        } else {
            if (!img.img.isNull()) {
                if (img.img.size().width()>Covers::constMaxSize.width() || img.img.size().height()>Covers::constMaxSize.height()) {
                    img.img=img.img.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                img.fileName=saveImg(job, img.img, data);
            }

            if (job.song.isArtistImageRequest()) {
                DBUG << "artist image, null?" << img.img.isNull();
                emit artistImage(job.song, img.img, img.fileName);
            } else if (job.song.isComposerImageRequest()) {
                DBUG << "compser image, null?" << img.img.isNull();
                emit composerImage(job.song, img.img, img.fileName);
            } else if (img.img.isNull()) {
                DBUG << "failed to download cover image";
                emit cover(job.song, QImage(), QString());
            } else {
                DBUG << "got cover image" << img.fileName;
                emit cover(job.song, img.img, img.fileName);
            }
        }
    }

    reply->deleteLater();
}

void CoverDownloader::onlineJobFinished()
{
    #ifdef ENABLE_ONLINE_SERVICES
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());

    if (!reply) {
        return;
    }
    reply->deleteLater();

    DBUG << "status" << reply->error() << reply->errorString();

    QHash<NetworkJob *, Job>::Iterator it(jobs.find(reply));
    QHash<NetworkJob *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        QByteArray data=QNetworkReply::NoError==reply->error() ? reply->readAll() : QByteArray();
        if (data.isEmpty()) {
            DBUG << reply->url().toString() << "empty!";
            return;
        }

        Job job=it.value();
        const Song &song=job.song;
        QString id=song.onlineService();
        QString fileName;
        QImage img=data.isEmpty() ? QImage() : QImage::fromData(data, Covers::imageFormat(data));

        bool png=Covers::isPng(data);
        DBUG << "Got image" << id << song.artist << song.album << png;
        if (!img.isNull()) {
            if (img.size().width()>Covers::constMaxSize.width() || img.size().height()>Covers::constMaxSize.height()) {
                img=img.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            QString cacheName=song.extraField(Song::OnlineImageCacheName);
            fileName=cacheName.isEmpty()
                        ? Utils::cacheDir(id.toLower(), true)+Covers::encodeName(song.album.isEmpty() ? song.artist : (song.artist+" - "+song.album))+(png ? ".png" : ".jpg")
                        : cacheName;
            QFile f(fileName);
            if (f.open(QIODevice::WriteOnly)) {
                DBUG << "Saved image to" << fileName;
                f.write(data);
            }
        }
        emit cover(job.song, img, fileName);
    }
    #endif
}

void CoverDownloader::failed(const Job &job)
{
    if (job.song.isArtistImageRequest()) {
        DBUG << "artist image" << job.song.artist;
        emit artistImage(job.song, QImage(), QString());
    } else if (job.song.isComposerImageRequest()) {
        DBUG << "composer image" << job.song.composer();
        emit composerImage(job.song, QImage(), QString());
    } else {
        DBUG << "cover image" << job.song.artist << job.song.album;
        emit cover(job.song, QImage(), QString());
    }
}

QString CoverDownloader::saveImg(const Job &job, const QImage &img, const QByteArray &raw)
{
    QString mimeType=typeFromRaw(raw);
    QString extension=mimeType.isEmpty() ? constExtensions[0] : mimeType;
    QString savedName;

    if (job.song.isCdda()) {
        QString dir = Utils::cacheDir(Covers::constCddaCoverDir, true);
        if (!dir.isEmpty()) {
            savedName=save(mimeType, extension, dir+job.filePath.mid(7), img, raw);
            if (!savedName.isEmpty()) {
                DBUG << job.song.file << savedName;
                return savedName;
            }
        }
        return QString();
    }

    if (job.song.isArtistImageRequest() || job.song.isComposerImageRequest()) {
        if (saveInMpdDir && !job.song.isNonMPD() && canSaveTo(job.dir)) {
            QString mpdDir=MPDConnection::self()->getDetails().dir;
            if (!mpdDir.isEmpty() && job.dir.startsWith(mpdDir) && 2==job.dir.mid(mpdDir.length()).split(Utils::constDirSep, QString::SkipEmptyParts).count()) {
                QDir d(job.dir);
                d.cdUp();
                savedName=save(mimeType, extension, d.absolutePath()+Utils::constDirSep+
                               (job.song.isArtistImageRequest() ? Covers::artistFileName(job.song) : Covers::composerFileName(job.song)), img, raw);
                if (!savedName.isEmpty()) {
                    DBUG << job.song.file << savedName;
                    return savedName;
                }
            }
        }

        QString dir = Utils::cacheDir(Covers::constCoverDir, true);
        if (!dir.isEmpty()) {
            savedName=save(mimeType, extension, dir+Covers::encodeName(job.song.basicArtist()), img, raw);
            if (!savedName.isEmpty()) {
                DBUG << job.song.file << savedName;
                return savedName;
            }
        }
    } else {
        // Try to save as cover.jpg in album dir...
        if (saveInMpdDir && !job.song.isNonMPD() && canSaveTo(job.dir)) {
            QString coverName=Covers::albumFileName(job.song);
            savedName=save(mimeType, extension, job.dir+coverName, img, raw);
            if (!savedName.isEmpty()) {
                DBUG << job.song.file << savedName;
                return savedName;
            }
        }

        // Could not save with album, save in cache dir...
        QString dir = Utils::cacheDir(Covers::constCoverDir+Covers::encodeName(job.song.albumArtist()), true);
        if (!dir.isEmpty()) {
            savedName=save(mimeType, extension, dir+Covers::encodeName(job.song.album), img, raw);
            if (!savedName.isEmpty()) {
                DBUG << job.song.file << savedName;
                return savedName;
            }
        }
    }

    return QString();
}

QHash<NetworkJob *, CoverDownloader::Job>::Iterator CoverDownloader::findJob(const Job &job)
{
    QHash<NetworkJob *, Job>::Iterator it(jobs.begin());
    QHash<NetworkJob *, Job>::Iterator end(jobs.end());

    for (; it!=end; ++it) {
        if ( ((*it).song.isComposerImageRequest()==job.song.isComposerImageRequest() && (*it).song.composer()==job.song.composer()) ||
             ((*it).song.isArtistImageRequest()==job.song.isArtistImageRequest() && (*it).song.albumArtist()==job.song.albumArtist() && (*it).song.album==job.song.album) ) {
            return it;
        }
    }

    return end;
}

NetworkAccessManager * CoverDownloader::network()
{
    if (!manager) {
        manager=new NetworkAccessManager(this);
    }
    return manager;
}

CoverLocator::CoverLocator()
    : timer(0)
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

void CoverLocator::stop()
{
    thread->stop();
}

void CoverLocator::startTimer(int interval)
{
    if (!timer) {
        timer=new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, SIGNAL(timeout()), this, SLOT(locate()), Qt::QueuedConnection);
    }
    timer->start(interval);
}

void CoverLocator::locate(const Song &s)
{
    queue.append(s);
    startTimer(0);
}

// To improve responsiveness of views, we only process a max of X images per even loop iteration.
// If more images are asked for, we place these into a list, and get them on the next iteration
// of the loop. This way things appear smoother.
static int maxCoverUpdatePerIteration=10;

void CoverLocator::locate()
{
    QList<Song> toDo;
    for (int i=0; i<maxCoverUpdatePerIteration && !queue.isEmpty(); ++i) {
        toDo.append(queue.takeFirst());
    }
    if (toDo.isEmpty()) {
        return;
    }
    QList<LocatedCover> covers;
    foreach (const Song &s, toDo) {
        DBUG << s.file << s.artist << s.albumartist << s.album;
        Covers::Image img=Covers::locateImage(s);
        covers.append(LocatedCover(s, img.img, img.fileName));
    }
    if (!covers.isEmpty()) {
        DBUG << "located" << covers.count();
        emit located(covers);
    }

    if (!queue.isEmpty()) {
        startTimer(0);
    }
}

CoverLoader::CoverLoader()
    : timer(0)
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

void CoverLoader::stop()
{
    thread->stop();
}

void CoverLoader::startTimer(int interval)
{
    if (!timer) {
        timer=new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, SIGNAL(timeout()), this, SLOT(load()), Qt::QueuedConnection);
    }
    timer->start(interval);
}

void CoverLoader::load(const Song &song)
{
    queue.append(LoadedCover(song));
    startTimer(0);
}

void CoverLoader::load()
{
    QList<LoadedCover> toDo;
    for (int i=0; i<maxCoverUpdatePerIteration && !queue.isEmpty(); ++i) {
        toDo.append(queue.takeFirst());
    }
    if (toDo.isEmpty()) {
        return;
    }
    QList<LoadedCover> covers;
    foreach (const LoadedCover &s, toDo) {
        DBUG << s.song.artist << s.song.albumId() << s.song.size;
        int size=s.song.size;
        #if QT_VERSION >= 0x050100
        if (size<constRetinaScaleMaxSize) {
            size*=devicePixelRatio;
        }
        #endif
        covers.append(LoadedCover(s.song, loadScaledCover(s.song, size)));
    }
    if (!covers.isEmpty()) {
        DBUG << "loaded" << covers.count();
        emit loaded(covers);
    }

    if (!queue.isEmpty()) {
        startTimer(0);
    }
}

Covers::Covers()
    : downloader(0)
    , locator(0)
    , loader(0)
{
    #if QT_VERSION >= 0x050100
    if (Settings::self()->retinaSupport()) {
        devicePixelRatio=qApp->devicePixelRatio();
    }
    #endif
    #ifdef ENABLE_UBUNTU
    cache.setMaxCost(20*1024*1024); // Not used?
    cacheScaledCovers=false;
    saveInMpdDir=false;
    #else
    maxCoverUpdatePerIteration=Settings::self()->maxCoverUpdatePerIteration();
    cache.setMaxCost(Settings::self()->coverCacheSize()*1024*1024);
    #endif
}

#ifdef ENABLE_UBUNTU
void Covers::setFetchCovers(bool f)
{
    fetchCovers=f;
}
#else
void Covers::readConfig()
{
    saveInMpdDir=Settings::self()->storeCoversInMpdDir();
    cacheScaledCovers=Settings::self()->cacheScaledCovers();
    fetchCovers=Settings::self()->fetchCovers();
}
#endif

void Covers::stop()
{
    if (downloader) {
        disconnect(downloader, SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImageDownloaded(Song,QImage,QString)));
        disconnect(downloader, SIGNAL(composerImage(Song,QImage,QString)), this, SLOT(composerImageDownloaded(Song,QImage,QString)));
        disconnect(downloader, SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverDownloaded(Song,QImage,QString)));
        downloader->stop();
        downloader=0;
    }
    if (locator) {
        disconnect(locator, SIGNAL(located(QList<LocatedCover>)), this, SLOT(located(QList<LocatedCover>)));
        locator->stop();
        locator=0;
    }
    if (loader) {
        disconnect(loader, SIGNAL(loaded(QList<LoadedCover>)), this, SLOT(loaded(QList<LoadedCover>)));
        loader->stop();
        loader=0;
    }
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    cleanCdda();
    #endif
}

static inline Song setSizeRequest(Song s, int size) { s.setSpecificSizeRequest(size); return s; }

void Covers::clearNameCache()
{
    mutex.lock();
    filenames.clear();
    mutex.unlock();
}

void Covers::clearScaleCache()
{
    cache.clear();
}

QPixmap * Covers::getScaledCover(const Song &song, int size)
{
    if (size<4) {
        return 0;
    }
//    DBUG_CLASS("Covers") << song.albumArtist() << song.album << song.mbAlbumId << size;
    QString key=cacheKey(song, size);
    QPixmap *pix(cache.object(key));
    if (!pix && cacheScaledCovers) {
        QImage img=loadScaledCover(song, size);
        if (!img.isNull()) {
            pix=new QPixmap(QPixmap::fromImage(img));
        }
        if (pix) {
            cache.insert(key, pix, pix->width()*pix->height()*(pix->depth()/8));
        } else {
            // Create a dummy image so that we dont keep on stating files that do not exist!
            pix=new QPixmap(1, 1);
            cache.insert(key, pix, 1);
        }
        cacheSizes.insert(size);
    }
    return pix && pix->width()>1 ? pix : 0;
}

QPixmap * Covers::saveScaledCover(const QImage &img, const Song &song, int size)
{
    if (size<4) {
        return 0;
    }

    if (cacheScaledCovers && !isOnlineServiceImage(song)) {
        QString fileName=getScaledCoverName(song, size, true);
        bool status=img.save(fileName, "JPG");
        DBUG_CLASS("Covers") << song.albumArtist() << song.album << song.mbAlbumId() << size << fileName << status;
    }
    QPixmap *pix=new QPixmap(QPixmap::fromImage(img));
    cache.insert(cacheKey(song, size), pix, pix->width()*pix->height()*(pix->depth()/8));
    cacheSizes.insert(size);
    return pix;
}

QPixmap * Covers::defaultPix(const Song &song, int size, int origSize)
{
    #if QT_VERSION < 0x050100
    Q_UNUSED(origSize)
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    bool podcast=!song.isArtistImageRequest() && !song.isComposerImageRequest() && song.isFromOnlineService() && PodcastService::constName==song.onlineService();
    QString key=song.isArtistImageRequest()
                ? QLatin1String("artist-")
                    : song.isComposerImageRequest()
                        ? QLatin1String("composer-")
                        : podcast
                            ? QLatin1String("podcast-")
                            : QLatin1String("album-");
    #else
    bool podcast=false;
    QString key=song.isArtistImageRequest() ? QLatin1String("artist-") : QLatin1String("album-");
    #endif
    key+=QString::number(size);
    QPixmap *pix=cache.object(key);
    #ifndef ENABLE_UBUNTU
    if (!pix) {
        Icon &icn=song.isArtistImageRequest()
                ? Icons::self()->artistIcon
                : song.isComposerImageRequest()
                    ? Icons::self()->composerIcon
                    : podcast
                        ? Icons::self()->podcastIcon
                        : Icons::self()->albumIcon;
        pix=new QPixmap(icn.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        #if QT_VERSION >= 0x050100
        if (size!=origSize) {
            pix->setDevicePixelRatio(devicePixelRatio);
            DBUG << "Set pixel ratio of dummy pixmap" << devicePixelRatio;
        }
        #endif
        cache.insert(key, pix, 1);
        cacheSizes.insert(size);
    }
    #endif
    return pix;
}

QPixmap * Covers::get(const Song &song, int size, bool urgent)
{
//    DBUG_CLASS("Covers") << song.albumArtist() << song.album << song.mbAlbumId() << song.composer() << song.isArtistImageRequest() << song.isComposerImageRequest() << size << urgent;
    QString key;
    QPixmap *pix=0;
    if (0==size) {
        size=22;
    }

    int origSize=size;
    #if QT_VERSION >= 0x050100
    if (size<constRetinaScaleMaxSize) {
        size*=devicePixelRatio;
    }
    #endif
    if (!song.isUnknown() || song.isStandardStream()) {
        key=cacheKey(song, size);
        pix=cache.object(key);

        #ifndef ENABLE_UBUNTU
        if (!pix) {
            if (song.isArtistImageRequest() && song.isVariousArtists()) {
                // Load artist image...
                pix=new QPixmap(Icons::self()->artistIcon.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else if (Song::SingleTracks==song.type) {
                pix=new QPixmap(Icons::self()->albumIcon.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else if (song.isStandardStream()) {
                pix=new QPixmap(Icons::self()->streamIcon.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            }
            #ifdef ENABLE_ONLINE_SERVICES
            else if (isOnlineServiceImage(song)) {
                Covers::Image img=serviceImage(song);
                if (!img.img.isNull()) {
                    pix=new QPixmap(QPixmap::fromImage(img.img.scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                }
            }
            #endif
            if (pix) {
                #if QT_VERSION >= 0x050100
                if (size!=origSize) {
                    pix->setDevicePixelRatio(devicePixelRatio);
                    DBUG << "Set pixel ratio of cover" << devicePixelRatio;
                }
                #endif
                cache.insert(key, pix, 1);
                cacheSizes.insert(size);
            }
        }
        #endif
        if (!pix) {
            if (urgent) {
                QImage cached=loadScaledCover(song, size);
                if (cached.isNull()) {
                    Image img=findImage(song, false);
                    if (!img.img.isNull()) {
                        pix=saveScaledCover(scale(song, img.img, size), song, size);
                        #if QT_VERSION >= 0x050100
                        if (size!=origSize) {
                            pix->setDevicePixelRatio(devicePixelRatio);
                            DBUG << "Set pixel ratio of saved scaled cover" << devicePixelRatio;
                        }
                        #endif
                        return pix;
                    } else if (constNoCover==img.fileName) {
                        return defaultPix(song, size, origSize);
                    }
                } else {
                    pix=new QPixmap(QPixmap::fromImage(cached));
                    #if QT_VERSION >= 0x050100
                    if (size!=origSize) {
                        pix->setDevicePixelRatio(devicePixelRatio);
                        DBUG << "Set pixel ratio of loaded scaled cover" << devicePixelRatio;
                    }
                    #endif
                    cache.insert(key, pix);
                    cacheSizes.insert(size);
                    return pix;
                }
            }
            if (cacheScaledCovers) {
                tryToLoad(setSizeRequest(song, origSize));
            } else {
                tryToLocate(setSizeRequest(song, origSize));
            }

            // Create a dummy image so that we dont keep on locating/loading/downloading files that do not exist!
            pix=new QPixmap(1, 1);
            #if QT_VERSION >= 0x050100
            if (size!=origSize) {
                pix->setDevicePixelRatio(devicePixelRatio);
                DBUG << "Set pixel ratio of dummy cover" << devicePixelRatio;
            }
            #endif
            cache.insert(key, pix, 1);
            cacheSizes.insert(size);
        }

        if (pix && pix->width()>1) {
            return pix;
        }
    }
    return defaultPix(song, size, origSize);
}

void Covers::coverDownloaded(const Song &song, const QImage &img, const QString &file)
{
    gotAlbumCover(song, img, file);
}

void Covers::artistImageDownloaded(const Song &song, const QImage &img, const QString &file)
{
    gotArtistImage(song, img, file);
}

void Covers::composerImageDownloaded(const Song &song, const QImage &img, const QString &file)
{
    gotComposerImage(song, img, file);
}

bool Covers::updateCache(const Song &song, const QImage &img, bool dummyEntriesOnly)
{
    // Only remove all scaled entries from disk if the cover has been set by the CoverDialog
    // This is the only case where dummyEntriesOnly==false
    // dummyEntriesOnly => entries in cache that have a 'dummy' pixmap
    if (!dummyEntriesOnly) {
        clearScaledCache(song);
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    bool emitLoaded=!song.isFromDevice();
    #else
    bool emitLoaded=true;
    #endif
    bool updated=false;

    foreach (int s, cacheSizes) {
        QString key=cacheKey(song, s);
        QPixmap *pix(cache.object(key));

        if (pix && (!dummyEntriesOnly || pix->width()<2)) {
            #if QT_VERSION >= 0x050100
            double pixRatio=pix->devicePixelRatio();
            #endif
            cache.remove(key);
            if (!img.isNull()) {
                DBUG_CLASS("Covers");
                QPixmap *p=saveScaledCover(scale(song, img, s), song, s);
                #if QT_VERSION >= 0x050100
                if (p) {
                    p->setDevicePixelRatio(pixRatio);
                    DBUG << "Set pixel ratio of updated cached pixmap" << devicePixelRatio;
                }
                #endif
                if (p && emitLoaded) {
                    #if QT_VERSION >= 0x050100
                    if (pixRatio>1.00001) {
                        s/=pixRatio;
                    }
                    #endif
                    emit loaded(song, s);
                    updated=true;
                }
            }
        }
    }

    return updated;
}

void Covers::tryToLocate(const Song &song)
{
    if (!locator) {
        qRegisterMetaType<LocatedCover>("LocatedCover");
        qRegisterMetaType<QList<LocatedCover> >("QList<LocatedCover>");
        locator=new CoverLocator();
        connect(locator, SIGNAL(located(QList<LocatedCover>)), this, SLOT(located(QList<LocatedCover>)), Qt::QueuedConnection);
        connect(this, SIGNAL(locate(Song)), locator, SLOT(locate(Song)), Qt::QueuedConnection);
    }
    emit locate(song);
}

void Covers::tryToDownload(const Song &song)
{
    #ifdef ENABLE_DEVICES_SUPPORT
    if (song.isFromDevice()) {
        return;
    }
    #endif
    if (!downloader) {
        downloader=new CoverDownloader();
        connect(this, SIGNAL(download(Song)), downloader, SLOT(download(Song)), Qt::QueuedConnection);
        connect(downloader, SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImageDownloaded(Song,QImage,QString)), Qt::QueuedConnection);
        connect(downloader, SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverDownloaded(Song,QImage,QString)), Qt::QueuedConnection);
    }
    emit download(song);
}

void Covers::tryToLoad(const Song &song)
{
    if (!loader) {
        qRegisterMetaType<LoadedCover>("LoadedCover");
        qRegisterMetaType<QList<LoadedCover> >("QList<LoadedCover>");
        loader=new CoverLoader();
        connect(loader, SIGNAL(loaded(QList<LoadedCover>)), this, SLOT(loaded(QList<LoadedCover>)), Qt::QueuedConnection);
        connect(this, SIGNAL(load(Song)), loader, SLOT(load(Song)), Qt::QueuedConnection);
    }
    emit load(song);
}

Covers::Image Covers::findImage(const Song &song, bool emitResult)
{
    Covers::Image i=locateImage(song);
    if (!i.img.isNull()) {
        if (song.isArtistImageRequest()) {
            gotArtistImage(song, i.img, i.fileName, emitResult);
        } else if (song.isComposerImageRequest()) {
            gotComposerImage(song, i.img, i.fileName, emitResult);
        } else {
            gotAlbumCover(song, i.img, i.fileName, emitResult);
        }
    }
    return i;
}

Covers::Image Covers::locateImage(const Song &song)
{
    DBUG_CLASS("Covers") << song.file << song.artist << song.albumartist << song.album << song.type;
    #ifdef ENABLE_ONLINE_SERVICES
    if (song.isFromOnlineService()) {
        QString id=song.onlineService();
        Covers::Image img;

        if (isOnlineServiceImage(song)) {
            img=serviceImage(song);
            if (!img.img.isNull()) {
                DBUG_CLASS("Covers") <<  "Got cover online image" << QString(img.fileName) << "for" << id;
                return img;
            }
        }

        img.fileName=song.extraField(Song::OnlineImageCacheName);
        if (img.fileName.isEmpty()) {
            img.fileName=Utils::cacheDir(id.toLower(), true)+Covers::encodeName(song.album.isEmpty() ? song.artist : (song.artist+" - "+song.album))+".jpg";
            if (!QFile::exists(img.fileName)) {
                img.fileName=Utils::changeExtension(img.fileName, ".png");
            }
        }
        if (!img.fileName.isEmpty()) {
            img.img=loadImage(img.fileName);
            if (!img.img.isNull()) {
                DBUG_CLASS("Covers") <<  "Got cover online image" << QString(img.fileName) << "for" << id;
                return img;
           }
        }
        DBUG_CLASS("Covers") << "Failed to locate online image for" << id;
        return Image();
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (song.isFromDevice()) {
        Device *dev=DevicesModel::self()->device(song.deviceId());
        Image img;
        if (dev) {
            img=dev->requestCover(song);
        }
        if (img.img.isNull()) {
            DBUG_CLASS("Covers") << "Failed to locate device image for" << song.deviceId();
        }
        return img;
    }
    #endif

    QString prevFileName=Covers::self()->getFilename(song);
    if (!prevFileName.isEmpty()) {
        if (constNoCover==prevFileName) {
            DBUG_CLASS("Covers") << "No cover";
            return Image(QImage(), constNoCover);
        }
        #ifdef TAGLIB_FOUND
        QImage img;
        if (prevFileName.startsWith(constCoverInTagPrefix)) {
            img=Tags::readImage(prevFileName.mid(constCoverInTagPrefix.length()));
        } else {
            img=loadImage(prevFileName);
        }
        #else
        QImage img(prevFileName);
        #endif
        if (!img.isNull()) {
            DBUG_CLASS("Covers") << "Found previous" << prevFileName;
            return Image(img, prevFileName);
        }
    }

    QString songFile=song.filePath();
    QString dirName;
    bool haveAbsPath=songFile.startsWith(Utils::constDirSep);

    if (song.isCantataStream()) {
        #if QT_VERSION < 0x050000
        QUrl u(songFile);
        #else
        QUrl url(songFile);
        QUrlQuery u(url);
        #endif
        songFile=u.hasQueryItem("file") ? u.queryItemValue("file") : QString();
    }

    QStringList coverFileNames;
    if (song.isArtistImageRequest()) {
        QString artistFile=artistFileName(song);
        QString basicArtist=song.basicArtist();
        coverFileNames=QStringList() << basicArtist+".jpg" << basicArtist+".png" << artistFile+".jpg" << artistFile+".png";
    } else if (song.isComposerImageRequest()) {
        QString composerFile=composerFileName(song);
        coverFileNames=QStringList() << composerFile+".jpg" << composerFile+".png";
    } else {
        QString mpdCover=albumFileName(song);
        if (!mpdCover.isEmpty()) {
            for (int e=0; constExtensions[e]; ++e) {
                coverFileNames << mpdCover+constExtensions[e];
            }
        }
        foreach (const QString &std, standardNames()) {
            if (!coverFileNames.contains(std)) {
                coverFileNames << std;
            }
        }
        for (int e=0; constExtensions[e]; ++e) {
            coverFileNames+=song.albumArtist()+QLatin1String(" - ")+song.album+constExtensions[e];
        }
        for (int e=0; constExtensions[e]; ++e) {
            coverFileNames+=song.album+constExtensions[e];
        }
    }

    if (!songFile.isEmpty() && !songFile.startsWith(("http:/")) &&
        (haveAbsPath || (!MPDConnection::self()->getDetails().dir.isEmpty() && !MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http://")) ) ) ) {
        dirName=songFile.endsWith(Utils::constDirSep) ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+songFile
                                       : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+songFile);


        if (song.isArtistImageRequest() || song.isComposerImageRequest()) {
            for (int level=0; level<2; ++level) {
                foreach (const QString &fileName, coverFileNames) {
                    DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
                    if (QFile::exists(dirName+fileName)) {
                        QImage img=loadImage(dirName+fileName);
                        if (!img.isNull()) {
                            DBUG_CLASS("Covers") << "Got artist/composer image" << QString(dirName+fileName);
                            return Image(img, dirName+fileName);
                        }
                    }
                }
                QDir d(dirName);
                d.cdUp();
                dirName=Utils::fixPath(d.absolutePath());
            }

            // For various artists tracks, or for non-MPD files, see if we have a matching image in MPD.
            // e.g. artist=Wibble, look for $mpdDir/Wibble/artist.png
            if (!song.isComposerImageRequest() && (song.isVariousArtists() || song.isNonMPD())) {
                QString basicArtist=song.basicArtist();
                dirName=MPDConnection::self()->getDetails().dirReadable ? MPDConnection::self()->getDetails().dir : QString();
                if (!dirName.isEmpty() && !dirName.startsWith(QLatin1String("http:/"))) {
                    dirName+=basicArtist+Utils::constDirSep;
                    foreach (const QString &fileName, coverFileNames) {
                        DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
                        if (QFile::exists(dirName+fileName)) {
                            QImage img=loadImage(dirName+fileName);
                            if (!img.isNull()) {
                                DBUG_CLASS("Covers") << "Got artist image" << QString(dirName+fileName);
                                return Image(img, dirName+fileName);
                            }
                        }
                    }
                }
            }
        } else {
            foreach (const QString &fileName, coverFileNames) {
                DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
                if (QFile::exists(dirName+fileName)) {
                    QImage img=loadImage(dirName+fileName);
                    if (!img.isNull()) {
                        DBUG_CLASS("Covers") << "Got cover image" << QString(dirName+fileName);
                        return Image(img, dirName+fileName);
                    }
                }
            }

            #ifdef TAGLIB_FOUND
            QString fileName=haveAbsPath ? songFile : (MPDConnection::self()->getDetails().dir+songFile);
            DBUG_CLASS("Covers") << "Checking file" << fileName;
            if (QFile::exists(fileName)) {
                QImage img(Tags::readImage(fileName));
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cover image from tag" << fileName;
                    return Image(img, constCoverInTagPrefix+fileName);
                }
            }
            #endif

            QStringList files=QDir(dirName).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable);
            foreach (const QString &fileName, files) {
                DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
                QImage img=loadImage(dirName+fileName);
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cover image" << QString(dirName+fileName);
                    return Image(img, dirName+fileName);
                }
            }
        }
    }


    if (song.isArtistImageRequest() || song.isComposerImageRequest()) {
        QString artistOrComposer=encodeName(song.isComposerImageRequest() ? song.composer() : song.albumArtist());

        // For non-MPD tracks, see if we actually have a saved MPD cover...
        if (MPDConnection::self()->getDetails().dirReadable) {
            QString songDir=artistOrComposer+Utils::constDirSep;
            if (!song.file.startsWith(songDir)) {
                QString dirName=MPDConnection::self()->getDetails().dir+songDir;
                if (QDir(dirName).exists()) {
                    foreach (const QString &fileName, coverFileNames) {
                        DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
                        if (QFile::exists(dirName+fileName)) {
                            QImage img=loadImage(dirName+fileName);
                            if (!img.isNull()) {
                                DBUG_CLASS("Covers") << "Got artist/composer image" << QString(dirName+fileName);
                                return Image(img, dirName+fileName);
                            }
                        }
                    }
                }
            }
        }
        // Check if cover is already cached
        QString dir(Utils::cacheDir(constCoverDir, false));
        for (int e=0; constExtensions[e]; ++e) {
            DBUG_CLASS("Covers") << "Checking cache file" << QString(dir+artistOrComposer+constExtensions[e]);
            if (QFile::exists(dir+artistOrComposer+constExtensions[e])) {
                QImage img=loadImage(dir+artistOrComposer+constExtensions[e]);
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cached artist/composer image" << QString(dir+artistOrComposer+constExtensions[e]);
                    return Image(img, dir+artistOrComposer+constExtensions[e]);
                }
            }
        }
    } else {
        QString artist=encodeName(song.albumArtist());
        QString album=encodeName(song.album);
        // For non-MPD tracks, see if we actually have a saved MPD cover...
        if (MPDConnection::self()->getDetails().dirReadable) {
            QString songDir=artist+Utils::constDirSep+album+Utils::constDirSep;
            if (!song.file.startsWith(songDir)) {
                QString dirName=MPDConnection::self()->getDetails().dir+songDir;
                if (QDir(dirName).exists()) {
                    foreach (const QString &fileName, coverFileNames) {
                        DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
                        if (QFile::exists(dirName+fileName)) {
                            QImage img=loadImage(dirName+fileName);
                            if (!img.isNull()) {
                                DBUG_CLASS("Covers") << "Got cover image" << QString(dirName+fileName);
                                return Image(img, dirName+fileName);
                            }
                        }
                    }
                }
            }
        }
        // Check if cover is already cached
        QString dir(Utils::cacheDir(constCoverDir+artist, false));
        for (int e=0; constExtensions[e]; ++e) {
            DBUG_CLASS("Covers") << "Checking cache file" << QString(dir+album+constExtensions[e]);
            if (QFile::exists(dir+album+constExtensions[e])) {
                QImage img=loadImage(dir+album+constExtensions[e]);
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cached cover image" << QString(dir+album+constExtensions[e]);
                    return Image(img, dir+album+constExtensions[e]);
                }
            }
        }
    }

    DBUG_CLASS("Covers") << "Failed to locate image";
    return Image();
}

// Dont return song files as cover files!
static Covers::Image fix(const Covers::Image &img)
{
    return Covers::Image(img.img, img.validFileName() ? img.fileName : QString());
}

bool Covers::Image::validFileName() const
{
    return !fileName.isEmpty() && !fileName.startsWith(constCoverInTagPrefix) && constNoCover!=fileName;
}

Covers::Image Covers::requestImage(const Song &song, bool urgent)
{
    DBUG << song.file << song.artist << song.albumartist << song.album << song.composer() << song.isArtistImageRequest() << song.isComposerImageRequest();

    #ifdef ENABLE_ONLINE_SERVICES
    if (urgent && song.isFromOnlineService()) {
        Covers::Image img=serviceImage(song);
        if (!img.img.isNull()) {
            return img;
        }
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (song.isFromDevice()) {
        Device *dev=DevicesModel::self()->device(song.deviceId());
        if (dev) {
            return dev->requestCover(song);
        }
        return Image();
    }
    #endif

    QString key=songKey(song);
    if (currentImageRequests.contains(key)) {
        return Image();
    }

    if (!urgent) {
        currentImageRequests.insert(key);
        tryToLocate(song);
        return Image();
    }

    Image img=findImage(song, false);
    if (img.img.isNull() && Song::OnlineSvrTrack!=song.type && constNoCover!=img.fileName) {
        DBUG << song.file << song.artist << song.albumartist << song.album << "Need to download";
        currentImageRequests.insert(key);
        tryToDownload(song);
    }

    return fix(img);
}

void Covers::located(const QList<LocatedCover> &covers)
{
    foreach (const LocatedCover &cvr, covers) {
        if (!cvr.img.isNull()) {
            if (cvr.song.isArtistImageRequest()) {
                gotArtistImage(cvr.song, cvr.img, cvr.fileName);
            } else if (cvr.song.isComposerImageRequest()) {
                gotComposerImage(cvr.song, cvr.img, cvr.fileName);
            } else {
                gotAlbumCover(cvr.song, cvr.img, cvr.fileName);
            }
        } else { // Failed to locate a cover, so try to download one...
            tryToDownload(cvr.song);
        }
    }
}

void Covers::loaded(const QList<LoadedCover> &covers)
{
    foreach (const LoadedCover &cvr, covers) {
        if (!cvr.img.isNull()) {
            int size=cvr.song.size;
            #if QT_VERSION >= 0x050100
            int origSize=size;
            if (size<constRetinaScaleMaxSize) {
                size*=devicePixelRatio;
            }
            #endif
            QPixmap *pix=new QPixmap(QPixmap::fromImage(cvr.img));
            #if QT_VERSION >= 0x050100
            if (size!=origSize) {
                pix->setDevicePixelRatio(devicePixelRatio);
                DBUG << "Set pixel ratio of loaded pixmap" << devicePixelRatio;
            }
            #endif
            cache.insert(cacheKey(cvr.song, size), pix);
            cacheSizes.insert(size);
            emit loaded(cvr.song, cvr.song.size);
        } else { // Failed to load a scaled cover, try locating non-scaled cover...
            tryToLocate(cvr.song);
        }
    }
}

void Covers::updateCover(const Song &song, const QImage &img, const QString &file)
{
    updateCache(song, img, false);
    if (!file.isEmpty()) {
        filenames[songKey(song)]=file;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!song.isFromDevice())
    #endif
    emit coverUpdated(song, img, file);
}

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
void Covers::cleanCdda()
{
    QString dir = Utils::cacheDir(Covers::constCddaCoverDir, false);
    if (!dir.isEmpty()) {
        QDir d(dir);
        QStringList entries=d.entryList(QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot);

        foreach (const QString &f, entries) {
            if (f.endsWith(".jpg") || f.endsWith(".png")) {
                QFile::remove(dir+f);
            }
        }
        d.cdUp();
        d.rmdir(dir);
    }
}
#endif

void Covers::gotAlbumCover(const Song &song, const QImage &img, const QString &fileName, bool emitResult)
{
    QString key=albumKey(song);
    currentImageRequests.remove(key);
//    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/")) {
        mutex.lock();
        filenames.insert(key, fileName.isEmpty() ? constNoCover : fileName);
        mutex.unlock();
//    }
    if (emitResult) {
        bool updatedCover=false;
        if (!img.isNull()) {
            updatedCover=updateCache(song, img, true);
        }
        if (updatedCover || !song.isSpecificSizeRequest()) {
            DBUG << "emit cover" << song.file << song.artist << song.albumartist << song.album << song.mbAlbumId() << img.width() << img.height() << fileName;
            emit cover(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
        }
    }
}

void Covers::gotArtistImage(const Song &song, const QImage &img, const QString &fileName, bool emitResult)
{
    QString key=artistKey(song);
    currentImageRequests.remove(key);
//    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/")) {
        mutex.lock();
        filenames.insert(key, fileName.isEmpty() ? constNoCover : fileName);
        mutex.unlock();
//    }
    if (emitResult) {
        if (!img.isNull()) {
            updateCache(song, img, true);
        }
        if (!song.isSpecificSizeRequest()) {
            DBUG << "emit artistImage" << song.file << song.artist << song.albumartist << song.album << img.width() << img.height() << fileName;
            emit artistImage(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
        }
    }
}

void Covers::gotComposerImage(const Song &song, const QImage &img, const QString &fileName, bool emitResult)
{
    QString key=composerKey(song);
    currentImageRequests.remove(key);
//    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/")) {
        mutex.lock();
        filenames.insert(key, fileName.isEmpty() ? constNoCover : fileName);
        mutex.unlock();
//    }
    if (emitResult) {
        if (!img.isNull()) {
            updateCache(song, img, true);
        }
        if (!song.isSpecificSizeRequest()) {
            DBUG << "emit composerImage" << song.file << song.artist << song.albumartist << song.album << song.composer() << img.width() << img.height() << fileName;
            emit composerImage(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
        }
    }
}

QString Covers::getFilename(const Song &s)
{
    mutex.lock();
    QMap<QString, QString>::ConstIterator fileIt=filenames.find(songKey(s));
    QString f=fileIt==filenames.end() ? QString() : fileIt.value();
    mutex.unlock();
    return f;
}
