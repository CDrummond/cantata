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
#include "combobox.h"
#include "headerlabel.h"
#include "musiclibrarymodel.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "qjson/parser.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QNetworkReply>
#include <QApplication>
#include <QTextBrowser>
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

static const QLatin1String constApiKey("N5JHVHNG0UOZZIDVT");
static const char *constNameKey="name";
static const int constCacheAge=7;

const QLatin1String ArtistView::constCacheDir("artists/");
const QLatin1String ArtistView::constInfoExt(".json.gz");

static QString cacheFileName(const QString &artist, bool similar, bool createDir)
{
    return Utils::cacheDir(ArtistView::constCacheDir, createDir)+Covers::encodeName(artist)+(similar ? "-similar" : "")+ArtistView::constInfoExt;
}

ArtistView::ArtistView(QWidget *parent)
    : View(parent)
    , currentBioJob(0)
    , currentSimilarJob(0)
{
    combo=new ComboBox(this);
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(setBio()));
    layout()->addWidget(combo);

    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), SLOT(artistImage(Song,QImage,QString)));
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(showArtist(QUrl)));
    Utils::clearOldCache(constCacheDir, constCacheAge);
    setStandardHeader(i18n("Artist Information"));
   
    int imageHeight=fontMetrics().height()*14;
    int imageWidth=imageHeight*1.5;
    setPicSize(QSize(imageWidth, imageHeight));
    provider=Settings::self()->infoProvider();
    clear();
}

void ArtistView::saveSettings()
{
    if (combo->count()) {
        provider=combo->itemData(combo->currentIndex()).toString();
    }
    if (!provider.isEmpty()) {
        Settings::self()->saveInfoProvider(provider);
    }
}

void ArtistView::update(const Song &s, bool force)
{
    Song song=s;
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }

    if (!song.albumartist.isEmpty() && !song.artist.isEmpty() && song.albumartist.length()<song.artist.length() && song.artist.startsWith(song.albumartist)) {
        song.artist=song.albumartist;
    }

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
        if (combo->count()) {
            provider=combo->itemData(combo->currentIndex()).toString();
        }

        clear();
        combo->clear();
        biographies.clear();
        similarArtists=QString();
        if (!currentSong.isEmpty()) {
            text->setText("<i>Retrieving...</i>");
            setHeader(currentSong.artist);

            Song s;
            s.albumartist=currentSong.artist;
            s.file=currentSong.file;
            Covers::Image img=Covers::self()->requestImage(s);
            if (!img.img.isNull()) {
                artistImage(s, img.img, img.fileName);
            }
            loadBio();
        }
    }
}

void ArtistView::artistImage(const Song &song, const QImage &i, const QString &f)
{
    Q_UNUSED(f)
    if (song.albumartist==currentSong.artist) {
        setPic(i);
    }
}

void ArtistView::setProvider()
{
    if (!provider.isEmpty() && combo->itemData(combo->currentIndex()).toString()!=provider) {
        for (int i=0; i<combo->count(); ++i) {
            if (combo->itemData(i).toString()==provider) {
                combo->setCurrentIndex(i);
                break;
            }
        }
        provider=combo->itemData(combo->currentIndex()).toString();
    }
}

void ArtistView::loadBio()
{
    QString cachedFile=cacheFileName(currentSong.artist, false, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::ReadOnly)) {
            QByteArray data=compressor.readAll();

            if (!data.isEmpty() && parseBioResponse(data)) {
                setProvider();
                setBio();
                Utils::touchFile(cachedFile);
                return;
            }
        }
    }

    showSpinner();
    requestBio();
}

void ArtistView::loadSimilar()
{
    QString cachedFile=cacheFileName(currentSong.artist, true, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::ReadOnly)) {
            QByteArray data=compressor.readAll();

            if (!data.isEmpty() && parseSimilarResponse(data)) {
                setBio();
                Utils::touchFile(cachedFile);
                return;
            }
        }
    }

    requestSimilar();
}

