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
#include "song.h"
#include "utils.h"
#include "mpdconnection.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "config.h"
#include "deviceoptions.h"
#include "thread.h"
#ifdef ENABLE_ONLINE_SERVICES
#include "soundcloudservice.h"
#include "podcastservice.h"
#include "onlineservicesmodel.h"
#endif
#ifdef TAGLIB_FOUND
#include "tags.h"
#endif
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QTextStream>
#include <qglobal.h>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include <QXmlStreamReader>
#include <QTimer>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#include <KDE/KGlobal>
#include <QApplication>
K_GLOBAL_STATIC(Covers, instance)
#endif

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

const QLatin1String Covers::constLastFmApiKey("11172d35eb8cc2fd33250a9e45a2d486");
const QLatin1String Covers::constCoverDir("covers/");
const QLatin1String Covers::constScaledCoverDir("covers-scaled/");
const QLatin1String Covers::constCddaCoverDir("cdda/");
const QLatin1String Covers::constFileName("cover");
const QLatin1String Covers::constArtistImage("artist");

static const char * constExtensions[]={".jpg", ".png", 0};
static bool saveInMpdDir=true;
static bool fetchCovers=true;
static QString constCoverInTagPrefix=QLatin1String("{tag}");

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
            #ifndef Q_OS_WIN
            if (!MPDConnection::self()->getDetails().dir.isEmpty() && filePrefix.startsWith(MPDConnection::self()->getDetails().dir)) {
                Utils::setFilePerms(filePrefix+mimeType);
            }
            #endif
            return filePrefix+mimeType;
        }
    }

    if (extension!=mimeType) {
        if (QFile::exists(filePrefix+extension)) {
            return filePrefix+extension;
        }

        if (img.save(filePrefix+extension)) {
            #ifndef Q_OS_WIN
            if (!MPDConnection::self()->getDetails().dir.isEmpty() && filePrefix.startsWith(MPDConnection::self()->getDetails().dir)) {
                Utils::setFilePerms(filePrefix+mimeType);
            }
            #endif
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

static inline QString albumKey(const Song &s)
{
    return "{"+s.albumArtist()+"}{"+s.album+"}";
}

static inline QString artistKey(const Song &s)
{
    return "{"+s.albumArtist()+"}";
}

static const QLatin1String constScaledFormat(".jpg");
static bool cacheScaledCovers=true;

static QString getScaledCoverName(const QString &artist, const QString &album, int size, bool createDir)
{
    if (album.isEmpty()) {
        QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/'), createDir);
        return dir.isEmpty() ? QString() : (dir+Covers::encodeName(artist)+constScaledFormat);
    }

    QString dir=Utils::cacheDir(Covers::constScaledCoverDir+QString::number(size)+QLatin1Char('/')+Covers::encodeName(artist), createDir);
    return dir.isEmpty() ? QString() : (dir+Covers::encodeName(album)+constScaledFormat);
}

static void clearScaledCache(const Song &song)
{
    bool artistImage=song.isArtistImageRequest();
    QString dirName=Utils::cacheDir(Covers::constScaledCoverDir, false);
    if (dirName.isEmpty()) {
        return;
    }

    QDir d(dirName);
    if (!d.exists()) {
        return;
    }

    QStringList sizeDirNames=d.entryList(QStringList() << "*", QDir::Dirs|QDir::NoDotAndDotDot);

    if (artistImage) {
        QString fileName=Covers::encodeName(song.artist)+constScaledFormat;
        foreach (const QString &sizeDirName, sizeDirNames) {
            QString fname=dirName+sizeDirName+QLatin1Char('/')+fileName;
            if (QFile::exists(fname)) {
                QFile::remove(fname);
            }
        }
    } else {
        QString subDir=Covers::encodeName(song.artist);
        QString fileName=Covers::encodeName(song.album)+constScaledFormat;
        foreach (const QString &sizeDirName, sizeDirNames) {
            QString fname=dirName+sizeDirName+QLatin1Char('/')+subDir+QLatin1Char('/')+fileName;
            if (QFile::exists(fname)) {
                QFile::remove(fname);
            }
        }
    }
}

QPixmap * Covers::getScaledCover(const QString &artist, const QString &album, int size)
{
    if (!cacheScaledCovers) {
        return 0;
    }

    DBUG_CLASS("Covers") << artist << album << size;
    QString cache=getScaledCoverName(artist, album, size, true);
    if (!cache.isEmpty() && QFile::exists(cache)) {
        QImage img(cache);
        if (!img.isNull() && (img.width()==size || img.height()==size)) {
            DBUG_CLASS("Covers") << artist << album << size << "scaled cover found";
            return new QPixmap(QPixmap::fromImage(img));
        }
    }
    return 0;
}

bool Covers::saveScaledCover(const QImage &img, const QString &artist, const QString &album, int size)
{
    if (!cacheScaledCovers) {
        return false;
    }

    QString fileName=getScaledCoverName(artist, album, size, true);
    bool status=img.save(fileName);
    DBUG_CLASS("Covers") << artist << album << size << fileName << status;
    return status;
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
    name.replace("/", "_");
    return name;
}

QString Covers::albumFileName(const Song &song)
{
    QString coverName=MPDConnection::self()->getDetails().coverName;
    if (coverName.isEmpty()) {
        coverName=Covers::constFileName;
    } else if (coverName.contains("%")) {
        coverName.replace(DeviceOptions::constAlbumArtist, encodeName(song.albumArtist()));
        coverName.replace(DeviceOptions::constTrackArtist, encodeName(song.albumArtist()));
        coverName.replace(DeviceOptions::constAlbumTitle, encodeName(song.album));
        coverName.replace("%", "");
    }
    return coverName;
}

QString Covers::artistFileName(const Song &song)
{
    QString coverName=MPDConnection::self()->getDetails().coverName;
    if (coverName.contains("%")){
        return encodeName(song.albumArtist());
    }
    return constArtistImage;
}

QString Covers::fixArtist(const QString &artist)
{
    if (QLatin1String("AC-DC")==artist) {
        return QLatin1String("AC/DC");
    }
    return artist;
}

#if !defined Q_OS_WIN
static QString xdgConfig()
{
    QString env = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    return Utils::fixPath(env.isEmpty() ? QDir::homePath() + QLatin1String("/.config/") : env);
}

#ifndef ENABLE_KDE_SUPPORT
static QString kdeHome()
{
    static QString kdeHomePath;
    if (kdeHomePath.isEmpty()) {
        kdeHomePath = QString::fromLocal8Bit(qgetenv("KDEHOME"));
        if (kdeHomePath.isEmpty())  {
            QDir homeDir(QDir::homePath());
            QString kdeConfDir(QLatin1String("/.kde"));
            if (homeDir.exists(QLatin1String(".kde4"))) {
                kdeConfDir = QLatin1String("/.kde4");
            }
            kdeHomePath = QDir::homePath() + kdeConfDir;
        }
    }
    return kdeHomePath;
}
#endif

static Covers::Image otherAppCover(const CoverDownloader::Job &job)
{
    #ifdef ENABLE_KDE_SUPPORT
    QString kdeDir=KGlobal::dirs()->localkdedir();
    #else
    QString kdeDir=kdeHome();
    #endif
    Covers::Image app;
    app.fileName=kdeDir+QLatin1String("/share/apps/amarok/albumcovers/large/")+
                 QCryptographicHash::hash(job.song.albumArtist().toLower().toLocal8Bit()+job.song.album.toLower().toLocal8Bit(),
                                          QCryptographicHash::Md5).toHex();

    app.img=QImage(app.fileName);

    if (app.img.isNull()) {
        app.fileName=xdgConfig()+QLatin1String("/Clementine/albumcovers/")+
                     QCryptographicHash::hash(job.song.albumArtist().toLower().toUtf8()+job.song.album.toLower().toUtf8(),
                                              QCryptographicHash::Sha1).toHex()+QLatin1String(".jpg");

        app.img=QImage(app.fileName);
    }

    if (!app.img.isNull() && saveInMpdDir && canSaveTo(job.dir)) {
        QFile f(app.fileName);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray raw=f.readAll();
            if (!raw.isEmpty()) {
                QString mimeType=typeFromRaw(raw);
                QString savedName;
                QString coverName=Covers::albumFileName(job.song);
                savedName=save(mimeType, mimeType.isEmpty() ? constExtensions[0] : mimeType, job.dir+coverName, app.img, raw);
                if (!savedName.isEmpty()) {
                    app.fileName=savedName;
                }
            }
        }
    }

    return app.img.isNull() ? Covers::Image() : app;
}
#endif

const QSize Covers::constMaxSize(600, 600);

Covers * Covers::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Covers *instance=0;
    if(!instance) {
        instance=new Covers;
    }
    return instance;
    #endif
}

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
{
    manager=new NetworkAccessManager(this);
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
    bool isArtistImage=song.isArtistImageRequest();

    if (jobs.end()!=findJob(Job(song, QString(), isArtistImage))) {
        return;
    }

    QString dirName;
    QString fileName=song.filePath();
    bool haveAbsPath=fileName.startsWith(Utils::constDirSep);

    if (haveAbsPath || !MPDConnection::self()->getDetails().dir.isEmpty()) {
        dirName=fileName.endsWith(Utils::constDirSep) ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+fileName
                                                      : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+fileName);
    }

    Job job(song, dirName, isArtistImage);

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
    bool isArtistImage=job.song.isArtistImageRequest();
    QString coverName=isArtistImage ? Covers::artistFileName(job.song) : Covers::albumFileName(job.song);
    coverName+=constExtensions[JobHttpJpg==type ? 0 : 1];
    QString dir=Utils::getDir(job.filePath);
    if (isArtistImage) {
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
    NetworkJob *j=manager->get(u);
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

    query.addQueryItem("method", job.isArtist ? "artist.getInfo" : "album.getInfo");
    query.addQueryItem("api_key", Covers::constLastFmApiKey);
    query.addQueryItem("autocorrect", "1");
    query.addQueryItem("artist", Covers::fixArtist(job.song.albumArtist()));
    if (!job.isArtist) {
        query.addQueryItem("album", job.song.album);
    }
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    NetworkJob *j = manager->get(url);
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
            bool isArtistImage=job.isArtist;

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
            NetworkJob *j=manager->get(QNetworkRequest(u));
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
            } else if (fetchCovers && JobHttpPng==job.type && (!job.level || !downloadViaHttp(job, JobHttpPng))) {
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

            if (job.isArtist) {
                DBUG << "artist image, null?" << img.img.isNull();
                emit artistImage(job.song, img.img, img.fileName);
            } else if (img.img.isNull()) {
                DBUG << "failed to download cover image";
                #if defined Q_OS_WIN
                emit cover(job.song, QImage(), QString());
                #else
                img=otherAppCover(job);
                emit cover(job.song, img.img, img.fileName);
                #endif
            } else {
                DBUG << "got cover image" << img.fileName;
                emit cover(job.song, img.img, img.fileName);
            }
        }
    }

    reply->deleteLater();
}

