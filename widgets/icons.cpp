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

#include "icons.h"
#include <QtGui/QApplication>
#include <QtGui/QPixmap>
#include <QtGui/QFont>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtCore/QDir>
#include <math.h>

static QPixmap createSingleIconPixmap(int size, QColor &col, double opacity)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QFont font(QLatin1String("sans"));
    font.setBold(false);
    font.setItalic(false);
    font.setPixelSize(size*0.9);
    p.setFont(font);
    p.setPen(col);
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawText(QRect(0, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.drawText(QRect(1, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.drawText(QRect(-1, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.end();
    return pix;
}

static QPixmap createConsumeIconPixmap(int size, QColor &col, double opacity)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setPen(QPen(col, 1.5));
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    QRectF rect(2.5, 2.5, size-4, size-4);
    double distanceX=fabs(cos(35.0))*(rect.width()/2);
    double distanceY=fabs(sin(35.0))*(rect.height()/2);
    double midX=rect.x()+(rect.width()/2);
    double midY=rect.y()+(rect.height()/2);
    p.drawArc(rect, 40*16, 280*16);
    p.drawLine(midX, midY, midX+distanceX, midY-distanceY);
    p.drawLine(midX, midY, midX+distanceX, midY+distanceY);
    p.drawPoint(midX, rect.y()+rect.height()/4);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.end();
    return pix;
}

static Icon createIcon(bool isSingle)
{
    Icon icon;
    QColor stdColor=QColor(QApplication::palette().color(QPalette::Active, QPalette::WindowText));
    if (stdColor==Qt::white) {
        stdColor=QColor(200, 200, 200);
    } else if (stdColor.red()<128 && stdColor.green()<128 && stdColor.blue()<128 &&
               stdColor.red()==stdColor.green() && stdColor.green()==stdColor.blue()) {
        stdColor=Qt::black;
    }
    QColor highlightColor=stdColor.red()<100 ? stdColor.lighter(50) : stdColor.darker(50);
    QList<int> sizes=QList<int>() << 16 << 22;

    foreach (int s, sizes) {
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, stdColor, 100.0) : createConsumeIconPixmap(s, stdColor, 100.0));
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, stdColor, 50.0) : createConsumeIconPixmap(s, stdColor, 50.0), QIcon::Disabled);
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, highlightColor, 100.0) : createConsumeIconPixmap(s, highlightColor, 100.0), QIcon::Active);
    }

    return icon;
}

#ifndef ENABLE_KDE_SUPPORT
Icon Icons::appIcon;
Icon Icons::shortcutsIcon;
#endif
Icon Icons::singleIcon;
Icon Icons::consumeIcon;
Icon Icons::repeatIcon;
Icon Icons::shuffleIcon;
Icon Icons::libraryIcon;
Icon Icons::wikiIcon;
Icon Icons::albumIcon;
Icon Icons::streamIcon;
Icon Icons::configureIcon;
Icon Icons::connectIcon;
Icon Icons::disconnectIcon;
Icon Icons::speakerIcon;
Icon Icons::lyricsIcon;
Icon Icons::dynamicIcon;
Icon Icons::playlistIcon;
Icon Icons::variousArtistsIcon;
Icon Icons::artistIcon;

void Icons::init()
{
    singleIcon=createIcon(true);
    consumeIcon=createIcon(false);
    repeatIcon=Icon("cantata-view-media-repeat");
    shuffleIcon=Icon("cantata-view-media-shuffle");
    libraryIcon=Icon("cantata-view-media-library");
    wikiIcon=Icon("cantata-view-wikipedia");
    albumIcon=Icon("media-optical");
    streamIcon=Icon("applications-internet");
    configureIcon=Icon("configure");
    connectIcon=Icon("network-connect");
    disconnectIcon=Icon("network-disconnect");
    speakerIcon=Icon("speaker");
    lyricsIcon=Icon("view-media-lyrics");
    dynamicIcon=Icon("media-playlist-shuffle");
    playlistIcon=Icon("view-media-playlist");
    variousArtistsIcon=Icon("cantata-view-media-artist-various");
    artistIcon=Icon("view-media-artist");
    #ifndef ENABLE_KDE_SUPPORT
    appIcon=Icon::create(QList<Icon::File>() << Icon::File(":cantata.svg") << Icon::File(":cantata16.png", 16) << Icon::File(":cantata22.png", 22)
                                             << Icon::File(":cantata32.png", 32) << Icon::File(":cantata48.png", 48) << Icon::File(":cantata64.png", 64));
    shortcutsIcon=Icon("preferences-desktop-keyboard");

    if (repeatIcon.isNull()) {
        repeatIcon=Icon::create(QList<Icon::File>() << Icon::File(":repeat16.png", 16) << Icon::File(":repeat22.png", 22));
    }
    if (shuffleIcon.isNull()) {
        shuffleIcon=Icon::create(QList<Icon::File>() << Icon::File(":shuffle16.png", 16) << Icon::File(":shuffle22.png", 22));
    }
    if (libraryIcon.isNull()) {
        libraryIcon=Icon::create(QList<Icon::File>() << Icon::File(":lib16.png", 16) << Icon::File(":lib32.png", 22));
    }
    if (wikiIcon.isNull()) {
        wikiIcon=Icon::create(QList<Icon::File>() << Icon::File(":wiki16.png", 16) << Icon::File(":wiki32.png", 22));
    }

    #ifndef Q_OS_WIN
    if (shortcutsIcon.isNull()) {
        shortcutsIcon=Icon("keyboard");
    }
    if (albumIcon.isNull()) {
        albumIcon=Icon("media-optical-audio");
    }
    if (configureIcon.isNull()) {
        configureIcon=Icon("gtk-preferences");
    }
    if (connectIcon.isNull()) {
        connectIcon=Icon("connect_creating");
    }
    if (disconnectIcon.isNull()) {
        disconnectIcon=Icon("media-eject");
    }
    if (speakerIcon.isNull()) {
        speakerIcon=Icon("audio-speakers");
        if (speakerIcon.isNull()) {
            speakerIcon=Icon("gnome-volume-control");
        }
    }
    if (lyricsIcon.isNull()) {
        lyricsIcon=Icon("text-x-generic");
    }
    if (dynamicIcon.isNull()) {
        dynamicIcon=Icon("text-x-generic");
    }
    if (playlistIcon.isNull()) {
        playlistIcon=Icon("audio-x-mp3-playlist");
        if (playlistIcon.isNull()) {
            playlistIcon=Icon("audio-x-generic");
        }
    }
    if (variousArtistsIcon.isNull()) {
        variousArtistsIcon.addFile(":va16.png");
        variousArtistsIcon.addFile(":va22.png");
        variousArtistsIcon.addFile(":va32.png");
        variousArtistsIcon.addFile(":va48.png");
        variousArtistsIcon.addFile(":va64.png");
        variousArtistsIcon.addFile(":va128.png");
    }
    if (artistIcon.isNull()) {
        artistIcon.addFile(":artist16.png");
        artistIcon.addFile(":artist22.png");
        artistIcon.addFile(":artist32.png");
        artistIcon.addFile(":artist48.png");
        artistIcon.addFile(":artist64.png");
        artistIcon.addFile(":artist128.png");
    }
    #endif // Q_OS_WIN
    #endif // ENABLE_KDE_SUPPORT
}

