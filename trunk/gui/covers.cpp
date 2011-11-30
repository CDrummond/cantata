#include "covers.h"
#include "musiclibrarymodel.h"
#include "lib/song.h"
#include "maiaXmlRpcClient.h"
#include "networkaccessmanager.h"
#include <QtCore/QFile>
#include <QtNetwork/QNetworkReply>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMD5>
#include <KDE/KStandardDirs>
#include <KDE/KGlobal>
K_GLOBAL_STATIC(Covers, instance)
#else
#include <QtCore/QCryptographicHash>
#endif

static const QLatin1String constApiKey("11172d35eb8cc2fd33250a9e45a2d486");
static const QLatin1String constCoverDir("covers/");
static const QLatin1String constKeySep("<<##>>");
static const QLatin1String constExtension(".jpg");

static QString encodeName(QString name)
{
    name.replace("/", "_");
    return name;
}

static QImage amarokCover(const QString &artist, const QString &album)
{
#ifdef ENABLE_KDE_SUPPORT
    QString amarokName=KGlobal::dirs()->localkdedir()+"/share/apps/amarok/albumcovers/large/"+
                       KMD5(artist.toLower().toLocal8Bit() + album.toLower().toLocal8Bit()).hexDigest();
#else
    QString amarokName=QDir::homePath+"/.kde/share/apps/amarok/albumcovers/large/"+
                       QCryptographicHash::hash(artist.toLower().toLocal8Bit()+album.toLower().toLocal8Bit(),
                                                QCryptographicHash::Md5).toHex();
#endif
    QImage img(amarokName);

    if (!img.isNull()) {
        QString dir = MusicLibraryModel::cacheDir(constCoverDir+encodeName(artist)+'/');
        if (!dir.isEmpty()) {
            QFile::copy(amarokName, QFile::encodeName(encodeName(album)+constExtension));
            //QString file(QFile::encodeName(album+constExtension));
            //img.save(dir+file);
        }
    }
    return img;
}

static QString getDir(const QString &f)
{
    QString d(f);

    int slashPos(d.lastIndexOf('/'));

    if(slashPos!=-1)
        d.remove(slashPos+1, d.length());

    if (!d.isEmpty() && !d.endsWith("/")) {
        d=d+"/";
    }

    return d;
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

Covers::Covers()
    : rpc(0)
    , manager(0)
{
}

void Covers::get(const Song &song)
{
    if (!mpdDir.isEmpty()) {
        QString dirName(getDir(mpdDir+song.file));
        QStringList fileNames;
        fileNames << "cover"+constExtension << "album"+constExtension;
        foreach(const QString &file, fileNames) {
            if (QFile::exists(QFile::encodeName(dirName+file))) {
                QImage img(dirName+file);

                if (!img.isNull()) {
                    emit cover(song.albumArtist(), song.album, img);
                    return;
                }
            }
        }
    }

    QString artist=encodeName(song.albumArtist());
    QString album=encodeName(song.album);
    // Check if cover is already cached
    QString dir(MusicLibraryModel::cacheDir(constCoverDir+artist+'/', false));
    QString file(QFile::encodeName(album+constExtension));
    if (QFile::exists(dir+file)) {
        QImage img(dir+file);
        if (!img.isNull()) {
            emit cover(artist, album, img);
            return;
        }
    }


    QString key=artist+constKeySep+album;
    if (jobs.contains(key)) {
        return;
    }

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
    jobs.insert(key, reply);
}

void Covers::albumInfo(QVariant &value, QNetworkReply *reply)
{
    QMap<QString, QNetworkReply *>::Iterator it(findJob(reply));
    QMap<QString, QNetworkReply *>::Iterator end(jobs.end());

    if (it!=end) {
        QString xmldoc = value.toString();
        xmldoc.replace("\\\"", "\"");

        QXmlStreamReader doc(xmldoc);
        QString url;

        while (!doc.atEnd() && url.isEmpty()) {
            doc.readNext();
            if (url.isEmpty() && doc.isStartElement() && doc.name() == "image" && doc.attributes().value("size").toString() == "extralarge") {
                url = doc.readElementText();
            }
        }

        if (!url.isEmpty()) {
            QUrl u;
            u.setEncodedUrl(url.toLatin1());
            it.value()=manager->get(QNetworkRequest(u));
        } else {
            QStringList parts = it.key().split(constKeySep);
            emit cover(parts[0], parts[1], QImage());
        }
    }
    reply->deleteLater();
}

void Covers::albumFailure(int, const QString &, QNetworkReply *reply)
{
    QMap<QString, QNetworkReply *>::Iterator it(findJob(reply));
    QMap<QString, QNetworkReply *>::Iterator end(jobs.end());

    if (it!=end) {
        QStringList parts = it.key().split(constKeySep);
        emit cover(parts[0], parts[1], amarokCover(parts[0], parts[1]));
        jobs.remove(it.key());
    }

    reply->deleteLater();
}

void Covers::jobFinished(QNetworkReply *reply)
{
    QMap<QString, QNetworkReply *>::Iterator it(findJob(reply));
    QMap<QString, QNetworkReply *>::Iterator end(jobs.end());

    if (it!=end) {
        QImage img = QNetworkReply::NoError==reply->error() ? QImage::fromData(reply->readAll()) : QImage();
        QStringList parts = it.key().split(constKeySep);

        if (!img.isNull() && img.size().width()<32) {
            img = QImage();
        }

        if (!img.isNull()) {
            QString dir = MusicLibraryModel::cacheDir(constCoverDir+encodeName(parts[0])+'/');
            if (!dir.isEmpty()) {
                QString file(QFile::encodeName(encodeName(parts[1])+constExtension));
                img.save(dir+file);
            }
        }
        jobs.remove(it.key());

        if (img.isNull()) {
            emit cover(parts[0], parts[1], amarokCover(parts[0], parts[1]));
        } else {
            emit cover(parts[0], parts[1], img);
        }
    }

    reply->deleteLater();
}

QMap<QString, QNetworkReply*>::Iterator Covers::findJob(QNetworkReply *reply)
{
    QMap<QString, QNetworkReply *>::Iterator it(jobs.begin());
    QMap<QString, QNetworkReply *>::Iterator end(jobs.end());

    for (; it!=end; ++it) {
        if (it.value()==reply) {
            return it;
        }
    }

    return end;
}
