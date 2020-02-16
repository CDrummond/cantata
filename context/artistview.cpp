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

#include "artistview.h"
#include "gui/covers.h"
#include "gui/apikeys.h"
#include "support/utils.h"
#include "network/networkaccessmanager.h"
#include "qtiocompressor/qtiocompressor.h"
#include "widgets/textbrowser.h"
#include "widgets/icons.h"
#include "contextengine.h"
#include "support/actioncollection.h"
#include "support/action.h"
#include "models/mpdlibrarymodel.h"
#include <QApplication>
#include <QTextStream>
#include <QLayout>
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QPixmap>
#include <QFile>
#include <QMenu>
#include <QTimer>
#include <QDesktopServices>
#include <algorithm>

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

static QString buildUrl(const LibraryDb::Album &al)
{
    QUrl url("cantata:///");
    QUrlQuery query;

    query.addQueryItem("artist", al.artist);
    query.addQueryItem("albumId", al.id);
    url.setQuery(query);
    return url.toString();
}

static QString buildUrl(const QString &artist)
{
    QUrl url("cantata:///");
    QUrlQuery query;

    query.addQueryItem("artist", artist);
    url.setQuery(query);
    return url.toString();
}

static QString checkHaveArtist(const QSet<QString> &mpdArtists, const QString &artist)
{
    if (mpdArtists.contains(artist)) {
        return QLatin1String("<a href=\"")+buildUrl(artist)+QLatin1String("\">")+artist+QLatin1String("</a>");
    } else {
        // Check for AC/DC -> AC-DC
        QString mod=artist;
        mod=mod.replace("/", "-");
        if (mod!=artist && mpdArtists.contains(mod)) {
            return QLatin1String("<a href=\"")+buildUrl(mod)+QLatin1String("\">")+artist+QLatin1String("</a>");
        }
    }
    return QString();
}

ArtistView::ArtistView(QWidget *parent)
    : View(parent)
    , currentSimilarJob(nullptr)
{
    engine=ContextEngine::create(this);
    refreshAction = ActionCollection::get()->createAction("refreshartist", tr("Refresh Artist Information"), Icons::self()->refreshIcon);
    connect(refreshAction, SIGNAL(triggered()), this, SLOT(refresh()));
    connect(engine, SIGNAL(searchResult(QString,QString)), this, SLOT(searchResponse(QString,QString)));
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), SLOT(artistImage(Song,QImage,QString)));
    connect(Covers::self(), SIGNAL(coverUpdated(Song,QImage,QString)), SLOT(artistImageUpdated(Song,QImage,QString)));
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(show(QUrl)));
    text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(text, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    setStandardHeader(tr("Artist"));
   
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
    if (cancelJobAction->isEnabled()) {
        menu->addAction(cancelJobAction);
    } else {
        menu->addAction(refreshAction);
    }
    menu->exec(text->mapToGlobal(pos));
    delete menu;
}

void ArtistView::refresh()
{
    if (currentSong.isEmpty()) {
        return;
    }
    for (const QString &lang: engine->getLangs()) {
        QFile::remove(cacheFileName(currentSong.artist, engine->getPrefix(lang), false, false));
    }
    QFile::remove(cacheFileName(currentSong.artist, QString(), true, false));
    update(currentSong, true);
}

void ArtistView::update(const Song &s, bool force)
{
    if (s.isEmpty() || s.artist.isEmpty()) {
        currentSong=s;
        engine->cancel();
        clear();
        abort();
        return;
    }

    Song song=s;
    song.artist=song.basicArtist(true);
    bool artistChanged=song.artist!=currentSong.artist;

    if (artistChanged) {
        artistAlbums.clear();
        abort();
    }
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
            s.setArtistImageRequest();
            s.albumartist=currentSong.artist;
            if (!currentSong.isVariousArtists()) {
                s.file=currentSong.file;
            }
            Covers::Image img=Covers::self()->requestImage(s, true);
            if (!img.img.isNull()) {
                pic=createPicTag(img.img, img.fileName);
            }
            loadBio();
        }
    }
}

const QList<LibraryDb::Album> & ArtistView::getArtistAlbums()
{
    if (artistAlbums.isEmpty() && !currentSong.isEmpty()) {
        artistAlbums=MpdLibraryModel::self()->getArtistOrComposerAlbums(currentSong.artist);
    }
    return artistAlbums;
}

void ArtistView::artistImage(const Song &song, const QImage &i, const QString &f)
{
    if (song.isArtistImageRequest() && song.albumartist==currentSong.artist && pic.isEmpty()) {
        pic=createPicTag(i, f);
        setBio();
    }
}

void ArtistView::artistImageUpdated(const Song &song, const QImage &i, const QString &f)
{
    if (song.isArtistImageRequest() && song.albumartist==currentSong.artist) {
        pic=createPicTag(i, f);
        setBio();
    }
}

