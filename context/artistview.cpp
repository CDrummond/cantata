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

#include "artistview.h"
#include "localize.h"
#include "covers.h"
#include "utils.h"
#include "musiclibrarymodel.h"
#include "networkaccessmanager.h"
#include "qtiocompressor/qtiocompressor.h"
#include "textbrowser.h"
#include "contextengine.h"
#include "actioncollection.h"
#include "musiclibrarymodel.h"
#include <QNetworkReply>
#include <QApplication>
#include <QTextStream>
#include <QLayout>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#ifndef Q_OS_WIN
#include <QProcess>
#include <QXmlStreamReader>
#endif
#include <QPixmap>
#include <QFile>
#include <QMenu>
#include <QTimer>

static const char *constNameKey="name";

const int ArtistView::constCacheAge=0; // 0 => dont automatically clean cache
const QLatin1String ArtistView::constCacheDir("artists/");
const QLatin1String ArtistView::constInfoExt(".html.gz");
const QLatin1String ArtistView::constSimilarInfoExt(".txt");

static QString cacheFileName(const QString &artist, const QString &lang, bool similar, bool createDir)
{
    return Utils::cacheDir(ArtistView::constCacheDir, createDir)+
            Covers::encodeName(artist)+(similar ? "-similar" : ("."+lang))+(similar ? ArtistView::constSimilarInfoExt : ArtistView::constInfoExt);
}

static QString buildUrl(const QString &artist, const QString &album=QString())
{
    QUrl url("cantata:///");
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("artist", artist);
    if (!album.isEmpty()) {
        query.addQueryItem("album", album);
    }
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif
    return url.toString();
}

ArtistView::ArtistView(QWidget *parent)
    : View(parent)
    , currentSimilarJob(0)
{
    engine=ContextEngine::create(this);
    refreshAction = ActionCollection::get()->createAction("refreshartist", i18n("Refresh Artist Information"), "view-refresh");
    connect(refreshAction, SIGNAL(triggered()), SLOT(refresh()));
    connect(engine, SIGNAL(searchResult(QString,QString)), this, SLOT(searchResponse(QString,QString)));
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), SLOT(artistImage(Song,QImage,QString)));
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(show(QUrl)));
    text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(text, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    setStandardHeader(i18n("Artist Information"));
   
    int imageHeight=fontMetrics().height()*14;
    int imageWidth=imageHeight*1.5;
    setPicSize(QSize(imageWidth, imageHeight));
    clear();
    if (constCacheAge>0) {
        clearCache();
        QTimer *timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(clearCache()));
        timer->start((int)((constCacheAge/2.0)*1000*24*60*60));
    }
}

void ArtistView::showContextMenu(const QPoint &pos)
{
   QMenu *menu = text->createStandardContextMenu();
   menu->addSeparator();
   menu->addAction(refreshAction);
   menu->exec(text->mapToGlobal(pos));
   delete menu;
}

void ArtistView::refresh()
{
    if (currentSong.isEmpty()) {
        return;
    }
    foreach (const QString &lang, engine->getLangs()) {
        QFile::remove(cacheFileName(currentSong.artist, engine->getPrefix(lang), false, false));
    }
    QFile::remove(cacheFileName(currentSong.artist, QString(), true, false));
    update(currentSong, true);
}

void ArtistView::update(const Song &s, bool force)
{
    if (s.isEmpty()) {
        currentSong=s;
        engine->cancel();
        clear();
        return;
    }

    Song song=s;
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }

    song.artist=song.basicArtist();
    bool artistChanged=song.artist!=currentSong.artist;

    if (!isVisible()) {
        if (artistChanged) {
            needToUpdate=true;
        }
        currentSong=song;
        return;
    }

    if (artistChanged || force) {
        currentSong=song;
        clear();
        pic.clear();
        biography.clear();
        albums.clear();
        similarArtists=QString();
        if (!currentSong.isEmpty()) {
            setHeader(currentSong.artist);

            Song s;
            s.albumartist=currentSong.artist;
            s.file=currentSong.file;
            Covers::Image img=Covers::self()->requestImage(s);
            if (!img.img.isNull()) {
                pic=createPicTag(img.img, img.fileName);
            }
            loadBio();
        }
    }
}