void CoverDownloader::failed(const Job &job)
{
    if (job.isArtist) {
        DBUG << "failed to download artist image";
        emit artistImage(job.song, QImage(), QString());
    } else {
        #if defined Q_OS_WIN
        DBUG << "Failed to download cover image";
        emit cover(job.song, QImage(), QString());
        #else
        DBUG << "failed to download cover image - try other app";
        Covers::Image img=otherAppCover(job);
        emit cover(job.song, img.img, img.fileName);
        #endif
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

    if (job.isArtist) {
        if (saveInMpdDir && !job.song.isNonMPD() && canSaveTo(job.dir)) {
            QString mpdDir=MPDConnection::self()->getDetails().dir;
            if (!mpdDir.isEmpty() && job.dir.startsWith(mpdDir) && 2==job.dir.mid(mpdDir.length()).split(Utils::constDirSep, QString::SkipEmptyParts).count()) {
                QDir d(job.dir);
                d.cdUp();
                savedName=save(mimeType, extension, d.absolutePath()+Utils::constDirSep+Covers::artistFileName(job.song), img, raw);
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
        if ((*it).isArtist==job.isArtist && (*it).song.albumArtist()==job.song.albumArtist() && (*it).song.album==job.song.album) {
            return it;
        }
    }

    return end;
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
static int maxPerLoopIteration=5;

// Max number of 'findImage' per iteration - these are done in GUI thread, so keep small...
static int maxFindPerLoopIteration=5;

void CoverLocator::locate()
{
    QList<Song> toDo;
    for (int i=0; i<maxPerLoopIteration && !queue.isEmpty(); ++i) {
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

Covers::Covers()
    : retrieved(0)
    , cache(300000)
    , countResetTimer(0)
{
    maxPerLoopIteration=Settings::self()->maxCoverUpdatePerIteration();
    maxFindPerLoopIteration=Settings::self()->maxCoverFindPerIteration();

    qRegisterMetaType<LocatedCover>("LocatedCover");
    qRegisterMetaType<QList<LocatedCover> >("QList<LocatedCover>");
    downloader=new CoverDownloader();
    locator=new CoverLocator();
    connect(this, SIGNAL(download(Song)), downloader, SLOT(download(Song)), Qt::QueuedConnection);
    connect(downloader, SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImageDownloaded(Song,QImage,QString)), Qt::QueuedConnection);
    connect(downloader, SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverDownloaded(Song,QImage,QString)), Qt::QueuedConnection);
    connect(locator, SIGNAL(located(QList<LocatedCover>)), this, SLOT(located(QList<LocatedCover>)), Qt::QueuedConnection);
    connect(this, SIGNAL(locate(Song)), locator, SLOT(locate(Song)), Qt::QueuedConnection);
}

void Covers::readConfig()
{
    saveInMpdDir=Settings::self()->storeCoversInMpdDir();
    cacheScaledCovers=Settings::self()->cacheScaledCovers();
    fetchCovers=Settings::self()->fetchCovers();
}

void Covers::stop()
{
    disconnect(downloader, SIGNAL(artistImage(Song,QImage,QString)), this, SLOT(artistImageDownloaded(Song,QImage,QString)));
    disconnect(downloader, SIGNAL(cover(Song,QImage,QString)), this, SLOT(coverDownloaded(Song,QImage,QString)));
    downloader->stop();
    disconnect(locator, SIGNAL(located(QList<LocatedCover>)), this, SLOT(located(QList<LocatedCover>)));
    locator->stop();
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    cleanCdda();
    #endif
}

static inline QString cacheKey(const Song &song, int size)
{
    return albumKey(song)+QString::number(size);
}

QPixmap * Covers::get(const Song &song, int size)
{
    if (song.isUnknown()) {
        return 0;
    }

    QString key=cacheKey(song, size);
    QPixmap *pix(cache.object(key));

    if (!pix) {
        QImage img;
        #ifdef ENABLE_ONLINE_SERVICES
        if (song.isFromOnlineService()) {
            if (SoundCloudService::constName==song.onlineService()) {
                img=QImage(SoundCloudService::iconPath());
            } else if (PodcastService::constName==song.onlineService()) {
                img=QImage(PodcastService::iconPath());
            }
        }
        #endif

        if (img.isNull()) {
            img=findImage(song, false).img;
        }

        cacheSizes.insert(size);
        if (!img.isNull()) {
            pix=new QPixmap(QPixmap::fromImage(img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            cache.insert(key, pix, pix->width()*pix->height()*(pix->depth()/8));
        } else {
            // Attempt to download cover...
            if (Song::OnlineSvrTrack!=song.type) {
                emit download(song);
            }
            // Create a dummy pixmap so that we dont keep on stating files that do not exist!
            pix=new QPixmap(1, 1);
            cache.insert(key, pix, 4);
        }
    }

    return pix && pix->width()>1 ? pix : 0;
}

void Covers::coverDownloaded(const Song &song, const QImage &img, const QString &file)
{
    if (!img.isNull()) {
        clearCache(song, img, true);
    }
    gotAlbumCover(song, img, file);
}

void Covers::artistImageDownloaded(const Song &song, const QImage &img, const QString &file)
{
    gotArtistImage(song, img, file);
}

// dummyEntriesOnly: If we have downloaded a cover, we can remove the dummy entry - so that
//                   the next time get() is called, it can read the saved file!
void Covers::clearCache(const Song &song, const QImage &img, bool dummyEntriesOnly)
{
    clearScaledCache(song);
    bool hadDummy=false;
    bool hadEntries=false;
    foreach (int s, cacheSizes) {
        QString key=cacheKey(song, s);
        QPixmap *pix(cache.object(key));

        if (pix && (!dummyEntriesOnly || pix->width()<2)) {
            if (!hadDummy) {
                hadDummy=pix->width()<2;
            }
            hadEntries=true;
            cache.remove(key);

            if (!img.isNull()) {
                pix=new QPixmap(QPixmap::fromImage(img.scaled(s, s, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                cache.insert(key, pix, pix->width()*pix->height()*(pix->depth()/8));
            }
        }
    }

    if (hadDummy && dummyEntriesOnly) {
        emit coverRetrieved(song);
    }
    if (hadEntries && !dummyEntriesOnly) {
        emit coverRetrieved(song);
    }
}

Covers::Image Covers::findImage(const Song &song, bool emitResult)
{
    Covers::Image i=locateImage(song);
    if (!i.img.isNull()) {
        if (song.isArtistImageRequest()) {
            gotArtistImage(song, i.img, i.fileName, emitResult);
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
    if (Song::OnlineSvrTrack==song.type) {
        return OnlineServicesModel::self()->readImage(song);
    }
    #endif
    bool isArtistImage=song.isArtistImageRequest();
    QString prevFileName=Covers::self()->getFilename(song, isArtistImage);
    if (!prevFileName.isEmpty()) {
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
    if (isArtistImage) {
        QString artistFile=artistFileName(song);
        QString basicArtist=song.basicArtist();
        coverFileNames=QStringList() << basicArtist+".jpg" << basicArtist+".png" << artistFile+".jpg" << artistFile+".png";
    } else {
        QString mpdCover=albumFileName(song);
        if (!mpdCover.isEmpty()) {
            for (int e=0; constExtensions[e]; ++e) {
                coverFileNames << mpdCover+constExtensions[e];
            }
        }
        coverFileNames << standardNames();
        for (int e=0; constExtensions[e]; ++e) {
            coverFileNames+=Utils::changeExtension(Utils::getFile(songFile), constExtensions[e]);
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


        if (isArtistImage) {
            QString basicArtist=song.basicArtist();
            for (int level=0; level<2; ++level) {
                foreach (const QString &fileName, coverFileNames) {
                    if (QFile::exists(dirName+fileName)) {
                        QImage img=loadImage(dirName+fileName);
                        if (!img.isNull()) {
                            DBUG_CLASS("Covers") << "Got artist image" << QString(dirName+fileName);
                            return Image(img, dirName+fileName);
                        }
                    }
                }
                QDir d(dirName);
                d.cdUp();
                dirName=Utils::fixPath(d.absolutePath());
            }

            // For various artists tracks, or for non-MPD files, see if we have a matching backdrop in MPD.
            // e.g. artist=Wibble, look for $mpdDir/Wibble/backdrop.png
            if (song.isVariousArtists() || song.isNonMPD()) {
                dirName=MPDConnection::self()->getDetails().dirReadable ? MPDConnection::self()->getDetails().dir : QString();
                if (!dirName.isEmpty() && !dirName.startsWith(QLatin1String("http:/"))) {
                    dirName+=basicArtist+Utils::constDirSep;
                    foreach (const QString &fileName, coverFileNames) {
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
                QImage img=loadImage(dirName+fileName);
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cover image" << QString(dirName+fileName);
                    return Image(img, dirName+fileName);
                }
            }
        }
    }

    QString artist=encodeName(song.albumArtist());

    if (isArtistImage) {
        // For non-MPD tracks, see if we actually have a saved MPD cover...
        if (MPDConnection::self()->getDetails().dirReadable) {
            QString songDir=encodeName(song.albumArtist())+Utils::constDirSep;
            if (!song.file.startsWith(songDir)) {
                QString dirName=MPDConnection::self()->getDetails().dir+songDir;
                if (QDir(dirName).exists()) {
                    foreach (const QString &fileName, coverFileNames) {
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
        }
        // Check if cover is already cached
        QString dir(Utils::cacheDir(constCoverDir, false));
        for (int e=0; constExtensions[e]; ++e) {
            if (QFile::exists(dir+artist+constExtensions[e])) {
                QImage img=loadImage(dir+artist+constExtensions[e]);
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cached artist image" << QString(dir+artist+constExtensions[e]);
                    return Image(img, dir+artist+constExtensions[e]);
                }
            }
        }
    } else {
        QString album=encodeName(song.album);
        // For non-MPD tracks, see if we actually have a saved MPD cover...
        if (MPDConnection::self()->getDetails().dirReadable) {
            QString songDir=encodeName(song.albumArtist())+Utils::constDirSep+album+Utils::constDirSep;
            if (!song.file.startsWith(songDir)) {
                QString dirName=MPDConnection::self()->getDetails().dir+songDir;
                if (QDir(dirName).exists()) {
                    foreach (const QString &fileName, coverFileNames) {
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
            if (QFile::exists(dir+album+constExtensions[e])) {
                QImage img=loadImage(dir+album+constExtensions[e]);
                if (!img.isNull()) {
                    DBUG_CLASS("Covers") << "Got cached cover image" << QString(dir+album+constExtensions[e]);
                    return Image(img, dir+album+constExtensions[e]);
                }
            }
        }

        CoverDownloader::Job job(song, dirName);

        #if !defined Q_OS_WIN
        // See if amarok, or clementine, has it...
        Image app=otherAppCover(job);
        if (!app.img.isNull()) {
            DBUG_CLASS("Covers") << "Got cover image (other app)" << app.fileName;
            return app;
        }
        #endif
    }

    DBUG_CLASS("Covers") << "Failed to locate image";
    return Image(QImage(), QString());
}

// Dont return song files as cover files!
static Covers::Image fix(const Covers::Image &img)
{
    return Covers::Image(img.img, img.fileName.startsWith(constCoverInTagPrefix) ? QString() : img.fileName);
}

Covers::Image Covers::requestImage(const Song &song, bool urgent)
{
    DBUG << song.file << song.artist << song.albumartist << song.album;

    #ifdef ENABLE_ONLINE_SERVICES
    if (urgent && song.isFromOnlineService()) {
        Covers::Image img;
        if (SoundCloudService::constName==song.onlineService()) {
            img.fileName=SoundCloudService::iconPath();
        } else if (PodcastService::constName==song.onlineService()) {
            img.fileName=PodcastService::iconPath();
        }
        if (!img.fileName.isEmpty()) {
            img.img=loadImage(img.fileName);
            if (!img.img.isNull()) {
                return img;
            }
        }
    }
    #endif

    QString key=song.isArtistImageRequest() ? artistKey(song) : albumKey(song);
    if (currentImageRequests.contains(key)) {
        return Covers::Image();
    }

    if (retrieved>=maxFindPerLoopIteration && !urgent) {
        currentImageRequests.insert(key);
        emit locate(song);
        return Covers::Image();
    }

    retrieved++;
    Image img=findImage(song, false);
    if (img.img.isNull() && Song::OnlineSvrTrack!=song.type) {
        DBUG << song.file << song.artist << song.albumartist << song.album << "Need to download";
        currentImageRequests.insert(key);
        emit download(song);
    }

    // We only want to read X files per QEventLoop iteratation. The above takes care of this, and any
    // extra requests are handled via the 'emit locate' However, after the QEVentLoop, we want to reset
    // this count back to 0. To do this, create a QTimer with a timout of 0 seconds. This should fire
    // immediately after the current QEventLoop event.
    if (!countResetTimer) {
        countResetTimer=new QTimer(this);
        countResetTimer->setSingleShot(true);
        connect(countResetTimer, SIGNAL(timeout()), SLOT(clearCount()), Qt::QueuedConnection);
    }
    countResetTimer->start(0);
    return fix(img);
}

void Covers::clearCount()
{
    retrieved=0;
}

void Covers::located(const QList<LocatedCover> &covers)
{
    foreach (const LocatedCover &cvr, covers) {
        if (!cvr.img.isNull()) {
            if (cvr.song.isArtistImageRequest()) {
                gotArtistImage(cvr.song, cvr.img, cvr.fileName);
            } else {
                gotAlbumCover(cvr.song, cvr.img, cvr.fileName);
            }
        } else {
            emit download(cvr.song);
        }
    }
    retrieved-=covers.size();
    if (retrieved<0) {
        retrieved=0;
    }
}

void Covers::emitCoverUpdated(const Song &song, const QImage &img, const QString &file)
{
    clearCache(song, img, false);
    filenames[song.isArtistImageRequest() ? artistKey(song) : albumKey(song)]=file;
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
    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/")) {
        mutex.lock();
        filenames.insert(key, fileName);
        mutex.unlock();
    }
    if (emitResult) {
        DBUG << "emit cover" << song.file << song.artist << song.albumartist << song.album << img.width() << img.height() << fileName;
        emit cover(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
    }
}

void Covers::gotArtistImage(const Song &song, const QImage &img, const QString &fileName, bool emitResult)
{
    QString key=artistKey(song);
    currentImageRequests.remove(key);
    if (!img.isNull() && !fileName.isEmpty() && !fileName.startsWith("http:/")) {
        mutex.lock();
        filenames.insert(key, fileName);
        mutex.unlock();
    }
    if (emitResult) {
        DBUG << "emit artistImage" << song.file << song.artist << song.albumartist << song.album << img.width() << img.height() << fileName;
        emit artistImage(song, img, fileName.startsWith(constCoverInTagPrefix) ? QString() : fileName);
    }
}

QString Covers::getFilename(const Song &s, bool isArtist)
{
    mutex.lock();
    QMap<QString, QString>::ConstIterator fileIt=filenames.find(isArtist ? artistKey(s) : albumKey(s));
    QString f=fileIt==filenames.end() ? QString() : fileIt.value();
    mutex.unlock();
    return f;
}