void ArtistView::handleBioReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (reply==currentBioJob) {
        hideSpinner();
        bool ok=false;
        if (QNetworkReply::NoError==reply->error()) {
            QByteArray data=reply->readAll();
            if (parseBioResponse(data)) {
                setProvider();
                setBio();
                ok=true;
                QFile f(cacheFileName(reply->property(constNameKey).toString(), false, true));
                QtIOCompressor compressor(&f);
                compressor.setStreamFormat(QtIOCompressor::GzipFormat);
                if (compressor.open(QIODevice::WriteOnly)) {
                    compressor.write(data);
                }
            }
        }
        if (!ok) {
            text->setText(i18n(("<i><Failed to download information!</i>")));
        }
        reply->deleteLater();
        currentBioJob=0;
    }
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
            if (parseSimilarResponse(data)) {
                setBio();
                QFile f(cacheFileName(reply->property(constNameKey).toString(), true, true));
                QtIOCompressor compressor(&f);
                compressor.setStreamFormat(QtIOCompressor::GzipFormat);
                if (compressor.open(QIODevice::WriteOnly)) {
                    compressor.write(data);
                }
            }
        }
        reply->deleteLater();
        currentSimilarJob=0;
    }
}

void ArtistView::setBio()
{
    int bio=combo->currentIndex();
    if (biographies.contains(bio)) {
        QString html=biographies[bio];
        if (!similarArtists.isEmpty()) {
            html+=similarArtists;
        }

        #ifndef Q_OS_WIN
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
            html+="<h3>"+i18n("Web Links")+"</h3><ul>"+QString(webLinks).replace("${artist}", currentSong.artist)+"</ul>";
        }
        #endif
        text->setText(html);
    }
}

void ArtistView::requestBio()
{
    abort();
    #if QT_VERSION < 0x050000
    QUrl url;
    QUrl &query=url;
    #else
    QUrl url;
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost("developer.echonest.com");
    url.setPath("api/v4/artist/biographies");
    query.addQueryItem("api_key", constApiKey);
    query.addQueryItem("name", currentSong.artist);
    query.addQueryItem("format", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    currentBioJob=NetworkAccessManager::self()->get(url);
    currentBioJob->setProperty(constNameKey, currentSong.artist);
    connect(currentBioJob, SIGNAL(finished()), this, SLOT(handleBioReply()));
}

void ArtistView::requestSimilar()
{
    abort();
    #if QT_VERSION < 0x050000
    QUrl url;
    QUrl &query=url;
    #else
    QUrl url;
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost("developer.echonest.com");
    url.setPath("api/v4/artist/similar");
    query.addQueryItem("api_key", constApiKey);
    query.addQueryItem("name", currentSong.artist);
    query.addQueryItem("format", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    currentSimilarJob=NetworkAccessManager::self()->get(url);
    currentSimilarJob->setProperty(constNameKey, currentSong.artist);
    connect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarReply()));
}

void ArtistView::abort()
{
    if (currentBioJob) {
        disconnect(currentBioJob, SIGNAL(finished()), this, SLOT(handleBioReply()));
        currentBioJob->abort();
        currentBioJob=0;
    }

    if (currentSimilarJob) {
        disconnect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarArtistsReply()));
        currentSimilarJob->abort();
        currentSimilarJob=0;
    }
}

struct Bio
{
    Bio(const QString &s=QString(), const QString &t=QString())
        : site(s), text(t) {
        if (QLatin1String("last.fm")==site) {
            text.replace("  ", "<br/><br/>");
        } else if (QLatin1String("wikipedia")==site) {
            text.replace(" \n\" \n\n", "\"<br/><br/>");
            text.replace(" \n\n\" \n", "<br/><br/>\"");
            text.replace("\n", "<br/><br/>");
        } else {
            text.replace("\n", "<br/><br/>");
        }
        text.replace("<br/><br/><br/>", "<br/>");
    }
    QString site;
    QString text;
};

static int value(const QString &site)
{
    return QLatin1String("last.fm")==site
            ? 0
            : QLatin1String("wikipedia")==site
              ? 1
              : 2;
}

static const QString constReferencesLine("This article does not cite any references or sources.");
static const QString constDisambiguationLine("This article is about the band");
static const QString constDisambiguationLine2("For other uses, see ");