void ArtistView::artistImage(const Song &song, const QImage &i, const QString &f)
{
    Q_UNUSED(f)
    if (song.albumartist==currentSong.artist && pic.isEmpty()) {
        pic=createPicTag(i, f);
        setBio();
    }
}

void ArtistView::loadBio()
{
    foreach (const QString &lang, engine->getLangs()) {
        QString prefix=engine->getPrefix(lang);
        QString cachedFile=cacheFileName(currentSong.artist, prefix, false, false);
        if (QFile::exists(cachedFile)) {
            QFile f(cachedFile);
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::ReadOnly)) {
                QString data=QString::fromUtf8(compressor.readAll());

                if (!data.isEmpty()) {
                    searchResponse(data, QString());
                    Utils::touchFile(cachedFile);
                    return;
                }
            }
        }
    }

    showSpinner();
    engine->search(QStringList() << currentSong.artist, ContextEngine::Artist);
}

void ArtistView::loadSimilar()
{
    QString cachedFile=cacheFileName(currentSong.artist, QString(), true, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            QStringList artists;
            while (!f.atEnd()) {
                QString artist=QString::fromUtf8(f.readLine());
                if (!artist.isEmpty()) {
                    artists.append(artist.trimmed());
                }
            }

            if (!artists.isEmpty()) {
                buildSimilar(artists);
                setBio();
                Utils::touchFile(cachedFile);
                return;
            }
        }
    }

    requestSimilar();
}

void ArtistView::handleSimilarReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (reply==currentSimilarJob) {
        if (QNetworkReply::NoError==reply->error()) {
            QByteArray data=reply->readAll();
            QStringList artists=parseSimilarResponse(data);
            if (!artists.isEmpty()) {
                buildSimilar(artists);
                setBio();
                QFile f(cacheFileName(reply->property(constNameKey).toString(), QString(), true, true));
                if (f.open(QIODevice::WriteOnly|QIODevice::Text)) {
                    QTextStream stream(&f);
                    foreach (const QString &artist, artists) {
                        stream << artist << endl;
                    }
                }
            }
        }
        reply->deleteLater();
        currentSimilarJob=0;
    }
}

void ArtistView::setBio()
{
    QString html=pic+"<br>"+biography;
    if (!similarArtists.isEmpty()) {
        html+=similarArtists;
    }

    if (albums.isEmpty()) {
        QMap<QString, QStringList> a=MusicLibraryModel::self()->getAlbums(currentSong);
        QMap<QString, QStringList>::ConstIterator it=a.begin();
        QMap<QString, QStringList>::ConstIterator c=a.find(currentSong.artist);
        QMap<QString, QStringList>::ConstIterator end=a.end();

        if (c!=end) {
            QStringList artistAlbums=c.value();
            qSort(artistAlbums);
            foreach (const QString &album, artistAlbums) {
                albums+=QLatin1String("<li><a href=\"")+buildUrl(currentSong.albumArtist(), album)+"\">"+album+"</a></li>";
            }
        }
        for (; it!=end; ++it) {
            if (it==c) {
                continue;
            }
            QStringList artistAlbums=it.value();
            qSort(artistAlbums);
            foreach (const QString &album, artistAlbums) {
                albums+=QLatin1String("<li><a href=\"")+buildUrl(it.key(), album)+"\">"+it.key()+" - "+album+"</a></li>";
            }
        }
    }

    if (!albums.isEmpty()) {
        html+=View::subHeader(i18n("Albums"))+QLatin1String("<ul>")+albums+QLatin1String("</ul>");
    }

    if (webLinks.isEmpty()) {
        QFile file(":weblinks.xml");
        if (file.open(QIODevice::ReadOnly)) {
            QXmlStreamReader reader(&file);
            while (!reader.atEnd()) {
                reader.readNext();
                if (QLatin1String("link")==reader.name()) {
                    QXmlStreamAttributes attributes = reader.attributes();
                    QString url=attributes.value("url").toString();
                    QString name=attributes.value("name").toString();

                    if (!url.isEmpty() && !name.isEmpty()) {
                        webLinks+=QLatin1String("<li><a href=\"")+url+"\">"+name+"</a></li>";
                    }
                }
            }
        }
    }

    if (!webLinks.isEmpty()) {
        html+=View::subHeader(i18n("Web Links"))+QLatin1String("<ul>")+QString(webLinks).replace("${artist}", currentSong.artist)+QLatin1String("</ul>");
    }

    setHtml(html);
}

