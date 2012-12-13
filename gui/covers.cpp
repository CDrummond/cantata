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

#include "covers.h"
#include "song.h"
#include "utils.h"
#include "mpdconnection.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "config.h"
#ifdef TAGLIB_FOUND
#include "tags.h"
#endif
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>
#include <QtCore/qglobal.h>
#include <QtNetwork/QNetworkReply>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QFont>
#include <QtXml/QXmlStreamReader>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#include <KDE/KGlobal>
#include <KDE/KTemporaryFile>
#include <KDE/KIO/NetAccess>
#include <QtGui/QApplication>
K_GLOBAL_STATIC(Covers, instance)
#endif

static const QLatin1String constApiKey("11172d35eb8cc2fd33250a9e45a2d486");
const QLatin1String Covers::constCoverDir("covers/");
static const QLatin1String constFileName("cover");
static const QStringList   constExtensions=QStringList() << ".jpg" << ".png";

static QStringList coverFileNames;
static bool saveInMpdDir=true;

void initCoverNames()
{
    if (coverFileNames.isEmpty()) {
        QStringList fileNames;
        fileNames << constFileName << QLatin1String("AlbumArt") << QLatin1String("folder");
        foreach (const QString &fileName, fileNames) {
            foreach (const QString &ext, constExtensions) {
                coverFileNames << fileName+ext;
            }
        }
    }
}

static inline bool isRequestingArtistImage(const Song &song)
{
    // If we are requesting artist image, then Song should ONLY have albumartist and file set
    // ...so check that some other fields are empty/default.
    return song.album.isEmpty() && song.artist.isEmpty() && !song.albumartist.isEmpty() && 0==song.size && 0==song.track;
}

static bool canSaveTo(const QString &dir)
{
    QString mpdDir=MPDConnection::self()->getDetails().dir;
    return !dir.isEmpty() && !mpdDir.isEmpty() && !mpdDir.startsWith(QLatin1String("http://")) && QDir(mpdDir).exists() && dir.startsWith(mpdDir);
}

