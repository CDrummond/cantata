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
#include "mpdparseutils.h"
#include "maiaXmlRpcClient.h"
#include "networkaccessmanager.h"
#include "network.h"
#include "settings.h"
#include <QtCore/QFile>
#include <QtCore/QCryptographicHash>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtNetwork/QNetworkReply>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QFont>

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardDirs>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(Covers, instance)
#endif

static const QLatin1String constApiKey("11172d35eb8cc2fd33250a9e45a2d486");
static const QLatin1String constCoverDir("covers/");
static const QLatin1String constFileName("cover");
static const QStringList   constExtensions=QStringList() << ".jpg" << ".png";
static const QLatin1String constSingleTracksFile("SingleTracks.png");

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

static bool canSaveTo(const QString &dir)
{
    return !dir.isEmpty() && !Settings::self()->mpdDir().isEmpty() && QDir(Settings::self()->mpdDir()).exists() && dir.startsWith(Settings::self()->mpdDir());
}

static QImage createSingleTracksCover(const QString &fileName)
{
    static const int constSize=128;
    QImage img(constSize, constSize, QImage::Format_ARGB32);
    QPixmap pix(QIcon::fromTheme("media-optical").pixmap(128, 128));
    QPainter p(&img);
    p.fillRect(QRect(0, 0, constSize, constSize), Qt::transparent);
    p.drawPixmap((constSize-pix.width())/2,(constSize-pix.height())/2, pix);
    QFont font(QLatin1String("serif"));
    font.setBold(true);
    font.setItalic(true);
    font.setPixelSize(constSize*0.8);
    p.setFont(font);
    p.setPen(Qt::black);
    p.drawText(QRect(0, 0, constSize, constSize), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.end();
    img.save(fileName);
    return img;
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
    if (!mimeType.isEmpty()) {
        if (QFile::exists(filePrefix+mimeType)) {
            return filePrefix+mimeType;
        }

        QFile f(filePrefix+mimeType);
        if (f.open(QIODevice::WriteOnly) && raw.size()==f.write(raw)) {
            if (!Settings::self()->mpdDir().isEmpty() && filePrefix.startsWith(Settings::self()->mpdDir())) {
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
            if (!Settings::self()->mpdDir().isEmpty() && filePrefix.startsWith(Settings::self()->mpdDir())) {
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
                 QCryptographicHash::hash(job.artist.toLower().toLocal8Bit()+job.album.toLower().toLocal8Bit(),
                                          QCryptographicHash::Md5).toHex();

    app.img=QImage(app.filename);

    if (app.img.isNull()) {
        app.filename=xdgConfig()+QLatin1String("/Clementine/albumcovers/")+
                     QCryptographicHash::hash(job.artist.toLower().toUtf8()+job.album.toLower().toUtf8(),
                                              QCryptographicHash::Sha1).toHex()+QLatin1String(".jpg");

        app.img=QImage(app.filename);
    }

    if (!app.img.isNull() && saveInMpdDir && canSaveTo(job.dir)) {
        QFile f(app.filename);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray raw=f.readAll();
            if (!raw.isEmpty()) {
                QString mimeType=typeFromRaw(raw);
                QString saveName=save(mimeType, mimeType.isEmpty() ? constExtensions.at(0) : mimeType, job.dir+constFileName, app.img, raw);
                if (!saveName.isEmpty()) {
                    app.filename=saveName;
                }
            }
        }
    }
    return app;
}

Covers * Covers::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Covers *instance=0;;
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

void Covers::copyCover(const Song &song, const QString &sourceDir, const QString &destDir, const QString &name)
{
    initCoverNames();

    // First, check if dir already has a cover file!
    foreach (const QString &coverFile, coverFileNames) {
        if (QFile::exists(destDir+coverFile)) {
            return;
        }
    }

    // No cover found, try to copy from source folder
    foreach (const QString &coverFile, coverFileNames) {
        if (QFile::exists(sourceDir+coverFile)) {
            QString destName(name);
            if (destName.isEmpty()) { // copying into mpd dir, so we want cover.jpg/png...
                if (coverFileNames.at(0)!=coverFile) { // source is not 'cover.xxx'
                    QString ext(coverFile.endsWith(constExtensions.at(0)) ? constExtensions.at(0) : constExtensions.at(1));
                    destName=constFileName+ext;
                } else {
                    destName=coverFile;
                }
            }
            // Diff extensions, so need to convert image type...
            if (destName.right(4)!=coverFile.right(4)) {
                QImage(sourceDir+coverFile).save(destDir+destName);
            } else {
                QFile::copy(sourceDir+coverFile, destDir+destName);
            }
            Utils::setFilePerms(destDir+destName);
            return;
        }
    }

    // None in source folder. Do we have a cached cover?
    QString artist=encodeName(song.albumArtist());
    QString album=encodeName(song.album);
    QString dir(Network::cacheDir(constCoverDir+artist, false));
    foreach (const QString &ext, constExtensions) {
        if (QFile::exists(dir+album+ext)) {
            QFile::copy(dir+album+ext, destDir+constFileName+ext);
            Utils::setFilePerms(destDir+constFileName+ext);
            return;
        }
    }
}

QStringList Covers::standardNames()
{
    initCoverNames();
    return coverFileNames;
}

Covers::Covers()
    : rpc(0)
    , manager(0)
    , cache(150000)
    , queue(0)
    , queueThread(0)
{
    saveInMpdDir=Settings::self()->storeCoversInMpdDir();
}

void Covers::stop()
{
    if (queueThread) {
        disconnect(queue, SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)), this, SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)));
        disconnect(queue, SIGNAL(download(const Song &)), this, SLOT(download(const Song &)));
        Utils::stopThread(queueThread);
    }
}