void ArtistView::loadBio()
{
    for (const QString &lang: engine->getLangs()) {
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
    NetworkJob *reply = qobject_cast<NetworkJob*>(sender());
    if (!reply) {
        return;
    }
    if (reply==currentSimilarJob) {
        if (reply->ok()) {
            QByteArray data=reply->readAll();
            QStringList artists=parseSimilarResponse(data);
            if (!artists.isEmpty()) {
                buildSimilar(artists);
                setBio();
                QFile f(cacheFileName(reply->property(constNameKey).toString(), QString(), true, true));
                if (f.open(QIODevice::WriteOnly|QIODevice::Text)) {
                    QTextStream stream(&f);
                    #ifdef Q_OS_WIN
                    stream.setCodec("UTF-8");
                    stream.setGenerateByteOrderMark(true);
                    #endif
                    for (const QString &artist: artists) {
                        stream << artist << endl;
                    }
                }
            }
        } else {
            setBio();
        }
        reply->deleteLater();
        currentSimilarJob=nullptr;
    }
}

void ArtistView::setBio()
{
    QString html=pic+"<br>"+biography;
    if (!similarArtists.isEmpty()) {
        html+=similarArtists;
    }

    if (albums.isEmpty()) {
        getArtistAlbums();

        for (const LibraryDb::Album &al: artistAlbums) {
            albums+=QLatin1String("<li><a href=\"")+buildUrl(al)+"\">"+al.name+"</a></li>";
        }
    }

    if (!albums.isEmpty()) {
        html+=View::subHeader(tr("Albums"))+QLatin1String("<ul>")+albums+QLatin1String("</ul>");
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
        QString artist=currentSong.artist;
        artist.replace(QLatin1Char('&'), QLatin1String("%26"));
        artist.replace(QLatin1Char('?'), QLatin1String("%3f"));
        html+=View::subHeader(tr("Web Links"))+QLatin1String("<ul>")+QString(webLinks).replace("${artist}", artist)+QLatin1String("</ul>");
    }

    setHtml(html);
}

void ArtistView::requestSimilar()
{
    abort();
    QUrl url("http://ws.audioscrobbler.com/2.0/");
    QUrlQuery query;

    query.addQueryItem("method", "artist.getSimilar");
    ApiKeys::self()->addKey(query, ApiKeys::LastFm);
    query.addQueryItem("autocorrect", "1");
    query.addQueryItem("artist", Covers::fixArtist(currentSong.artist));
    url.setQuery(query);

    currentSimilarJob=NetworkAccessManager::self()->get(url);
    currentSimilarJob->setProperty(constNameKey, currentSong.artist);
    connect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarReply()));
}

void ArtistView::abort()
{
    engine->cancel();
    if (currentSimilarJob) {
        currentSimilarJob->cancelAndDelete();
        currentSimilarJob=nullptr;
    }
    hideSpinner();
}

void ArtistView::searchResponse(const QString &resp, const QString &lang)
{
    biography=engine->translateLinks(resp);
    hideSpinner();

    if (!resp.isEmpty() && !lang.isEmpty()) {
        QFile f(cacheFileName(currentSong.artist, lang, false, true));
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::WriteOnly)) {
            compressor.write(resp.toUtf8().constData());
        }
    }
    loadSimilar();
    setBio();
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

    std::sort(artists.begin(), artists.end());
    return artists;
}

void ArtistView::buildSimilar(const QStringList &artists)
{
    QSet<QString> mpdArtists=MpdLibraryModel::self()->getArtists();
    QSet<QString> mpdLowerArtists;
    bool first=true;
    for (QString artist: artists) {
        if (similarArtists.isEmpty()) {
            similarArtists=QLatin1String("<br/>")+View::subHeader(tr("Similar Artists"));
        }
        // Check if we have artist in collection...
        QString artistLink=checkHaveArtist(mpdArtists, artist);
        if (artistLink.isEmpty()) {
            // ...and check case-insensitively...
            if (mpdLowerArtists.isEmpty()) {
                for (const QString &a: mpdArtists) {
                    mpdLowerArtists.insert(a.toLower());
                }
            }
            artistLink=checkHaveArtist(mpdLowerArtists, artist.toLower());
        }
        if (!artistLink.isEmpty()) {
            artist=artistLink;
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
        QUrlQuery q(url);

        if (q.hasQueryItem("artist")) {
            if (q.hasQueryItem("albumId")) {
                emit findAlbum(q.queryItemValue("artist"), q.queryItemValue("albumId"));
            } else {
                emit findArtist(q.queryItemValue("artist"));
            }
        }
    } else {
        QDesktopServices::openUrl(url);
    }
}

void ArtistView::clearCache()
{
    Utils::clearOldCache(constCacheDir, constCacheAge);
}

#include "moc_artistview.cpp"