void ArtistView::requestSimilar()
{
    abort();
    QUrl url("http://ws.audioscrobbler.com/2.0/");
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("method", "artist.getSimilar");
    query.addQueryItem("api_key", Covers::constLastFmApiKey);
    query.addQueryItem("autocorrect", "1");
    query.addQueryItem("artist", Covers::fixArtist(currentSong.artist));
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    currentSimilarJob=NetworkAccessManager::self()->get(url);
    currentSimilarJob->setProperty(constNameKey, currentSong.artist);
    connect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarReply()));
}

void ArtistView::abort()
{
    engine->cancel();
    if (currentSimilarJob) {
        disconnect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarReply()));
        currentSimilarJob->abort();
        currentSimilarJob=0;
    }
}

void ArtistView::searchResponse(const QString &resp, const QString &lang)
{
    biography=engine->translateLinks(resp);
    emit haveBio(currentSong.artist, resp);
    hideSpinner();

    if (!resp.isEmpty()) {
        if (!lang.isEmpty()) {
            QFile f(cacheFileName(currentSong.artist, lang, false, false));
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(resp.toUtf8().constData());
            }
        }
        loadSimilar();
        setBio();
    }
}

QStringList ArtistView::parseSimilarResponse(const QByteArray &resp)
{
    QStringList artists;
    QXmlStreamReader doc(resp);
    bool inSection=false;

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            if (!inSection && QLatin1String("artist")==doc.name()) {
                inSection=true;
            } else if (inSection && QLatin1String("name")==doc.name()) {
                artists.append(doc.readElementText());
            }
        } else if (doc.isEndElement() && inSection && QLatin1String("artist")==doc.name()) {
            inSection=false;
        }
    }

    qSort(artists);
    return artists;
}

void ArtistView::buildSimilar(const QStringList &artists)
{
    QSet<QString> mpdArtists=MusicLibraryModel::self()->getAlbumArtists();
    bool first=true;
    foreach (QString artist, artists) {
        if (similarArtists.isEmpty()) {
            similarArtists=QLatin1String("<br/>")+View::subHeader(i18n("Similar Artists"));
        }
        if (mpdArtists.contains(artist)) {
            artist=QLatin1String("<a href=\"")+buildUrl(artist)+QLatin1String("\">")+artist+QLatin1String("</a>");
        } else {
            // Check for AC/DC -> AC-DC
            QString mod=artist;
            mod=mod.replace("/", "-");
            if (mod!=artist && mpdArtists.contains(mod)) {
                artist=QLatin1String("<a href=\"")+buildUrl(mod)+QLatin1String("\">")+artist+QLatin1String("</a>");
            }
        }
        if (first) {
            first=false;
        } else {
            similarArtists+=", ";
        }
        similarArtists+=artist;
    }

    if (!similarArtists.isEmpty()) {
        similarArtists+="<br>";
    }
}

void ArtistView::show(const QUrl &url)
{
    if (QLatin1String("cantata")==url.scheme()) {
        #if QT_VERSION < 0x050000
        const QUrl &q=url;
        #else
        QUrlQuery q(url);
        #endif

        if (q.hasQueryItem("artist")) {
            if (q.hasQueryItem("album")) {
                emit findAlbum(q.queryItemValue("artist"), q.queryItemValue("album"));
            } else {
                emit findArtist(q.queryItemValue("artist"));
            }
        }
    } else {
        #ifdef Q_OS_WIN
        QProcess::startDetached(QLatin1String("cmd"), QStringList() << QLatin1String("/c") << QLatin1String("start") << url.toString());
        #else
        QProcess::startDetached(QLatin1String("xdg-open"), QStringList() << url.toString());
        #endif
    }
}

void ArtistView::clearCache()
{
    Utils::clearOldCache(constCacheDir, constCacheAge);
}
