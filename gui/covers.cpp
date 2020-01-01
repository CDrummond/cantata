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

#include "covers.h"
#include "mpd-interface/song.h"
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include "network/networkaccessmanager.h"
#include "settings.h"
#include "config.h"
#include "apikeys.h"
#include "devices/deviceoptions.h"
#include "support/thread.h"
#include "online/onlineservice.h"
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
#include <QUrlQuery>
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
#include <QJsonParseError>
#include <QJsonDocument>
#include <QDesktopWidget>

GLOBAL_STATIC(Covers, instance)

#include <QDebug>
static int debugLevel=0;
#define DBUG_CLASS(CLASS) if (debugLevel) qWarning() << CLASS << QThread::currentThread()->objectName() << __FUNCTION__
#define DBUG DBUG_CLASS(metaObject()->className())
#define VERBOSE_DBUG_CLASS(CLASS) if (debugLevel>1) qWarning() << CLASS << QThread::currentThread()->objectName() << __FUNCTION__
#define VERBOSE_DBUG VERBOSE_DBUG_CLASS(metaObject()->className())
void Covers::enableDebug(bool verbose)
{
    debugLevel=verbose ? 2 : 1;
}

bool Covers::debugEnabled()
{
    return debugLevel>0;
}

bool Covers::verboseDebugEnabled()
{
    return debugLevel>1;
}

const QLatin1String Covers::constCoverDir("covers/");
const QLatin1String Covers::constScaledCoverDir("covers-scaled/");
const QLatin1String Covers::constCddaCoverDir("cdda/");
const QLatin1String Covers::constFileName("cover");
const QLatin1String Covers::constArtistImage("artist");
const QLatin1String Covers::constComposerImage("composer");
const QString Covers::constCoverInTagPrefix=QLatin1String("{tag}");

static const char * constExtensions[]={".jpg", ".png", nullptr};
static bool saveInMpdDir=true;
static bool fetchCovers=true;
static QString constNoCover=QLatin1String("{nocover}");

static double devicePixelRatio=1.0;
// Only scale images to device pixel ratio if un-scaled size is less then 300pixels.
static const int constRetinaScaleMaxSize=300;

#ifdef USE_JPEG_FOR_SCALED_CACHE
static const QLatin1String constScaledExtension(".jpg");
static const QLatin1String constScaledPrevExtension(".png");
static const char * constScaledFormat="JPG";
#else
static const QLatin1String constScaledExtension(".png");
static const QLatin1String constScaledPrevExtension(".jpg");
static const char * constScaledFormat="PNG";
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
    return !dir.isEmpty() && !mpdDir.isEmpty() && !mpdDir.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) &&
           !mpdDir.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)  && QDir(mpdDir).exists() && dir.startsWith(mpdDir);
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

static const QString typeFromFilename(const QString &fileName)
{
    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray header=f.read(10);
        f.reset();
        return typeFromRaw(header).toLatin1();
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
    return img;
}

static inline bool isOnlineServiceImage(const Song &s)
{
    return OnlineService::showLogoAsCover(s);
}

static Covers::Image serviceImage(const Song &s)
{
    Covers::Image img;

    if (s.hasExtraField(Song::OnlineImageCacheName)) {
        img.fileName=s.extraField(Song::OnlineImageCacheName);
        if (!img.fileName.isEmpty()) {
            img.img=loadImage(img.fileName);
            if (!img.img.isNull()) {
                DBUG_CLASS("Covers") << s.onlineService() << img.fileName;
                return img;
            }
        }
    }
    img.fileName=OnlineService::iconPath(s.onlineService());
    if (!img.fileName.isEmpty()) {
        img.img=loadImage(img.fileName);
        if (!img.img.isNull()) {
            DBUG_CLASS("Covers") << s.onlineService();
        }
    }
    return img;
}

static inline QString albumKey(const Song &s)
{
    if (Song::SingleTracks==s.type) {
        return QLatin1String("-single-tracks-");
    }
    if (s.isStandardStream()) {
        return QLatin1String("-stream-");
    }
    if (isOnlineServiceImage(s)) {
        return s.onlineService();
    }
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
    } else if (isOnlineServiceImage(song)) {
        return song.onlineService()+QString::number(size);
    }

    return songKey(song)+QString::number(size);
}

