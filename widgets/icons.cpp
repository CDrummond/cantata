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

static QList<int> constStdSizes=QList<int>() << 16 << 22 << 32 << 48;

static QPixmap createSingleIconPixmap(int size, const QColor &col, double opacity)
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
    p.end();
    return pix;
}

static QPixmap createConsumeIconPixmap(int size, const QColor &col, double opacity)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setPen(QPen(col, size/10.0));
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
    p.end();
    return pix;
}

static void calcIconColors(QColor &stdColor, QColor &highlightColor)
{
    stdColor=QColor(QApplication::palette().color(QPalette::Active, QPalette::ButtonText));
    if (stdColor==Qt::white) {
        stdColor=QColor(200, 200, 200);
    } else if (stdColor.red()<128 && stdColor.green()<128 && stdColor.blue()<128 &&
               stdColor.red()==stdColor.green() && stdColor.green()==stdColor.blue()) {
        stdColor=Qt::black;
    }
    highlightColor=stdColor.red()<100 ? stdColor.lighter(50) : stdColor.darker(50);
}

static Icon createIcon(bool isSingle, const QColor &stdColor, const QColor &highlightColor)
{
    Icon icon;

    foreach (int s, constStdSizes) {
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, stdColor, 1.0) : createConsumeIconPixmap(s, stdColor, 1.0));
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, stdColor, 0.5) : createConsumeIconPixmap(s, stdColor, 0.5), QIcon::Disabled);
        icon.addPixmap(isSingle ? createSingleIconPixmap(s, highlightColor, 1.0) : createConsumeIconPixmap(s, highlightColor, 1.0), QIcon::Active);
    }

    return icon;
}

static unsigned char checkBounds(int num)
{
    return num < 0 ? 0 : (num > 255 ? 255 : num);
}

static void adjustPix(unsigned char *data, int numChannels, int w, int h, int stride, int r, int g, int b, double opacity)
{
    int width=w*numChannels,
        offset=0;

    for(int row=0; row<h; ++row) {
        for(int column=0; column<width; column+=numChannels) {
            unsigned char source=data[offset+column+1];

            #if Q_BYTE_ORDER == Q_BIG_ENDIAN
            /* ARGB */
            data[offset+column]*=opacity;
            data[offset+column+1] = checkBounds(r-source);
            data[offset+column+2] = checkBounds(g-source);
            data[offset+column+3] = checkBounds(b-source);
            #else
            /* BGRA */
            data[offset+column] = checkBounds(b-source);
            data[offset+column+1] = checkBounds(g-source);
            data[offset+column+2] = checkBounds(r-source);
            data[offset+column+3]*=opacity;
            #endif
        }
        offset+=stride;
    }
}

static QPixmap recolour(const QImage &img, const QColor &col, double opacity)
{
    QImage i=img;
    if (i.depth()!=32) {
        i=i.convertToFormat(QImage::Format_ARGB32);
    }

    adjustPix(i.bits(), 4, i.width(), i.height(), i.bytesPerLine(), col.red(), col.green(), col.blue(), opacity);
    return QPixmap::fromImage(i);
}

static Icon createRecolourableIcon(const QString &name, const QColor &stdColor, const QColor &highlightColor)
{
    if (QColor(Qt::black)==stdColor) {
        // Text colour is black, so is icon, therefore no need to recolour!!!
        return Icon::create(name, constStdSizes);
    }

    Icon icon;

    foreach (int s, constStdSizes) {
        QImage img(QChar(':')+name+QString::number(s));
        if (!img.isNull()) {
            icon.addPixmap(recolour(img, stdColor, 1.0));
            icon.addPixmap(recolour(img, stdColor, 0.5), QIcon::Disabled);
            icon.addPixmap(recolour(img, highlightColor, 1.0), QIcon::Active);
        }
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
Icon Icons::radioStreamIcon;
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
Icon Icons::editIcon;
Icon Icons::clearListIcon;
Icon Icons::menuIcon;

void Icons::init()
{
    QColor stdColor;
    QColor highlightColor;
    calcIconColors(stdColor, highlightColor);

    singleIcon=createIcon(true, stdColor, highlightColor);
    consumeIcon=createIcon(false, stdColor, highlightColor);
    libraryIcon=Icon("cantata-view-media-library");
    radioStreamIcon=Icon("cantata-view-radiostream");
    wikiIcon=Icon("cantata-view-wikipedia");
    albumIcon=Icon("media-optical");
    streamIcon=Icon("applications-internet");
    configureIcon=Icon("configure");
    connectIcon=Icon("dialog-ok");
    disconnectIcon=Icon("media-eject");
    speakerIcon=Icon("speaker");
    lyricsIcon=Icon("view-media-lyrics");
    dynamicIcon=Icon("media-playlist-shuffle");
    playlistIcon=Icon("view-media-playlist");
    variousArtistsIcon=Icon("cantata-view-media-artist-various");
    artistIcon=Icon("view-media-artist");
    editIcon=Icon("document-edit");
    clearListIcon=Icon("edit-clear-list");
    repeatIcon=createRecolourableIcon("repeat", stdColor, highlightColor);
    shuffleIcon=createRecolourableIcon("shuffle", stdColor, highlightColor);
    menuIcon=createRecolourableIcon("menu", stdColor, highlightColor);
    #ifndef ENABLE_KDE_SUPPORT
    appIcon=Icon::create("cantata", QList<int>() << 16 << 22 << 32 << 48 << 64);
    shortcutsIcon=Icon("preferences-desktop-keyboard");
    if (libraryIcon.isNull()) {
        libraryIcon=Icon::create("lib", constStdSizes);
    }
    if (radioStreamIcon.isNull()) {
        radioStreamIcon=Icon::create("radio", constStdSizes);
    }
    if (wikiIcon.isNull()) {
        wikiIcon=Icon::create("wiki", constStdSizes);
    }
    if (variousArtistsIcon.isNull()) {
        variousArtistsIcon=Icon::create("va", QList<int>() << 16 << 22 << 32 << 48 << 64 << 128);
    }
    if (artistIcon.isNull()) {
        artistIcon=Icon::create("artist", QList<int>() << 16 << 22 << 32 << 48 << 64 << 128);
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
        connectIcon=Icon("gtk-stock-ok");
    }
    //if (disconnectIcon.isNull()) {
    //    disconnectIcon=Icon("media-eject");
    //}
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
    if (editIcon.isNull()) {
        editIcon=Icon("gtk-edit");
    }
    if (clearListIcon.isNull()) {
        clearListIcon=Icon("edit-delete");
    }
    #endif // Q_OS_WIN
    #endif // ENABLE_KDE_SUPPORT
}
