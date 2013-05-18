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

#include "albumview.h"
#include "localize.h"
#include "covers.h"
#include "networkaccessmanager.h"
#include "utils.h"
#include "qtiocompressor/qtiocompressor.h"
#include "musiclibrarymodel.h"
#include <QTextBrowser>
#include <QFile>
#include <QUrl>
#include <QNetworkReply>
#include <QXmlStreamReader>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QDebug>

const QLatin1String AlbumView::constCacheDir("albums/");
const QLatin1String AlbumView::constInfoExt(".xml.gz");

static QString cacheFileName(const QString &artist, const QString &album, bool createDir)
{
    return Utils::cacheDir(AlbumView::constCacheDir, createDir)+Covers::encodeName(artist)+QLatin1String(" - ")+Covers::encodeName(album)+AlbumView::constInfoExt;
}

enum Parts {
    Cover = 0x01,
    Details = 0x02,
    All = Cover+Details
};

AlbumView::AlbumView(QWidget *p)
    : View(p)
    , detailsReceived(0)
    , job(0)
{
    connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), SLOT(coverRetreived(const Song &, const QImage &, const QString &)));
    connect(Covers::self(), SIGNAL(coverUpdated(const Song &, const QImage &, const QString &)), SLOT(coverRetreived(const Song &, const QImage &, const QString &)));
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(playSong(QUrl)));

    int imageSize=fontMetrics().height()*18;
    setPicSize(QSize(imageSize, imageSize));
    setStandardHeader(i18n("Album Information"));
    clear();
}

void AlbumView::update(const Song &song, bool force)
{
    if (force || song.albumArtist()!=currentSong.albumArtist() || song.album!=currentSong.album) {
        currentSong=song;
        if (!isVisible()) {
            needToUpdate=true;
            return;
        }
        details.clear();
        trackList.clear();
        detailsReceived=0;
        setHeader(song.album.isEmpty() ? stdHeader : song.album);
        Covers::Image cImg=Covers::self()->requestImage(song);
        if (!cImg.img.isNull()) {
            detailsReceived|=Cover;
            setPic(cImg.img);
        }
        getTrackListing();
        getDetails();

        if (All==detailsReceived) {
            hideSpinner();
        } else {
            showSpinner();
        }
    } else if (song.title!=currentSong.title) {
        currentSong=song;
        getTrackListing();
        updateDetails();
    }
}

void AlbumView::playSong(const QUrl &url)
{
    emit playSong(url.path());
}

void AlbumView::getTrackListing()
{
    QList<Song> songs=MusicLibraryModel::self()->getAlbumTracks(currentSong);

    if (!songs.isEmpty()) {
        trackList=QLatin1String("<h3>")+i18n("Tracks")+QLatin1String("</h3><p><table>");
        foreach (const Song &s, songs) {
            trackList+=QLatin1String("<tr><td>")+QString::number(s.track)+
                       QLatin1String("</td><td><a href=\"")+s.file+"\">"+
                       (s==currentSong ? "<b>"+s.displayTitle()+"</b>" : s.displayTitle())+QLatin1String("</a></td></tr>");
        }

        trackList+=QLatin1String("</table></p>");
    }
}

void AlbumView::getDetails()
{
    abort();
    QString cachedFile=cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::ReadOnly)) {
            QByteArray data=compressor.readAll();

            if (!data.isEmpty() && parseLastFmResponse(data)) {
                updateDetails();
                Utils::touchFile(cachedFile);
                detailsReceived|=Details;
                return;
            }
        }
    }
    QUrl url("http://ws.audioscrobbler.com/2.0/");
    #if QT_VERSION < 0x050000
    QUrl &query=url;
    #else
    QUrlQuery query;
    #endif

    query.addQueryItem("method", "album.getinfo");
    query.addQueryItem("api_key", Covers::constLastFmApiKey);
    query.addQueryItem("autocorrect", "1");
    query.addQueryItem("artist", Covers::fixArtist(currentSong.albumArtist()));
    query.addQueryItem("album", currentSong.album);
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    job=NetworkAccessManager::self()->get(url);
    connect(job, SIGNAL(finished()), this, SLOT(infoRetreived()));
}

void AlbumView::abort()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(infoRetreived()));
        job->deleteLater();
        job=0;
    }
}

void AlbumView::coverRetreived(const Song &s, const QImage &img, const QString &file)
{
    Q_UNUSED(file)
    if (s==currentSong) {
        detailsReceived|=Cover;
        if (All==detailsReceived) {
            hideSpinner();
        }
        setPic(img);
    }
}

void AlbumView::infoRetreived()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply!=job) {
        return;
    }

    detailsReceived|=Details;
    if (All==detailsReceived) {
        hideSpinner();
    }
    if (QNetworkReply::NoError==reply->error()) {
        QByteArray data=reply->readAll();
        if (parseLastFmResponse(data)) {
            QFile f(cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, true));
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(data);
            }
        }
    }
    updateDetails();
    reply->deleteLater();
    job=0;
}

static inline QString escape(const QString &str)
{
    return QString(str).replace("/", "%2F");
}

bool AlbumView::parseLastFmResponse(const QByteArray &data)
{
    QXmlStreamReader xml(data);
    bool finished=false;

    while (xml.readNextStartElement() && !finished) {
        if (QLatin1String("album")==xml.name()) {
            while (xml.readNextStartElement() && !finished) {
                if (QLatin1String("wiki")==xml.name()) {
                    while (xml.readNextStartElement() && !finished) {
                        if (QLatin1String("content")==xml.name()) {
                            details = xml.readElementText();
                            static const QRegExp re("User-contributed text.*");
                            details.remove(re);
                            finished=true;
                        } else {
                            xml.skipCurrentElement();
                        }
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
        }
    }

    if (!details.isEmpty()) {
        int split = details.indexOf('\n', 512);
        if (-1==split) {
            split = details.indexOf(". ", 512);
        }

        details="<p>"+details.left(split);
        if (-1!=split) {
            QString url = "http://www.last.fm/music/"+escape(Covers::fixArtist(currentSong.albumArtist()))+"/"+escape(currentSong.album)+"/+wiki";
            details += QString("<br/><a href='%1'>%2</a>").arg(url, i18n("Read more"));
        }
        details+="</p>";
    }
    return !details.isEmpty();
}

void AlbumView::updateDetails()
{
    text->setText(details+trackList);
}