static QString getScaledCoverName(const Song &song, int size, bool createDir)
{
    if (song.isArtistImageRequest()) {
        QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/'), createDir);
        return dir.isEmpty() ? QString() : (dir+Covers::encodeName(song.albumArtist())+constScaledExtension);
    }

    if (song.isComposerImageRequest()) {
        QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/'), createDir);
        return dir.isEmpty() ? QString() : (dir+Covers::encodeName(song.composer())+constScaledExtension);
    }

    QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/')+Covers::encodeName(song.albumArtist()), createDir);
    return dir.isEmpty() ? QString() : (dir+Covers::encodeName(song.albumId())+constScaledExtension);
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
            for (const QString &sizeDirName: sizeDirNames) {
                QString fname=dirName+sizeDirName+QLatin1Char('/')+fileName;
                if (QFile::exists(fname)) {
                    QFile::remove(fname);
                }
            }
        }
    } else {
        QString subDir=Covers::encodeName(song.albumArtist());
        for (int i=0; constExtensions[i]; ++i) {
            QString fileName=Covers::encodeName(song.album)+constExtensions[i];
            for (const QString &sizeDirName: sizeDirNames) {
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
            QImage img(fileName, constScaledFormat);
            if (!img.isNull() && (img.width()==size || img.height()==size)) {
                DBUG_CLASS("Covers") << song.albumArtist() << song.albumId() << size << "scaled cover found" << fileName;
                return img;
            }
        } else { // Remove any previous PNG/JPEG scaled cover...
            fileName=Utils::changeExtension(fileName, constScaledPrevExtension);
            if (QFile::exists(fileName)) {
                QFile::remove(fileName);
            }
        }
    }
    VERBOSE_DBUG_CLASS("Covers")  << song.albumArtist() << song.albumId() << size << "scaled cover NOT found";

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
    return isJpg(data) ? "JPG" : (isPng(data) ? "PNG" : nullptr);
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
    if (name.startsWith(QLatin1Char('.'))) {
        name[0]=QLatin1Char('_');
    }
    #endif
    return name;
}

