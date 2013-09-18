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
#include "artistview.h"
#include "localize.h"
#include "covers.h"
#include "networkaccessmanager.h"
#include "utils.h"
#include "qtiocompressor/qtiocompressor.h"
#include "musiclibrarymodel.h"
#include "contextengine.h"
#include "textbrowser.h"
#include "actioncollection.h"
#include <QScrollBar>
#include <QFile>
#include <QUrl>
#include <QProcess>
#include <QMenu>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>

const QLatin1String AlbumView::constCacheDir("albums/");
const QLatin1String AlbumView::constInfoExt(".html.gz");

static QString cacheFileName(const QString &artist, const QString &album, const QString &lang, bool createDir)
{
    return Utils::cacheDir(AlbumView::constCacheDir, createDir)+Covers::encodeName(artist)+QLatin1String(" - ")+Covers::encodeName(album)+"."+lang+AlbumView::constInfoExt;
}

enum Parts {
    Cover = 0x01,
    Details = 0x02,
    All = Cover+Details
};

AlbumView::AlbumView(QWidget *p)
    : View(p)
    , detailsReceived(0)
{
    engine=ContextEngine::create(this);
    refreshAction = ActionCollection::get()->createAction("refreshalbum", i18n("Refresh Album Information"), "view-refresh");
    connect(refreshAction, SIGNAL(triggered()), SLOT(refresh()));
    connect(engine, SIGNAL(searchResult(QString,QString)), this, SLOT(searchResponse(QString,QString)));
    connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
    connect(Covers::self(), SIGNAL(coverUpdated(const Song &, const QImage &, const QString &)), SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(playSong(QUrl)));
    text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(text, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    setStandardHeader(i18n("Album Information"));
    int imageSize=fontMetrics().height()*18;
    setPicSize(QSize(imageSize, imageSize));
    clear();
    if (ArtistView::constCacheAge>0) {
        clearCache();
        QTimer *timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(clearCache()));
        timer->start((int)((ArtistView::constCacheAge/2.0)*1000*24*60*60));
    }
}

void AlbumView::showContextMenu(const QPoint &pos)
{
   QMenu *menu = text->createStandardContextMenu();
   menu->addSeparator();
   menu->addAction(refreshAction);
   menu->exec(text->mapToGlobal(pos));
   delete menu;
}

void AlbumView::refresh()
{
    if (currentSong.isEmpty()) {
        return;
    }
    foreach (const QString &lang, engine->getLangs()) {
        QFile::remove(cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, engine->getPrefix(lang), false));
    }
    update(currentSong, true);
}

void AlbumView::update(const Song &song, bool force)
{
    if (song.isStream() && !song.isCantataStream() && !song.isCdda() && song.album.isEmpty() && !song.name.isEmpty() && song.name!=currentSong.name) {
        currentSong=song;
        clearDetails();
        setHeader(song.name);
        needToUpdate=false;
        detailsReceived=All;
        pic=createPicTag(QImage(), INSTALL_PREFIX"/share/"+QCoreApplication::applicationName()+"/icons/stream.png");
        updateDetails();
        return;
    }

    if (song.isEmpty() || song.albumArtist().isEmpty() || song.album.isEmpty()) {
        currentSong=song;
        clearDetails();
        return;
    }

    if (force || song.albumArtist()!=currentSong.albumArtist() || song.album!=currentSong.album) {
        currentSong=song;
        currentArtist=currentSong.basicArtist();
        if (!isVisible()) {
            needToUpdate=true;
            return;
        }
        clearDetails();
        setHeader(song.album.isEmpty() ? stdHeader : song.album);
        Covers::Image cImg=Covers::self()->requestImage(song);
        if (!cImg.img.isNull()) {
            detailsReceived|=Cover;
            pic=createPicTag(cImg.img, cImg.fileName);
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
    if (QLatin1String("cantata")==url.scheme()) {
        emit playSong(url.path().mid(1)); // Remove leading /
    } else {
        #ifdef Q_OS_WIN
        QProcess::startDetached(QLatin1String("cmd"), QStringList() << QLatin1String("/c") << QLatin1String("start") << url.toString());
        #else
        QProcess::startDetached(QLatin1String("xdg-open"), QStringList() << url.toString());
        #endif
    }
}

void AlbumView::getTrackListing()
{
    if (songs.isEmpty()) {
        songs=MusicLibraryModel::self()->getAlbumTracks(currentSong);
    }

    if (!songs.isEmpty()) {
        trackList=View::subHeader(i18n("Tracks"))+QLatin1String("<p><table>");
        foreach (const Song &s, songs) {
            trackList+=QLatin1String("<tr><td>")+QString::number(s.track)+
                       QLatin1String("</td><td><a href=\"cantata:///")+s.file+"\">"+
                       (s.file==currentSong.file ? "<b>"+s.displayTitle()+"</b>" : s.displayTitle())+QLatin1String("</a></td></tr>");
        }

        trackList+=QLatin1String("</table></p>");
        updateDetails();
    }
}

void AlbumView::getDetails()
{
    engine->cancel();
    foreach (const QString &lang, engine->getLangs()) {
        QString prefix=engine->getPrefix(lang);
        QString cachedFile=cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, prefix, false);
        if (QFile::exists(cachedFile)) {
            QFile f(cachedFile);
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::ReadOnly)) {
                QByteArray data=compressor.readAll();

                if (!data.isEmpty()) {
                    searchResponse(QString::fromUtf8(data), QString());
                    Utils::touchFile(cachedFile);
                    return;
                }
            }
        }
    }
    engine->search(QStringList() << currentSong.albumArtist() << currentSong.album, ContextEngine::Album);
}

void AlbumView::coverRetrieved(const Song &s, const QImage &img, const QString &file)
{
    if (s==currentSong && pic.isEmpty()) {
        detailsReceived|=Cover;
        if (All==detailsReceived) {
            hideSpinner();
        }
        pic=createPicTag(img, file);
        if (!pic.isEmpty()) {
            updateDetails();
        }
    }
}

void AlbumView::searchResponse(const QString &resp, const QString &lang)
{
    detailsReceived|=Details;
    if (All==detailsReceived) {
        hideSpinner();
    }

    if (!resp.isEmpty()) {
        details=engine->translateLinks(resp);
        if (!lang.isEmpty()) {
            QFile f(cacheFileName(Covers::fixArtist(currentSong.albumArtist()), currentSong.album, lang, true));
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(resp.toUtf8().constData());
            }
        }
        updateDetails();
    }
}

void AlbumView::updateDetails(bool preservePos)
{
    int pos=preservePos ? text->verticalScrollBar()->value() : 0;
    if (!details.isEmpty()) {
        setHtml(pic+"<br>"+details+"<br>"+trackList);
    } else {
        setHtml(pic+trackList);
    }
    if (preservePos) {
        text->verticalScrollBar()->setValue(pos);
    }
}

void AlbumView::clearCache()
{
    Utils::clearOldCache(constCacheDir, ArtistView::constCacheAge);
}

void AlbumView::clearDetails()
{
    details.clear();
    trackList.clear();
    bio.clear();
    pic.clear();
    songs.clear();
    clear();
    engine->cancel();
    detailsReceived=0;
}
