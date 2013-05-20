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
#include <QScrollBar>
#include <QFile>
#include <QUrl>
#include <QNetworkReply>

static const int constCheckChars=100; // Num chars to cehck between artist bio and details - as sometimes wikipedia does not know album, so returns artist!

const QLatin1String AlbumView::constCacheDir("albums/");
const QLatin1String AlbumView::constInfoExt(".html.gz");

static QString cacheFileName(const QString &artist, const QString &locale, const QString &album, bool createDir)
{
    return Utils::cacheDir(AlbumView::constCacheDir, createDir)+Covers::encodeName(artist)+QLatin1String(" - ")+Covers::encodeName(album)+"."+locale+AlbumView::constInfoExt;
}

enum Parts {
    Cover = 0x01,
    Details = 0x02,
    ArtistBio = 0x04,
    All = Cover+Details+ArtistBio
};

AlbumView::AlbumView(QWidget *p)
    : View(p)
    , triedWithFilter(false)
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
    if (song.isEmpty()) {
        currentSong=song;
        cancel();
        clear();
        return;
    }
    if (force || song.albumArtist()!=currentSong.albumArtist() || song.album!=currentSong.album) {
        currentSong=song;
        if (!isVisible()) {
            needToUpdate=true;
            return;
        }
        details.clear();
        trackList.clear();
        bio.clear();
        triedWithFilter=false;
        detailsReceived=bioArtist==currentSong.artist ? ArtistBio : 0;
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
        updateDetails(true);
    }
}

void AlbumView::playSong(const QUrl &url)
{
    emit playSong(url.path());
}

void AlbumView::artistBio(const QString &artist, const QString &b)
{
    if (artist==currentSong.artist) {
        bioArtist=artist;
        detailsReceived|=ArtistBio;
        if (All==detailsReceived) {
            hideSpinner();
        }
        bio=b.left(constCheckChars);
        updateDetails();
    }
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
    cancel();
    QString cachedFile=cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, locale, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::ReadOnly)) {
            QByteArray data=compressor.readAll();

            if (!data.isEmpty()) {
                searchResponse(QString::fromUtf8(data));
                Utils::touchFile(cachedFile);
                return;
            }
        }
    }
    search(currentSong.albumArtist()+" "+currentSong.album);
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

void AlbumView::searchResponse(const QString &resp)
{
    if (View::constAmbiguous==resp && !triedWithFilter) {
        triedWithFilter=true;
        search(currentSong.albumArtist()+" "+currentSong.album+" "+i18nc("Search pattern for an album", "(album|score|soundtrack)"));
        return;
    }

    detailsReceived|=Details;
    if (All==detailsReceived) {
        hideSpinner();
    }

    if (!resp.isEmpty() && View::constAmbiguous!=resp) {
        details=resp;
        QFile f(cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, locale, true));
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::WriteOnly)) {
            compressor.write(resp.toUtf8().constData());
        }
        updateDetails();
    }
}

void AlbumView::updateDetails(bool preservePos)
{
    if (detailsReceived&ArtistBio && detailsReceived&Details && details.left(constCheckChars)==bio) {
        details.clear();
    }
    int pos=preservePos ? text->verticalScrollBar()->value() : 0;
    if (detailsReceived&ArtistBio) {
        text->setText(details+trackList);
    } else {
        text->setText(trackList);
    }
    if (preservePos) {
        text->verticalScrollBar()->setValue(pos);
    }
}