static inline QString cacheKey(const QString &artist, const QString &album, int size)
{
    return artist+QLatin1String(" - ")+album+QChar(':')+QString::number(size);
}

static QSet<int> cacheSizes;

QPixmap * Covers::get(const Song &song, int size, bool isSingleTracks)
{
    if (song.isUnknown()) {
        return 0;
    }

    QString key=cacheKey(song.albumArtist(), song.album, size);
    QPixmap *pix(cache.object(key));

    if (!pix) {
        Covers::Image img=getImage(song, isSingleTracks);

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
void Covers::clearDummyCache(const QString &artist, const QString &album)
{
    bool hadDummy=false;
    foreach (int s, cacheSizes) {
        QString key=cacheKey(artist, album, s);
        QPixmap *pix(cache.object(key));

        if (pix && pix->width()<2) {
            cache.remove(key);
            hadDummy=true;
        }
    }

    if (hadDummy) {
        emit coverRetrieved(artist, album);
    }
}

Covers::Image Covers::getImage(const Song &song, bool isSingleTracks)
{
    if (isSingleTracks) {
         QString fileName(Network::cacheDir(constCoverDir)+constSingleTracksFile);
         if (QFile::exists(fileName)) {
             return Image(QImage(fileName), fileName);
         } else {
             return Image(createSingleTracksCover(fileName), fileName);
         }
    }

    QString dirName;
    bool haveAbsPath=song.file.startsWith('/');

    if (haveAbsPath || !Settings::self()->mpdDir().isEmpty()) {
        dirName=song.file.endsWith('/') ? (haveAbsPath ? QString() : Settings::self()->mpdDir())+song.file
                                        : MPDParseUtils::getDir((haveAbsPath ? QString() : Settings::self()->mpdDir())+song.file);
        initCoverNames();
        foreach (const QString &fileName, coverFileNames) {
            if (QFile::exists(dirName+fileName)) {
                QImage img(dirName+fileName);

                if (!img.isNull()) {
                    return Image(img, dirName+fileName);
                }
            }
        }
        QStringList files=QDir(dirName).entryList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.png"), QDir::Files|QDir::Readable);
        foreach (const QString &fileName, files) {
            QImage img(dirName+fileName);

            if (!img.isNull()) {
                return Image(img, dirName+fileName);
            }
        }
    }

    QString artist=encodeName(song.albumArtist());
    QString album=encodeName(song.album);
    // Check if cover is already cached
    QString dir(Network::cacheDir(constCoverDir+artist, false));
    foreach (const QString &ext, constExtensions) {
        if (QFile::exists(dir+album+ext)) {
            QImage img(dir+album+ext);
            if (!img.isNull()) {
                return Image(img, dir+album+ext);
            }
        }
    }

    Job job(song.albumArtist(), song.album, dirName);

    // See if amarok, or clementine, has it...
    AppCover app=otherAppCover(job);
    if (!app.img.isNull()) {
        return Image(app.img, app.filename);
    }

    return Image(QImage(), QString());
}

Covers::Image Covers::get(const Song &song, bool isSingleTracks)
{
    Image img=getImage(song, isSingleTracks);

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
        connect(queue, SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)), this, SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)));
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
    Covers::Image img=Covers::self()->getImage(song, false);
    if (img.img.isNull()) {
        emit download(song);
    } else {
        emit cover(song.albumArtist(), song.album, img.img, img.fileName);
    }
}

void Covers::setSaveInMpdDir(bool s)
{
    saveInMpdDir=s;
}