static const QString typeFromRaw(const QByteArray &raw)
{
    if (raw.size()>9 && /*raw[0]==0xFF && raw[1]==0xD8 && raw[2]==0xFF*/ raw[6]=='J' && raw[7]=='F' && raw[8]=='I' && raw[9]=='F') {
        return constExtensions.at(0);
    } else if (raw.size()>4 && /*raw[0]==0x89 &&*/ raw[1]=='P' && raw[2]=='N' && raw[3]=='G') {
        return constExtensions.at(1);
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

static QString encodeName(QString name)
{
    name.replace("/", "_");
    return name;
}

#if !defined Q_OS_WIN
static QString xdgConfig()
{
    QString env = QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
    QString dir = (env.isEmpty() ? QDir::homePath() + QLatin1String("/.config/") : env);
    if (!dir.endsWith("/")) {
        dir=dir+'/';
    }
    return dir;
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

struct AppCover
{
    QImage img;
    QString filename;
};

static AppCover otherAppCover(const Covers::Job &job)
{
    #ifdef ENABLE_KDE_SUPPORT
    QString kdeDir=KGlobal::dirs()->localkdedir();
    #else
    QString kdeDir=kdeHome();
    #endif
    AppCover app;
    app.filename=kdeDir+QLatin1String("/share/apps/amarok/albumcovers/large/")+
                 QCryptographicHash::hash(job.song.albumArtist().toLower().toLocal8Bit()+job.song.album.toLower().toLocal8Bit(),
                                          QCryptographicHash::Md5).toHex();

    app.img=QImage(app.filename);

    if (app.img.isNull()) {
        app.filename=xdgConfig()+QLatin1String("/Clementine/albumcovers/")+
                     QCryptographicHash::hash(job.song.albumArtist().toLower().toUtf8()+job.song.album.toLower().toUtf8(),
                                              QCryptographicHash::Sha1).toHex()+QLatin1String(".jpg");

        app.img=QImage(app.filename);
    }

    if (!app.img.isNull() && saveInMpdDir && canSaveTo(job.dir)) {
        QFile f(app.filename);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray raw=f.readAll();
            if (!raw.isEmpty()) {
                QString mimeType=typeFromRaw(raw);
                QString savedName;
                QString coverName=MPDConnection::self()->getDetails().coverName;
                if (coverName.isEmpty()) {
                    coverName=constFileName;
                }
                savedName=save(mimeType, mimeType.isEmpty() ? constExtensions.at(0) : mimeType, job.dir+coverName, app.img, raw);
                if (!savedName.isEmpty()) {
                    app.filename=savedName;
                }
            }
        }
    }
    return app;
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

bool Covers::isCoverFile(const QString &file)
{
    initCoverNames();
    return coverFileNames.contains(file);
}

static bool fExists(const QString &dir, const QString &file)
{
    return QFile::exists(dir+file);
}

static void fCopy(const QString &sDir, const QString &sFile, const QString &dDir, const QString &dFile)
{
    QFile::copy(sDir+sFile, dDir+dFile);
}

static void copyImage(const QString &sourceDir, const QString &destDir, const QString &coverFile, const QString &destName, unsigned short maxSize)
{
    QImage img(sourceDir+coverFile);
    if (maxSize>0 && (img.width()>maxSize || img.height()>maxSize)) { // Need to scale image...
        img=img.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(destDir+destName);
    } else if (destName.right(4)!=coverFile.right(4)) { // Diff extensions, so need to convert image type...
        img.save(destDir+destName);
    } else { // no scaling, and same image type, so we can just copy...
        fCopy(sourceDir, coverFile, destDir, destName);
    }
    Utils::setFilePerms(destDir+destName);
}

bool Covers::copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name, unsigned short maxSize)
{
    // First, check if dir already has a cover file!
    initCoverNames();
    QStringList names=coverFileNames;
    foreach (const QString &ext, constExtensions) {
        names+=song.album+ext;
    }

    QString mpdCover=MPDConnection::self()->getDetails().coverName;
    if (mpdCover.isEmpty()) {
        mpdCover=constFileName;
    }

    foreach (const QString &ext, constExtensions) {
        if (!names.contains(mpdCover+ext)) {
            names.prepend(mpdCover+ext);
        }
    }

    foreach (const QString &ext, constExtensions) {
        names+=song.albumArtist()+QLatin1String(" - ")+song.album+ext;
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
                if (coverFileNames.at(0)!=coverFile) { // source is not 'cover.xxx'
                    QString ext(coverFile.endsWith(constExtensions.at(0)) ? constExtensions.at(0) : constExtensions.at(1));
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
        foreach (const QString &ext, constExtensions) {
            if (QFile::exists(dir+album+ext)) {
                copyImage(dir, destDir, album+ext, destName, maxSize);
                return true;
            }
        }
    }

    return false;
}

QStringList Covers::standardNames()
{
    initCoverNames();
    return coverFileNames;
}

Covers::Covers()
    : manager(0)
    , cache(300000)
    , queue(0)
    , queueThread(0)
{
    saveInMpdDir=Settings::self()->storeCoversInMpdDir();
}

void Covers::stop()
{
    if (queueThread) {
        disconnect(queue, SIGNAL(cover(const Song &, const QImage &, const QString &)), this, SIGNAL(cover(const Song &, const QImage &, const QString &)));
        disconnect(queue, SIGNAL(download(const Song &)), this, SLOT(download(const Song &)));
        Utils::stopThread(queueThread);
    }
}

static inline QString cacheKey(const Song &song, int size)
{
    return song.albumArtist()+QChar(':')+song.album+QChar(':')+QChar(':')+QString::number(size);
}

static QSet<int> cacheSizes;

QPixmap * Covers::get(const Song &song, int size)
{
    if (song.isUnknown()) {
        return 0;
    }

    QString key=cacheKey(song, size);
    QPixmap *pix(cache.object(key));

    if (!pix) {
        Covers::Image img=getImage(song);

        cacheSizes.insert(size);
        if (!img.img.isNull()) {
            pix=new QPixmap(QPixmap::fromImage(img.img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            cache.insert(key, pix, pix->width()*pix->height()*(pix->depth()/8));
        } else {
            // Attempt to download cover...
            download(song);
            // Create a dummy pixmap so that we dont keep on stating files that do not exist!
            pix=new QPixmap(1, 1);
            cache.insert(key, pix, 4);
        }
    }

    return pix && pix->width()>1 ? pix : 0;
}

// If we have downloaded a cover, we can remove the dummy entry - so that the next time get() is called,
// it can read the saved file!
void Covers::clearDummyCache(const Song &song, const QImage &img)
{
    bool hadDummy=false;
    foreach (int s, cacheSizes) {
        QString key=cacheKey(song, s);
        QPixmap *pix(cache.object(key));

        if (pix && pix->width()<2) {
            cache.remove(key);

            QStringList parts=img.isNull() ? QStringList() : key.split(':');
            if (parts.size()>3) {
                int size=parts.at(3).toInt();
                pix=new QPixmap(QPixmap::fromImage(img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                cache.insert(key, pix, pix->width()*pix->height()*(pix->depth()/8));
            }
            hadDummy=true;
        }
    }

    if (hadDummy) {
        emit coverRetrieved(song);
    }
}

Covers::Image Covers::getImage(const Song &song)
{
    QString dirName;
    QString songFile=song.file;
    bool haveAbsPath=song.file.startsWith('/');
    bool isArtistImage=isRequestingArtistImage(song);

    if (song.isCantataStream()) {
        QUrl u(songFile);
        songFile=u.hasQueryItem("file") ? u.queryItemValue("file") : QString();
    }
    if (!songFile.isEmpty() &&
        (haveAbsPath || (!MPDConnection::self()->getDetails().dir.isEmpty() && !MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http://")) ) ) ) {
        dirName=songFile.endsWith('/') ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+songFile
                                       : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+songFile);
        if (isArtistImage) {
            QStringList names=QStringList() << song.albumartist+".jpg" << song.albumartist+".png" << "artist.jpg" << "artist.png";
            for (int level=0; level<2; ++level) {
                foreach (const QString &fileName, names) {
                    if (QFile::exists(dirName+fileName)) {
                        QImage img(dirName+fileName);

                        if (!img.isNull()) {
                            return Image(img, dirName+fileName);
                        }
                    }
                }
                QDir d(dirName);
                d.cdUp();
                dirName=d.absolutePath();
            }
        } else {
            initCoverNames();
            QStringList names;
            QString mpdCover=MPDConnection::self()->getDetails().coverName;
            if (!mpdCover.isEmpty()) {
                foreach (const QString &ext, constExtensions) {
                    names << mpdCover+ext;
                }
            }
            names << coverFileNames;
            foreach (const QString &ext, constExtensions) {
                names+=Utils::changeExtension(Utils::getFile(song.file), ext);
            }
            foreach (const QString &ext, constExtensions) {
                names+=song.albumArtist()+QLatin1String(" - ")+song.album+ext;
            }
            foreach (const QString &ext, constExtensions) {
                names+=song.album+ext;
            }
            foreach (const QString &fileName, names) {
                if (QFile::exists(dirName+fileName)) {
                    QImage img(dirName+fileName);

                    if (!img.isNull()) {
                        return Image(img, dirName+fileName);
                    }
                }
            }

            #ifdef TAGLIB_FOUND
            QString fileName=haveAbsPath ? song.file : (MPDConnection::self()->getDetails().dir+songFile);
            if (QFile::exists(fileName)) {
                QImage img(Tags::readImage(fileName));
                if (!img.isNull()) {
                    return Image(img, QString());
                }
            }
            #endif

            QStringList files=QDir(dirName).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable);
            foreach (const QString &fileName, files) {
                QImage img(dirName+fileName);

                if (!img.isNull()) {
                    return Image(img, dirName+fileName);
                }
            }
        }
    }

    QString artist=encodeName(song.albumArtist());

    if (isArtistImage) {
        QString dir(Utils::cacheDir(constCoverDir, false));
        foreach (const QString &ext, constExtensions) {
            if (QFile::exists(dir+artist+ext)) {
                QImage img(dir+artist+ext);
                if (!img.isNull()) {
                    return Image(img, dir+artist+ext);
                }
            }
        }
    } else {
        QString album=encodeName(song.album);
        // Check if cover is already cached
        QString dir(Utils::cacheDir(constCoverDir+artist, false));
        foreach (const QString &ext, constExtensions) {
            if (QFile::exists(dir+album+ext)) {
                QImage img(dir+album+ext);
                if (!img.isNull()) {
                    return Image(img, dir+album+ext);
                }
            }
        }

        Job job(song, dirName);

        #if !defined Q_OS_WIN
        // See if amarok, or clementine, has it...
        AppCover app=otherAppCover(job);
        if (!app.img.isNull()) {
            return Image(app.img, app.filename);
        }
        #endif
    }

    return Image(QImage(), QString());
}

Covers::Image Covers::get(const Song &song)
{
    Image img=getImage(song);

    if (img.img.isNull()) {
        download(song);
    }
    return img;
}

void Covers::requestCover(const Song &song, bool urgent)
{
    if (!queueThread) {
        queue=new CoverQueue;
        queueThread=new QThread(this);
        queue->moveToThread(queueThread);
        connect(queue, SIGNAL(cover(const Song &, const QImage &, const QString &)), this, SIGNAL(cover(const Song &, const QImage &, const QString &)));
        connect(queue, SIGNAL(artistImage(const QString &, const QImage &)), this, SIGNAL(artistImage(const QString &, const QImage &)));
        connect(queue, SIGNAL(download(const Song &)), this, SLOT(download(const Song &)));
        queueThread->start();
    }
    queue->getCover(song, urgent);
}

CoverQueue::CoverQueue()
    : mutex(new QMutex)
{
    connect(this, SIGNAL(getNext()), this, SLOT(getNextCover()), Qt::QueuedConnection);
}

void CoverQueue::getCover(const Song &song, bool urgent)
{
    mutex->lock();
    bool added=false;
    int idx=songs.indexOf(song);
    if (urgent) {
        if  (-1!=idx) {
            songs.removeAt(idx);
        }
        songs.prepend(song);
        added=true;
    } else if (-1==idx) {
        songs.append(song);
        added=true;
    }
    mutex->unlock();

    if (added) {
        emit getNext();
    }
}

void CoverQueue::getNextCover()
{
    mutex->lock();
    if (songs.isEmpty()) {
        mutex->unlock();
        return;
    }
    Song song=songs.takeAt(0);
    mutex->unlock();
    Covers::Image img=Covers::self()->getImage(song);
    if (img.img.isNull()) {
        emit download(song);
    } else if (song.album.isEmpty() && song.artist.isEmpty() && !song.albumartist.isEmpty()) {
        emit artistImage(song.albumartist, img.img);
    } else {
        emit cover(song, img.img, img.fileName);
    }
}

void Covers::setSaveInMpdDir(bool s)
{
    saveInMpdDir=s;
}

void Covers::download(const Song &song)
{
    bool isArtistImage=isRequestingArtistImage(song);

    if (jobs.end()!=findJob(Job(song, QString(), isArtistImage))) {
        return;
    }
    bool haveAbsPath=song.file.startsWith('/');
    QString dirName;

    if (!isArtistImage && (haveAbsPath || !MPDConnection::self()->getDetails().dir.isEmpty())) {
        dirName=song.file.endsWith('/') ? (haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+song.file
                                        : Utils::getDir((haveAbsPath ? QString() : MPDConnection::self()->getDetails().dir)+song.file);
    }

    if (!manager) {
        manager=new NetworkAccessManager(this);
    }

    Job job(song, dirName, isArtistImage);

    if (!isArtistImage && !MPDConnection::self()->getDetails().dir.isEmpty() && MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http://"))) {
        downloadViaHttp(job, JobHttpJpg);
    } else {
        downloadViaLastFm(job);
    }
}

void Covers::downloadViaHttp(Job &job, JobType type)
{
    QUrl u;
    QString coverName=MPDConnection::self()->getDetails().coverName;
    if (coverName.isEmpty()) {
        coverName=constFileName;
    }
    coverName+=constExtensions.at(JobHttpJpg==type ? 0 : 1);
    u.setEncodedUrl(MPDConnection::self()->getDetails().dir.toLatin1()+QUrl::toPercentEncoding(Utils::getDir(job.song.file), "/")+coverName.toLatin1());

    job.type=type;
    QNetworkReply *j=manager->get(QNetworkRequest(u));
    connect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
    jobs.insert(j, job);
}

static QDomElement addArg(QDomDocument &doc, const QString &key, const QString &val)
{
    QDomElement member = doc.createElement("member");
    QDomElement name = doc.createElement("name");
    QDomElement value = doc.createElement("value");
    QDomElement str = doc.createElement("string");

    member.appendChild(name);
    name.appendChild(doc.createTextNode(key));
    member.appendChild(value);
    value.appendChild(str);
    str.appendChild(doc.createTextNode(val));

    return member;
}

void Covers::downloadViaLastFm(Job &job)
{
    // Query lastfm...
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", QString("version=\"1.0\" encoding=\"UTF-8\"")));
    QDomElement methodCall = doc.createElement("methodCall");
    QDomElement methodName = doc.createElement("methodName");
    QDomElement params = doc.createElement("params");
    QDomElement param = doc.createElement("param");
    QDomElement value = doc.createElement("value");
    QDomElement str = doc.createElement("struct");

    doc.appendChild(methodCall);
    methodCall.appendChild(methodName);
    methodName.appendChild(doc.createTextNode(job.isArtist ? "artist.getInfo" : "album.getInfo"));
    methodCall.appendChild(params);
    params.appendChild(param);
    param.appendChild(value);
    value.appendChild(str);

    str.appendChild(addArg(doc, "api_key", constApiKey));
    str.appendChild(addArg(doc, "artist", job.song.albumArtist()));
    if (!job.isArtist) {
        str.appendChild(addArg(doc, "album", job.song.album));
    }

    QNetworkRequest request;
    request.setUrl(QUrl("http://ws.audioscrobbler.com/2.0/"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
    QNetworkReply *j = manager->post(request, doc.toString().toUtf8());
    connect(j, SIGNAL(finished()), this, SLOT(lastFmCallFinished()));
    job.type=JobLastFm;
    jobs.insert(j, job);
}

void Covers::lastFmCallFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    QHash<QNetworkReply *, Job>::Iterator it(jobs.find(reply));
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        Job job=it.value();
        jobs.erase(it);
        QString url;

        if(QNetworkReply::NoError==reply->error()) {
            QString resp;
            QXmlStreamReader doc(QString::fromUtf8(reply->readAll()));
            int level=0;

            while (!doc.atEnd() && url.isEmpty()) {
                doc.readNext();

                if (doc.isStartElement()) {
                    ++level;
                    if ( (1==level && QLatin1String("methodResponse")!=doc.name()) ||
                         (2==level && QLatin1String("params")!=doc.name()) ||
                         (3==level && QLatin1String("param")!=doc.name()) ||
                         (4==level && QLatin1String("value")!=doc.name()) ||
                         (5==level && QLatin1String("string")!=doc.name()) ) {
                        break;
                    } else if (5==level) {
                        resp = doc.readElementText();
                        break;
                    }
                } else if (doc.isEndElement()) {
                    break;
                }
            }

            if (!resp.isEmpty()) {
                bool inSection=false;
                bool isArtistImage=job.isArtist;
                QXmlStreamReader doc(resp.replace("\\\"", "\""));

                while (!doc.atEnd() && url.isEmpty()) {
                    doc.readNext();

                    if (doc.isStartElement()) {
                        if (!inSection && QLatin1String(isArtistImage ? "artist" : "album")==doc.name()) {
                            inSection=true;
                        } else if (inSection && QLatin1String("image")==doc.name() && QLatin1String("extralarge")==doc.attributes().value("size").toString()) {
                            url = doc.readElementText();
                        } else if (QLatin1String("similar")==doc.name()) {
                            break;
                        }
                    } else if (doc.isEndElement() && inSection && QLatin1String(isArtistImage ? "artist" : "album")==doc.name()) {
                        inSection=false;
                    }
                }
            }
        }

        if (!url.isEmpty()) {
            QUrl u;
            u.setEncodedUrl(url.toLatin1());
            QNetworkReply *j=manager->get(QNetworkRequest(u));
            connect(j, SIGNAL(finished()), this, SLOT(jobFinished()));
            jobs.insert(j, job);
        } else {
            if (job.isArtist) {
                emit artistImage(job.song.albumartist, QImage());
            } else {
                #if defined Q_OS_WIN
                emit cover(job.song, QImage(), QString());
                #else
                AppCover app=otherAppCover(job);
                emit cover(job.song, app.img, app.filename);
                #endif
            }
        }
    }
    reply->deleteLater();
}

void Covers::jobFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    QHash<QNetworkReply *, Job>::Iterator it(jobs.find(reply));
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        QByteArray data=QNetworkReply::NoError==reply->error() ? reply->readAll() : QByteArray();
        QString url=reply->url().toString();
        QImage img = data.isEmpty() ? QImage() : QImage::fromData(data, url.endsWith(".jpg") ? "JPG" : (url.endsWith(".png") ? "PNG" : 0));
        QString fileName;
        Job job=it.value();

        if (!img.isNull() && img.size().width()<32) {
            img = QImage();
        }

        jobs.remove(it.key());
        if (img.isNull() && JobLastFm!=job.type) {
            if (JobHttpJpg==job.type) {
                downloadViaHttp(job, JobHttpPng);
            } else {
                downloadViaLastFm(job);
            }
        } else {
            if (!img.isNull()) {
                if (img.size().width()>constMaxSize.width() || img.size().height()>constMaxSize.height()) {
                    img=img.scaled(constMaxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                fileName=saveImg(job, img, data);
                clearDummyCache(job.song, img);
            }

            if (job.isArtist) {
                emit artistImage(job.song.albumartist, img);
            } else if (img.isNull()) {
                #if defined Q_OS_WIN
                emit cover(job.song, QImage(), QString());
                #else
                AppCover app=otherAppCover(job);
                emit cover(job.song, app.img, app.filename);
                #endif
            } else {
                emit cover(job.song, img, fileName);
            }
        }
    }

    reply->deleteLater();
}

QString Covers::saveImg(const Job &job, const QImage &img, const QByteArray &raw)
{
    QString mimeType=typeFromRaw(raw);
    QString extension=mimeType.isEmpty() ? constExtensions.at(0) : mimeType;
    QString savedName;

    if (job.isArtist) {
        QString dir = Utils::cacheDir(constCoverDir);
        if (!dir.isEmpty()) {
            savedName=save(mimeType, extension, dir+encodeName(job.song.albumartist), img, raw);
            if (!savedName.isEmpty()) {
                return savedName;
            }
        }
    } else {
        // Try to save as cover.jpg in album dir...
        if (saveInMpdDir && canSaveTo(job.dir)) {
            QString coverName=MPDConnection::self()->getDetails().coverName;
            if (coverName.isEmpty()) {
                coverName=constFileName;
            }
            savedName=save(mimeType, extension, job.dir+coverName, img, raw);
            if (!savedName.isEmpty()) {
                return savedName;
            }
        }

        // Could not save with album, save in cache dir...
        QString dir = Utils::cacheDir(constCoverDir+encodeName(job.song.albumArtist()));
        if (!dir.isEmpty()) {
            savedName=save(mimeType, extension, dir+encodeName(job.song.album), img, raw);
            if (!savedName.isEmpty()) {
                return savedName;
            }
        }
    }

    return QString();
}

QHash<QNetworkReply *, Covers::Job>::Iterator Covers::findJob(const Job &job)
{
    QHash<QNetworkReply *, Job>::Iterator it(jobs.begin());
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    for (; it!=end; ++it) {
        if ((*it).isArtist==job.isArtist && (*it).song.albumArtist()==job.song.albumArtist() && (*it).song.album==job.song.album) {
            return it;
        }
    }

    return end;
}