static QString albumCoverName;
QString Covers::albumFileName(const Song &song)
{
    QString coverName=albumCoverName;
    if (coverName.isEmpty()) {
        coverName=Covers::constFileName;
    }
    else if (coverName.contains(QLatin1Char('%'))) {
        coverName.replace(DeviceOptions::constAlbumArtist, encodeName(song.albumArtist()));
        coverName.replace(DeviceOptions::constTrackArtist, encodeName(song.albumArtist()));
        coverName.replace(DeviceOptions::constAlbumTitle, encodeName(song.album));
        coverName.replace(QLatin1String("%"), QLatin1String(""));
    }
    return coverName;
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

        QStringList files;
        QString userDir=Utils::dataDir();

        if (!userDir.isEmpty()) {
            files.append(Utils::fixPath(userDir)+QLatin1String("tag_fixes.xml"));
        }

        files.append(":tag_fixes.xml");

        for (const auto &f: files) {
            QFile file(f);
            if (file.open(QIODevice::ReadOnly)) {
                QXmlStreamReader doc(&file);
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

bool Covers::copyImage(const QString &sourceDir, const QString &destDir, const QString &coverFile, const QString &destName, unsigned short maxSize)
{
    QImage img=loadImage(sourceDir+coverFile);
    bool ok=false;
    if (maxSize>0 && (img.width()>maxSize || img.height()>maxSize)) { // Need to scale image...
        img=img.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ok=img.save(destDir+destName);
        DBUG_CLASS("Covers") << "Rescaling image from" << QString(sourceDir+coverFile) << img.width() << "x" << img.height() << "to" << QString(destDir+destName) << maxSize << ok;
    } else if (destName.right(4)!=typeFromFilename(sourceDir+coverFile)) { // Diff extensions, so need to convert image type...
        ok=img.save(destDir+destName);
        DBUG_CLASS("Covers") << "Converting image type from" << QString(sourceDir+coverFile) << "to" << QString(destDir+destName) << ok;
    } else { // no scaling, and same image type, so we can just copy...
        ok=QFile::copy(sourceDir+coverFile, destDir+destName);
        DBUG_CLASS("Covers") << "Copying from" << QString(sourceDir+coverFile) << "to" << QString(destDir+destName) << ok;
        if (!ok) {
            DBUG_CLASS("Covers") << "Copy failed";
            QFile src(sourceDir+coverFile);
            if (src.open(QIODevice::ReadOnly)) {
                QByteArray data = src.readAll();
                DBUG_CLASS("Covers") << "Read" << data.length() << "from" << QString(sourceDir+coverFile);
                src.close();
                QFile dest(destDir+destName);
                if (dest.open(QIODevice::WriteOnly)) {
                    quint64 written = dest.write(data);
                    DBUG_CLASS("Covers") << "Wrote" << written << "to" << QString(destDir+destName);
                    dest.close();
                    ok=true;
                }
            }
        }
    }
    if (ok) {
        Utils::setFilePerms(destDir+destName);
    }
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
    DBUG_CLASS("Covers") << names;
    for (const QString &coverFile: names) {
        if (QFile::exists(destDir+coverFile)) {
            DBUG_CLASS("Covers") << coverFile << "already exists";
            return true;
        }
    }

    // No cover found, try to copy from source folder
    for (const QString &coverFile: names) {
        if (QFile::exists(sourceDir+coverFile)) {
            QString destName(name);
            if (destName.isEmpty()) { // copying into mpd dir, so we want cover.jpg/png...
                if (standardNames().at(0)!=coverFile) { // source is not 'cover.xxx'
                    QString ext(coverFile.endsWith(constExtensions[0]) ? constExtensions[0] : constExtensions[1]);
                    destName=mpdCover+ext;
                } else {
                    destName=coverFile;
                }
            }
            DBUG_CLASS("Covers") << "Found cover to copy" << sourceDir << coverFile << "copying as" << destName;
            copyImage(sourceDir, destDir, coverFile, destName, maxSize);
            return true;
        }
    }

    DBUG_CLASS("Covers") << "Try to locate any image in source folder";
    QStringList files=QDir(sourceDir).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable);
    for (const QString &coverFile: files) {
        QString destName(name);
        if (destName.isEmpty()) { // copying into mpd dir, so we want cover.jpg/png...
            if (standardNames().at(0)!=coverFile) { // source is not 'cover.xxx'
                QString ext(coverFile.endsWith(constExtensions[0]) ? constExtensions[0] : constExtensions[1]);
                destName=mpdCover+ext;
            } else {
                destName=coverFile;
            }
        }
        DBUG_CLASS("Covers") << "Found cover to copy" << sourceDir << coverFile << "copying as" << destName;
        copyImage(sourceDir, destDir, coverFile, destName, maxSize);
        return true;
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
                DBUG_CLASS("Covers") << "Copying cover from cache" << QString(album+constExtensions[e]) << "copying as" << destName;
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
        fileNames << Covers::constFileName << QLatin1String("albumart") << QLatin1String("folder");
        for (const QString &fileName: fileNames) {
            for (int e=0; constExtensions[e]; ++e) {
                *coverFileNames << fileName+constExtensions[e];
            }
        }
    }

    return *coverFileNames;
}

CoverDownloader::CoverDownloader()
    : manager(nullptr)
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
    connect(this, SIGNAL(mpdCover(Song)), MPDConnection::self(), SLOT(getCover(Song)));
    connect(MPDConnection::self(), SIGNAL(albumArt(Song,QByteArray)), this, SLOT(mpdAlbumArt(Song,QByteArray)));
}

void CoverDownloader::stop()
{
    thread->stop();
}

void CoverDownloader::download(const Song &song)
{
    DBUG << song.file << song.artist << song.albumartist << song.album;
    if (song.isFromOnlineService()) {
        QString serviceName=song.onlineService();
        QString imageUrl=song.extraField(Song::OnlineImageUrl);

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

    if (!song.isArtistImageRequest() && !song.isComposerImageRequest() && MPDConnection::self()->supportsCoverDownload()) {
        downloadViaMpd(job);
    } else if (!MPDConnection::self()->getDetails().dir.isEmpty() &&
               (MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
                MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))) {
        downloadViaHttp(job, JobHttpJpg);
    } else if (fetchCovers || job.song.isArtistImageRequest()) {
        downloadViaRemote(job);
    } else {
        failed(job);
    }
}

void CoverDownloader::downloadViaMpd(Job &job)
{
    job.type=JobMpd;
    emit mpdCover(job.song);
    mpdJobs.insert(job.song.file, job);
}

bool CoverDownloader::downloadViaHttp(Job &job, JobType type)
{
    QUrl u;
    QString coverName=job.song.isArtistImageRequest()
                            ? Covers::constArtistImage
                            : job.song.isComposerImageRequest()
                                ? Covers::constComposerImage
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
    u=QUrl(MPDConnection::self()->getDetails().dir+dir+coverName.toLatin1());

    job.type=type;
    NetworkJob *j=network()->get(u);
    connect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
    jobs.insert(j, job);
    DBUG << u.toString();
    return true;
}

void CoverDownloader::downloadViaRemote(Job &job)
{
    QUrl url;
    if (job.song.isArtistImageRequest()) {
        url=QUrl("https://ws.audioscrobbler.com/2.0/");
        QUrlQuery query;

        query.addQueryItem("method", "artist.getinfo");
        ApiKeys::self()->addKey(query, ApiKeys::LastFm);
        query.addQueryItem("autocorrect", "1");
        query.addQueryItem("artist", Covers::fixArtist(job.song.albumArtist()));
        url.setQuery(query);

        NetworkJob *j = network()->get(url);
        connect(j, SIGNAL(finished()), this, SLOT(lastFmArtistCallFinished()));
        job.type=JobRemote;
        jobs.insert(j, job);
        DBUG << url.toString();
    } else {
        url=QUrl("http://itunes.apple.com/search");
        QUrlQuery query;
        query.addQueryItem("term", Covers::fixArtist(job.song.albumArtist())+ " " + job.song.album);
        query.addQueryItem("limit", QString::number(10));
        query.addQueryItem("media", "music");
        query.addQueryItem("entity", "album");
        url.setQuery(query);
        NetworkJob *j = network()->get(url);
        connect(j, SIGNAL(finished()), this, SLOT(remoteCallFinished()));
        job.type=JobRemote;
        jobs.insert(j, job);
        DBUG << url.toString();
    }
}

void CoverDownloader::mpdAlbumArt(const Song &song, const QByteArray &data)
{
    QHash<QString, Job>::Iterator it = mpdJobs.find(song.file);
    if (it!=mpdJobs.end()) {
        Covers::Image img;
        img.img= data.isEmpty() ? QImage() : QImage::fromData(data, Covers::imageFormat(data));
        Job job=it.value();

        if (!img.img.isNull() && img.img.size().width()<32) {
            img.img = QImage();
        }

        mpdJobs.remove(it.key());
        if (img.img.isNull()) {
            downloadViaHttp(job, JobHttpJpg);
        } else {
            if (!img.img.isNull()) {
                if (img.img.size().width()>Covers::constMaxSize.width() || img.img.size().height()>Covers::constMaxSize.height()) {
                    img.img=img.img.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                img.fileName=saveImg(job, img.img, data);
                if (!img.fileName.isEmpty()) {
                    clearScaledCache(job.song);
                }
            }

            DBUG << "got cover image" << img.fileName;
            emit cover(job.song, img.img, img.fileName);
        }
    } else {
       DBUG << "missing job for " << song.file;
    }
}

void CoverDownloader::remoteCallFinished()
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

        if (reply->ok()) {
            if (job.song.isArtistImageRequest()) {
                QJsonParseError jsonParseError;
                QVariantMap parsed=QJsonDocument::fromJson(reply->readAll(), &jsonParseError).toVariant().toMap();
                bool ok=QJsonParseError::NoError==jsonParseError.error;

                if (ok && !parsed.isEmpty()) {
                    QVariantList artistbackgrounds;

                    if (parsed.contains("artistthumb")) {
                        artistbackgrounds=parsed["artistthumb"].toList();
                    } else {
                        QVariantMap artist=parsed[parsed.keys().first()].toMap();

                        if (artist.contains("artistthumb")) {
                            artistbackgrounds=artist["artistthumb"].toList();
                        }
                    }

                    if (!artistbackgrounds.isEmpty()) {
                        QVariantMap artistbackground=artistbackgrounds.first().toMap();
                        if (artistbackground.contains("url")) {
                            url=artistbackground["url"].toString();
                        }
                    }
                }
            } else {
                QJsonParseError jsonParseError;
                QVariantMap parsed=QJsonDocument::fromJson(reply->readAll(), &jsonParseError).toVariant().toMap();
                bool ok=QJsonParseError::NoError==jsonParseError.error;

                if (ok && parsed.contains("results")) {
                    QVariantList results=parsed["results"].toList();
                    for (const QVariant &res: results) {
                        QVariantMap item=res.toMap();
                        if (item.contains("artworkUrl100")) {
                            QString thumbUrl=item["artworkUrl100"].toString();
                            url=QString(thumbUrl).replace("100x100", "600x600");
                            break;
                        }
                    }
                }
            }
        }

        if (!url.isEmpty()) {
            NetworkJob *j=network()->get(QNetworkRequest(QUrl(url)));
            connect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
            DBUG << "download" << url;
            jobs.insert(j, job);
        } else {
            failed(job);
        }
    }
    reply->deleteLater();
}