bool ArtistView::parseBioResponse(const QByteArray &resp)
{
    QMultiMap<int, Bio> biogs;
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("response")) {
        QVariantMap response=parsed["response"].toMap();
        if (response.contains("biographies")) {
            QVariantList biographies=response["biographies"].toList();
            foreach (const QVariant &b, biographies) {
                QVariantMap details=b.toMap();
                if (!details.isEmpty() && details.contains("text") && details.contains("site")) {
                    QString site=details["site"].toString();
                    QString text=details["text"].toString();

                    if (text.startsWith(constReferencesLine) || text.startsWith(constDisambiguationLine) || text.startsWith(constDisambiguationLine2)) {
                        int eol=text.indexOf("\n");
                        if (-1!=eol) {
                            text=text.mid(eol+1);
                        }
                    }

//                    if (QLatin1String("wikipedia")==site) {
//                        int start=text.indexOf("\n , ");
//                        if (-1!=start) {
//                            int end=text.indexOf("\n", start+4);
//                            if (-1!=end) {
//                                text=text.remove(start, end-start);
//                            }
//                        }
//                    }
                    while ('\n'==text[0]) {
                        text=text.mid(1);
                    }
                    text.replace("\n ", "\n");

                    biogs.insertMulti(value(site), Bio(site, text));
                }
            }
        }
    }

    QMultiMap<int, Bio>::ConstIterator it=biogs.constBegin();
    QMultiMap<int, Bio>::ConstIterator end=biogs.constEnd();
    for (; it!=end; ++it) {
        if (it.value().text.length()<75 && combo->count()>0) {
            // Ignore the "..." cr*p
            continue;
        }

        // And some others that seem to be just lots of dots???
        if (it.value().text.length()<250) {
            QString copy=it.value().text;
            copy.replace(".", "");
            if (copy.length()<75) {
                continue;
            }
        }
        biographies[combo->count()]=it.value().text;
        combo->insertItem(combo->count(), i18n("Source: %1").arg(it.value().site), it.value().site);
    }

    if (!biogs.isEmpty()) {
        loadSimilar();
    }
    return !biogs.isEmpty();
}

bool ArtistView::parseSimilarResponse(const QByteArray &resp)
{
    QSet<QString> mpdArtists=MusicLibraryModel::self()->getAlbumArtists();
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("response")) {
        QVariantMap response=parsed["response"].toMap();
        if (response.contains("artists")) {
            QVariantList artists=response["artists"].toList();
            foreach (const QVariant &a, artists) {
                QVariantMap details=a.toMap();
                if (!details.isEmpty() && details.contains("name")) {
                    QString artist=details["name"].toString();
                    if (similarArtists.isEmpty()) {
                        similarArtists="<br/><h3>"+i18n("Similar Artists")+"</h3><ul>";
                    }
                    if (mpdArtists.contains(artist)) {
                        artist=QLatin1String("<a href=\"cantata://?artist=")+artist+"\">"+artist+"</a>";
                    } else {
                        // Check for AC/DC -> AC-DC
                        QString mod=artist;
                        mod=mod.replace("/", "-");
                        if (mod!=artist && mpdArtists.contains(mod)) {
                            artist=QLatin1String("<a href=\"cantata://?artist=")+mod+"\">"+artist+"</a>";
                        }
                    }

                    similarArtists+="<li>"+artist+"</li>";
                }
            }
        }
    }

    if (!similarArtists.isEmpty()) {
        similarArtists+="</ul></p>";
    }
    return !similarArtists.isEmpty();
}

void ArtistView::showArtist(const QUrl &url)
{
    if (QLatin1String("cantata")==url.scheme()) {
        #if QT_VERSION < 0x050000
        const QUrl &q=url;
        #else
        QUrlQuery q(url);
        #endif

        if (q.hasQueryItem("artist")) {
            emit findArtist(q.queryItemValue("artist"));
        }
    }
    #ifndef Q_OS_WIN
    else {
        QProcess::startDetached(QLatin1String("xdg-open"), QStringList() << url.toString());
    }
    #endif
}