void Covers::download(const Song &song)
{
    if (jobs.end()!=findJob(song)) {
        return;
    }

    bool haveAbsPath=song.file.startsWith('/');
    QString dirName;

    if (haveAbsPath || !Settings::self()->mpdDir().isEmpty()) {
        dirName=song.file.endsWith('/') ? (haveAbsPath ? QString() : Settings::self()->mpdDir())+song.file
                                        : MPDParseUtils::getDir((haveAbsPath ? QString() : Settings::self()->mpdDir())+song.file);
    }

    Job job(song.albumArtist(), song.album, dirName);

    // Query lastfm...
    if (!rpc) {
        rpc = new MaiaXmlRpcClient(QUrl("http://ws.audioscrobbler.com/2.0/"));
    }

    if (!manager) {
        manager=new NetworkAccessManager(this);
        connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(jobFinished(QNetworkReply *)));
    }

    QMap<QString, QVariant> args;

    args["artist"] = song.albumArtist();
    args["album"] = song.album;
    args["api_key"] = constApiKey;
    QNetworkReply *reply=rpc->call("album.getInfo", QVariantList() << args,
                                   this, SLOT(albumInfo(QVariant &, QNetworkReply *)),
                                   this, SLOT(albumFailure(int, const QString &, QNetworkReply *)));
    jobs.insert(reply, job);
}

void Covers::albumInfo(QVariant &value, QNetworkReply *reply)
{
    QHash<QNetworkReply *, Job>::Iterator it(jobs.find(reply));
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        QString xmldoc = value.toString();
        xmldoc.replace("\\\"", "\"");

        QXmlStreamReader doc(xmldoc);
        QString url;
        Job job=it.value();

        while (!doc.atEnd() && url.isEmpty()) {
            doc.readNext();
            if (url.isEmpty() && doc.isStartElement() &&
                QLatin1String("image")==doc.name() && QLatin1String("extralarge")==doc.attributes().value("size").toString()) {
                url = doc.readElementText();
            }
        }

        if (!url.isEmpty()) {
            QUrl u;
            u.setEncodedUrl(url.toLatin1());
            jobs.remove(it.key());
            jobs.insert(manager->get(QNetworkRequest(u)), job);
        } else {
            emit cover(job.artist, job.album, QImage(), QString());
        }
    }
    reply->deleteLater();
}

void Covers::albumFailure(int, const QString &, QNetworkReply *reply)
{
    QHash<QNetworkReply *, Job>::Iterator it(jobs.find(reply));
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        Job job=it.value();
        AppCover app=otherAppCover(job);
        emit cover(job.artist, job.album, app.img, app.filename);
        jobs.remove(it.key());
    }

    reply->deleteLater();
}

void Covers::jobFinished(QNetworkReply *reply)
{
    QHash<QNetworkReply *, Job>::Iterator it(jobs.find(reply));
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    if (it!=end) {
        QByteArray data=QNetworkReply::NoError==reply->error() ? reply->readAll() : QByteArray();
        QImage img = data.isEmpty() ? QImage() : QImage::fromData(data);
        QString fileName;
        Job job=it.value();

        if (!img.isNull() && img.size().width()<32) {
            img = QImage();
        }

        if (!img.isNull()) {
            fileName=saveImg(job, img, data);
        }

        jobs.remove(it.key());

        if (img.isNull()) {
            AppCover app=otherAppCover(job);
            emit cover(job.artist, job.album, app.img, app.filename);
        } else {
            emit cover(job.artist, job.album, img, fileName);
        }
    }

    reply->deleteLater();
}

QString Covers::saveImg(const Job &job, const QImage &img, const QByteArray &raw)
{
    QString mimeType=typeFromRaw(raw);
    QString extension=mimeType.isEmpty() ? constExtensions.at(0) : mimeType;
    QString savedName;

    // Try to save as cover.jpg in album dir...
    if (saveInMpdDir && canSaveTo(job.dir)) {
        savedName=save(mimeType, extension, job.dir+constFileName, img, raw);
        if (!savedName.isEmpty()) {
            clearDummyCache(job.artist, job.album);
            return savedName;
        }
    }

    // Could not save with album, save in cache dir...
    QString dir = Network::cacheDir(constCoverDir+encodeName(job.artist));
    if (!dir.isEmpty()) {
        savedName=save(mimeType, extension, dir+encodeName(job.album), img, raw);
        if (!savedName.isEmpty()) {
            clearDummyCache(job.artist, job.album);
            return savedName;
        }
    }

    return QString();
}

QHash<QNetworkReply *, Covers::Job>::Iterator Covers::findJob(const Song &song)
{
    QHash<QNetworkReply *, Job>::Iterator it(jobs.begin());
    QHash<QNetworkReply *, Job>::Iterator end(jobs.end());

    for (; it!=end; ++it) {
        if ((*it).artist==song.albumArtist() && (*it).album==song.album) {
            return it;
        }
    }

    return end;
}