void CoverDownloader::lastFmArtistCallFinished()
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
        QUrl url;

        if (reply->ok()) {
            bool inSection=false;
            QXmlStreamReader doc(reply->readAll());
            QStringList musicBrainzIds;

            doc.setNamespaceProcessing(false);
            while (!doc.atEnd()) {
                doc.readNext();

                if (doc.isStartElement()) {
                    if (!inSection && QLatin1String("artist")==doc.name()) {
                        inSection=true;
                    } else if (inSection && QLatin1String("mbid")==doc.name()) {
                        QString id=doc.readElementText();
                        if (id.length()>4) {
                            musicBrainzIds.append(id);
                        }
                    }
                } else if (doc.isEndElement() && inSection && QLatin1String("artist")==doc.name()) {
                    inSection=false;
                }
            }

            if (!musicBrainzIds.isEmpty()) {
                url=QUrl("http://webservice.fanart.tv/v3/music/"+musicBrainzIds.first());
                QUrlQuery query;
                ApiKeys::self()->addKey(query, ApiKeys::FanArt);
                url.setQuery(query);
            }
        }

        if (url.isValid()) {
            NetworkJob *j=network()->get(QNetworkRequest(url));
            connect(j, SIGNAL(finished()), this, SLOT(remoteCallFinished()));
            DBUG << "download" << url;
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
        if (img.img.isNull() && JobRemote!=job.type) {
            if (JobHttpJpg==job.type) {
                if (!job.level || !downloadViaHttp(job, JobHttpJpg)) {
                    job.level=0;
                    downloadViaHttp(job, JobHttpPng);
                }
            } else if ((fetchCovers || job.song.isArtistImageRequest()) && JobHttpPng==job.type && (!job.level || !downloadViaHttp(job, JobHttpPng)) && !job.song.isComposerImageRequest()) {
                downloadViaRemote(job);
            } else {
                failed(job);
            }
        } else {
            if (!img.img.isNull()) {
                if (img.img.size().width()>Covers::constMaxSize.width() || img.img.size().height()>Covers::constMaxSize.height()) {
                    img.img=img.img.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                img.fileName=saveImg(job, img.img, data);
                if (!img.fileName.isEmpty()) {
                    clearScaledCache(job.song);
                }
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
        DBUG << "Got image" << id << song.albumArtist() << song.album << png;
        if (!img.isNull()) {
            if (img.size().width()>Covers::constMaxSize.width() || img.size().height()>Covers::constMaxSize.height()) {
                img=img.scaled(Covers::constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            QString cacheName=song.extraField(Song::OnlineImageCacheName);
            fileName=cacheName.isEmpty()
                        ? Utils::cacheDir(id.toLower(), true)+Covers::encodeName(song.album.isEmpty() ? song.albumArtist() : (song.albumArtist()+" - "+song.album))+(png ? ".png" : ".jpg")
                        : cacheName;
            if (png && !cacheName.isEmpty()) {
                QImage img=QImage::fromData(data, Covers::imageFormat(data));
                img.save(fileName, "JPG");
                DBUG << "Saved image to" << fileName;
            } else {
                QFile f(fileName);
                if (f.open(QIODevice::WriteOnly)) {
                    DBUG << "Saved image to" << fileName;
                    f.write(data);
                }
            }
        }
        emit cover(job.song, img, fileName);
    }
}

void CoverDownloader::failed(const Job &job)
{
    if (job.song.isArtistImageRequest()) {
        DBUG << "artist image" << job.song.albumArtist();
        emit artistImage(job.song, QImage(), QString());
    } else if (job.song.isComposerImageRequest()) {
        DBUG << "composer image" << job.song.composer();
        emit composerImage(job.song, QImage(), QString());
    } else {
        DBUG << "cover image" << job.song.albumArtist() << job.song.album;
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
        // (As of 2.3.0) No longer save artist images into MPD dir
//        if (saveInMpdDir && !job.song.isNonMPD() && canSaveTo(job.dir)) {
//            QString mpdDir=MPDConnection::self()->getDetails().dir;
//            if (!mpdDir.isEmpty() && job.dir.startsWith(mpdDir) && 2==job.dir.mid(mpdDir.length()).split(Utils::constDirSep, QString::SkipEmptyParts).count()) {
//                QDir d(job.dir);
//                d.cdUp();
//                savedName=save(mimeType, extension, d.absolutePath()+Utils::constDirSep+
//                               (job.song.isArtistImageRequest() ? Covers::artistFileName(job.song) : Covers::composerFileName(job.song)), img, raw);
//                if (!savedName.isEmpty()) {
//                    DBUG << job.song.file << savedName;
//                    return savedName;
//                }
//            }
//        }

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
    bool isComposer=job.song.isComposerImageRequest();
    bool isArtist=job.song.isArtistImageRequest();
    bool isCover=!isComposer && !isArtist;

    for (; it!=end; ++it) {
        if ((*it).song.isComposerImageRequest()) {
            if (isComposer && (*it).song.composer()==job.song.composer()) {
                return it;
            }
        } else if ((*it).song.isArtistImageRequest()) {
            if (isArtist && (*it).song.albumArtist()==job.song.albumArtist()) {
                return it;
            }
        } else if (isCover && (*it).song.albumArtist()==job.song.albumArtist() && (*it).song.album==job.song.album) {
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
    : timer(nullptr)
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
        timer=thread->createTimer(this);
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
static const int constMaxCoverUpdatePerIteration=10;

void CoverLocator::locate()
{
    QList<Song> toDo;
    for (int i=0; i<constMaxCoverUpdatePerIteration && !queue.isEmpty(); ++i) {
        toDo.append(queue.takeFirst());
    }
    if (toDo.isEmpty()) {
        return;
    }
    QList<LocatedCover> covers;
    for (const Song &s: toDo) {
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
    : timer(nullptr)
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
        timer=thread->createTimer(this);
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
    for (int i=0; i<constMaxCoverUpdatePerIteration && !queue.isEmpty(); ++i) {
        toDo.append(queue.takeFirst());
    }
    if (toDo.isEmpty()) {
        return;
    }
    QList<LoadedCover> covers;
    for (const LoadedCover &s: toDo) {
        DBUG << s.song.albumArtist() << s.song.albumId() << s.song.size;
        int size=s.song.size;
        if (size<constRetinaScaleMaxSize) {
            size*=devicePixelRatio;
        }
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
    : downloader(nullptr)
    , locator(nullptr)
    , loader(nullptr)
{
    devicePixelRatio=qApp->devicePixelRatio();

    // Use screen size to calculate max cost - Issue #1498
    int maxCost = 0;
    QDesktopWidget *dw=QApplication::desktop();
    if (dw) {
        QWidget w;
        QSize sz = dw->availableGeometry(&w).size();
        maxCost = sz.width() * sz.height() * 5; // *5 as 32-bit pixmap (so 4 bytes), + some wiggle rooom :-)
    }
    cache.setMaxCost(qMax(static_cast<int>(15*1024*1024*devicePixelRatio), maxCost)); // Ensure at least 15M
}

void Covers::readConfig()
{
    saveInMpdDir=Settings::self()->storeCoversInMpdDir();
    fetchCovers=Settings::self()->fetchCovers();
    albumCoverName=Settings::self()->coverFilename();
    if (albumCoverName.isEmpty()) {
        albumCoverName=constFileName;
    }
}

void Covers::stop()
{
    if (downloader) {
        disconnect(downloader, SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImageDownloaded(Song,QImage,QString)));
        disconnect(downloader, SIGNAL(composerImage(Song,QImage,QString)), this, SLOT(composerImageDownloaded(Song,QImage,QString)));
        disconnect(downloader, SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverDownloaded(Song,QImage,QString)));
        downloader->stop();
        downloader=nullptr;
    }
    if (locator) {
        disconnect(locator, SIGNAL(located(QList<LocatedCover>)), this, SLOT(located(QList<LocatedCover>)));
        locator->stop();
        locator=nullptr;
    }
    if (loader) {
        disconnect(loader, SIGNAL(loaded(QList<LoadedCover>)), this, SLOT(loaded(QList<LoadedCover>)));
        loader->stop();
        loader=nullptr;
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
    if (size<4 || song.isUnknownAlbum()) {
        return nullptr;
    }
//    DBUG_CLASS("Covers") << song.albumArtist() << song.album << song.mbAlbumId << size;
    QString key=cacheKey(song, size);
    QPixmap *pix(cache.object(key));
    if (!pix) {
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
    return pix && pix->width()>1 ? pix : nullptr;
}

QPixmap * Covers::saveScaledCover(const QImage &img, const Song &song, int size)
{
    if (size<4) {
        return nullptr;
    }

    if (!isOnlineServiceImage(song)) {
        QString fileName=getScaledCoverName(song, size, true);
        bool status=img.save(fileName, constScaledFormat);
        DBUG_CLASS("Covers") << song.albumArtist() << song.album << song.mbAlbumId() << size << fileName << status;
    }
    QPixmap *pix=new QPixmap(QPixmap::fromImage(img));
    cache.insert(cacheKey(song, size), pix, pix->width()*pix->height()*(pix->depth()/8));
    cacheSizes.insert(size);
    return pix;
}

QPixmap * Covers::defaultPix(const Song &song, int size, int origSize)
{
    bool podcast=!song.isArtistImageRequest() && !song.isComposerImageRequest() && song.isFromOnlineService() && OnlineService::isPodcasts(song.onlineService());
    QString key=song.isArtistImageRequest()
                ? QLatin1String("artist-")
                    : song.isComposerImageRequest()
                        ? QLatin1String("composer-")
                        : podcast
                            ? QLatin1String("podcast-")
                            : QLatin1String("album-");

    key+=QString::number(size);
    QPixmap *pix=cache.object(key);
    if (!pix) {
        const QIcon &icn=song.isArtistImageRequest() || song.isComposerImageRequest()
                ? Icons::self()->artistIcon
                : podcast
                    ? Icons::self()->podcastIcon
                    : Icons::self()->albumIcon(size);
        pix=new QPixmap(icn.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        if (size!=origSize) {
            pix->setDevicePixelRatio(devicePixelRatio);
            DBUG << "Set pixel ratio of dummy pixmap" << devicePixelRatio;
        }
        cache.insert(key, pix, 1);
        cacheSizes.insert(size);
    }
    return pix;
}

QPixmap * Covers::get(const Song &song, int size, bool urgent)
{
    VERBOSE_DBUG_CLASS("Covers") << song.albumArtist() << song.album << song.mbAlbumId() << song.composer() << song.isArtistImageRequest() << song.isComposerImageRequest() << size << urgent << song.type << song.isStandardStream() << isOnlineServiceImage(song);
    QString key;
    QPixmap *pix=nullptr;
    if (0==size) {
        size=22;
    }

    int origSize=size;
    if (size<constRetinaScaleMaxSize) {
        size*=devicePixelRatio;
    }
    if (!song.isUnknownAlbum() || song.isStandardStream()) {
        key=cacheKey(song, size);
        pix=cache.object(key);

        if (!pix) {
            /*if (song.isArtistImageRequest() && song.isVariousArtists()) {
                // Load artist image...
                pix=new QPixmap(Icons::self()->artistIcon.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else*/ if (Song::SingleTracks==song.type) {
                pix=new QPixmap(Icons::self()->albumIcon(size).pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else if (song.isStandardStream()) {
                pix=new QPixmap(Icons::self()->streamIcon.pixmap(size, size).scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else if (isOnlineServiceImage(song)) {
                Covers::Image img=serviceImage(song);
                if (!img.img.isNull()) {
                    pix=new QPixmap(QPixmap::fromImage(img.img.scaled(QSize(size, size), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                }
            }
            if (pix) {
                VERBOSE_DBUG_CLASS("Covers") << "Got standard image";
                if (size!=origSize) {
                    pix->setDevicePixelRatio(devicePixelRatio);
                    VERBOSE_DBUG << "Set pixel ratio of cover" << devicePixelRatio;
                }
                cache.insert(key, pix, 1);
                cacheSizes.insert(size);
            }
        }
        if (!pix) {
            if (urgent) {
                QImage cached=loadScaledCover(song, size);
                if (cached.isNull()) {
                    Image img=findImage(song, false);
                    if (!img.img.isNull()) {
                        pix=saveScaledCover(scale(song, img.img, size), song, size);
                        if (size!=origSize) {
                            pix->setDevicePixelRatio(devicePixelRatio);
                            VERBOSE_DBUG << "Set pixel ratio of saved scaled cover" << devicePixelRatio;
                        }
                        return pix;
                    } else if (constNoCover==img.fileName) {
                        return defaultPix(song, size, origSize);
                    }
                } else {
                    pix=new QPixmap(QPixmap::fromImage(cached));
                    if (size!=origSize) {
                        pix->setDevicePixelRatio(devicePixelRatio);
                        VERBOSE_DBUG << "Set pixel ratio of loaded scaled cover" << devicePixelRatio;
                    }
                    cache.insert(key, pix, pix->width()*pix->height()*(pix->depth()/8));
                    cacheSizes.insert(size);
                    return pix;
                }
            }
            VERBOSE_DBUG << "Cached cover not found";
            tryToLoad(setSizeRequest(song, origSize));

            // Create a dummy image so that we dont keep on locating/loading/downloading files that do not exist!
            pix=new QPixmap(1, 1);
            if (size!=origSize) {
                pix->setDevicePixelRatio(devicePixelRatio);
                VERBOSE_DBUG << "Set pixel ratio of dummy cover" << devicePixelRatio;
            }
            cache.insert(key, pix, 1);
            cacheSizes.insert(size);
        }

        if (pix && pix->width()>1) {
            VERBOSE_DBUG << "Found cached pixmap" << pix->width();
            return pix;
        }
    }
    VERBOSE_DBUG << "Use default pixmap";
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

    for (int s: cacheSizes) {
        QString key=cacheKey(song, s);
        QPixmap *pix(cache.object(key));

        if (pix && (!dummyEntriesOnly || pix->width()<2)) {
            double pixRatio=pix->devicePixelRatio();
            cache.remove(key);
            if (!img.isNull()) {
                DBUG_CLASS("Covers");
                QPixmap *p=saveScaledCover(scale(song, img, s), song, s);
                if (p) {
                    p->setDevicePixelRatio(pixRatio);
                    DBUG << "Set pixel ratio of updated cached pixmap" << devicePixelRatio;
                }
                if (p && emitLoaded) {
                    if (pixRatio>1.00001) {
                        s/=pixRatio;
                    }
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

static Covers::Image findCoverInDir(const Song &song, const QString &dirName, const QStringList &coverFileNames, const QString &songFileName=QString())
{
    for (const QString &fileName: coverFileNames) {
        DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
        if (QFile::exists(dirName+fileName)) {
            QImage img=loadImage(dirName+fileName);
            if (!img.isNull()) {
                DBUG_CLASS("Covers") << "Got cover image" << QString(dirName+fileName);
                return Covers::Image(img, dirName+fileName);
            }
        }
    }

    if (!songFileName.isEmpty()) {
        #ifdef TAGLIB_FOUND
        DBUG_CLASS("Covers") << "Checking file" << songFileName;
        if (QFile::exists(songFileName)) {
            QImage img(Tags::readImage(songFileName));
            if (!img.isNull()) {
                DBUG_CLASS("Covers") << "Got cover image from tag" << songFileName;

                // Save image to cache folder - required for MPRIS
                if (!song.isCdda() && !song.isArtistImageRequest()) {
                    QString dir = Utils::cacheDir(Covers::constCoverDir+Covers::encodeName(song.albumArtist()), true);
                    if (!dir.isEmpty()) {
                        QString fileName=dir+Covers::encodeName(song.album)+".jpg";
                        if (img.save(fileName)) {
                            return Covers::Image(img, fileName);
                        }
                    }
                }

                return Covers::Image(img, Covers::constCoverInTagPrefix+songFileName);
            }
        }
        #endif
    }

    QStringList files=QDir(dirName).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable);
    for (const QString &fileName: files) {
        DBUG_CLASS("Covers") << "Checking file" << QString(dirName+fileName);
        QImage img=loadImage(dirName+fileName);
        if (!img.isNull()) {
            DBUG_CLASS("Covers") << "Got cover image" << QString(dirName+fileName);
            return Covers::Image(img, dirName+fileName);
        }
    }
    return Covers::Image();
}

Covers::Image Covers::locateImage(const Song &song)
{
    DBUG_CLASS("Covers") << song.file << song.albumArtist() << song.albumartist << song.album << song.type;
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
            img.fileName=Utils::cacheDir(id.toLower(), true)+Covers::encodeName(song.album.isEmpty() ? song.albumArtist() : (song.albumArtist()+" - "+song.album))+".jpg";
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
        DBUG_CLASS("Covers") << "Stream file" << songFile;
    }

    QStringList coverFileNames;
    if (song.isArtistImageRequest()) {
        QString basicArtist=song.basicArtist();
        coverFileNames=QStringList() << basicArtist+".jpg" << basicArtist+".png" << basicArtist+".jpeg" << constArtistImage+".jpg" << constArtistImage+".png" << constArtistImage+".jpeg";
    } else if (song.isComposerImageRequest()) {
        coverFileNames=QStringList() << constComposerImage+".jpg" << constComposerImage+".png" << constComposerImage+".jpeg";
    } else {
        QString mpdCover=albumFileName(song);
        if (!mpdCover.isEmpty()) {
            for (int e=0; constExtensions[e]; ++e) {
                coverFileNames << mpdCover+constExtensions[e];
            }
        }
        for (const QString &std: standardNames()) {
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

    if (!songFile.isEmpty() && !songFile.startsWith("http:/", Qt::CaseInsensitive) && !songFile.startsWith("https:/", Qt::CaseInsensitive) && !song.isCdda() &&
        (haveAbsPath || song.isCantataStream() || (!MPDConnection::self()->getDetails().dir.isEmpty() && !MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)  &&
                                                   !MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) ) ) {
        dirName=song.isCantataStream() ? Utils::getDir(songFile)
                                       : songFile.endsWith(Utils::constDirSep)
                                            ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+songFile
                                            : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+songFile);

        if (song.isArtistImageRequest() || song.isComposerImageRequest()) {
            for (int level=0; level<2; ++level) {
                for (const QString &fileName: coverFileNames) {
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
                if (!dirName.isEmpty() && !dirName.startsWith(QLatin1String("http:/"), Qt::CaseInsensitive) && !dirName.startsWith(QLatin1String("https:/"), Qt::CaseInsensitive)) {
                    dirName+=basicArtist+Utils::constDirSep;
                    for (const QString &fileName: coverFileNames) {
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
            Covers::Image img=findCoverInDir(song, dirName, coverFileNames, haveAbsPath || song.isCantataStream() ? songFile : (MPDConnection::self()->getDetails().dir+songFile));
            if (!img.img.isNull()) {
                return img;
            }

            QStringList dirs=QDir(dirName).entryList(QDir::Dirs|QDir::Readable|QDir::NoDotAndDotDot);
            for (const QString &dir: dirs) {
                img=findCoverInDir(song, dirName+dir+Utils::constDirSep, coverFileNames);
                if (!img.img.isNull()) {
                    return img;
                }
            }
        }
    }

    if (song.isArtistImageRequest() || song.isComposerImageRequest()) {
        QString artistOrComposer=encodeName(song.isComposerImageRequest() ? song.composer() : song.albumArtist());
        if (!artistOrComposer.isEmpty()) {
            // For non-MPD tracks, see if we actually have a saved MPD cover...
            if (MPDConnection::self()->getDetails().dirReadable) {
                QString songDir=artistOrComposer+Utils::constDirSep;
                if (!song.file.startsWith(songDir)) {
                    QString dirName=MPDConnection::self()->getDetails().dir+songDir;
                    if (QDir(dirName).exists()) {
                        for (const QString &fileName: coverFileNames) {
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
            if (!dir.isEmpty()) {
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
                    for (const QString &fileName: coverFileNames) {
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
        if (!dir.isEmpty()) {
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
    if (song.isUnknownAlbum()) {
        return Image();
    }

    DBUG << song.file << song.artist << song.albumartist << song.album << song.composer() << song.isArtistImageRequest() << song.isComposerImageRequest();

    if (urgent && song.isFromOnlineService()) {
        Covers::Image img=serviceImage(song);
        if (!img.img.isNull()) {
            return img;
        }
    }
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
    for (const LocatedCover &cvr: covers) {
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
    for (const LoadedCover &cvr: covers) {
        if (!cvr.img.isNull()) {
            int size=cvr.song.size;
            int origSize=size;
            if (size<constRetinaScaleMaxSize) {
                size*=devicePixelRatio;
            }
            QPixmap *pix=new QPixmap(QPixmap::fromImage(cvr.img));
            if (size!=origSize) {
                pix->setDevicePixelRatio(devicePixelRatio);
                DBUG << "Set pixel ratio of loaded pixmap" << devicePixelRatio;
            }
            cache.insert(cacheKey(cvr.song, size), pix, pix->width()*pix->height()*(pix->depth()/8));
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

        for (const QString &f: entries) {
            if (f.endsWith(".jpg") || f.endsWith(".jpeg") || f.endsWith(".png")) {
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
//    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/", Qt::CaseInsensitive) && !fileName.startsWith("https:/", Qt::CaseInsensitive)  ) {
        mutex.lock();
        filenames.insert(key, fileName.isEmpty() ? constNoCover : fileName);
        mutex.unlock();
//    }
    if (emitResult) {
        bool updatedCover=false;
        if (!img.isNull()) {
            updatedCover=updateCache(song, img, true);
        }
        if (updatedCover || song.isCdda()/* || !song.isSpecificSizeRequest()*/) {
            DBUG << "emit cover" << song.file << song.artist << song.albumartist << song.album << song.mbAlbumId() << img.width() << img.height() << fileName;
            emit cover(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
        }
    }
}

void Covers::gotArtistImage(const Song &song, const QImage &img, const QString &fileName, bool emitResult)
{
    QString key=artistKey(song);
    currentImageRequests.remove(key);
//    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/", Qt::CaseInsensitive) && !fileName.startsWith("https:/", Qt::CaseInsensitive)) {
        mutex.lock();
        filenames.insert(key, fileName.isEmpty() ? constNoCover : fileName);
        mutex.unlock();
//    }
    if (emitResult) {
        if (!img.isNull()) {
            updateCache(song, img, true);
        }
//        if (!song.isSpecificSizeRequest()) {
            DBUG << "emit artistImage" << song.file << song.artist << song.albumartist << song.album << img.width() << img.height() << fileName;
            emit artistImage(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
//        }
    }
}

void Covers::gotComposerImage(const Song &song, const QImage &img, const QString &fileName, bool emitResult)
{
    QString key=composerKey(song);
    currentImageRequests.remove(key);
//    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/", Qt::CaseInsensitive) && !fileName.startsWith("https:/", Qt::CaseInsensitive)) {
        mutex.lock();
        filenames.insert(key, fileName.isEmpty() ? constNoCover : fileName);
        mutex.unlock();
//    }
    if (emitResult) {
        if (!img.isNull()) {
            updateCache(song, img, true);
        }
//        if (!song.isSpecificSizeRequest()) {
            DBUG << "emit composerImage" << song.file << song.artist << song.albumartist << song.album << song.composer() << img.width() << img.height() << fileName;
            emit composerImage(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
//        }
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

#include "moc_covers.cpp"
