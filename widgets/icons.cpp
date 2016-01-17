/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QDebug>
#include "icons.h"
#include "treeview.h"
#include "config.h"
#include "gui/settings.h"
#include "support/globalstatic.h"
#include "support/utils.cpp"
#include "support/pathrequester.h"
#include "support/monoicon.h"
#include <QApplication>
#include <QPixmap>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QDir>
#include <QFile>
#include <math.h>
#if !defined Q_OS_WIN && !defined Q_OS_MAC && !defined ENABLE_UBUNTU
#include "support/gtkstyle.h"
#endif
#ifdef Q_OS_MAC
#include "support/osxstyle.h"
#endif

GLOBAL_STATIC(Icons, instance)

static QList<int> constStdSmallSizes=QList<int>() << 16 << 22 << 32 ;
static QList<int> constStdSizes=QList<int>() << constStdSmallSizes << 48; // << 64;
static QList<int> constMonoSvgSizes=QList<int>() << constStdSizes << 64;

static const int constDarkLimit=80;
static const int constDarkValue=64;
static const int constLightLimit=240;
static const int constLightValue=240;

static bool inline isLight(const QColor &col)
{
    return col.red()>100 && col.blue()>100 && col.green()>100;
}

static bool inline isVeryLight(const QColor &col, int limit=constLightValue)
{
    return col.red()>=limit && col.blue()>=limit && col.green()>=limit;
}

static bool inline isVeryDark(const QColor &col, int limit=constDarkValue)
{
    return col.red()<limit && col.blue()<limit && col.green()<limit;
}

static QColor clampColor(const QColor &color, int darkLimit=constDarkLimit, int darkValue=constDarkValue,
                         int lightLimit=constLightLimit, int lightValue=constLightValue)
{
    return isVeryLight(color, lightLimit)
            ? QColor(lightValue, lightValue, lightValue)
            : isVeryDark(color, darkLimit)
                ? QColor(darkValue, darkValue, darkValue)
                : color;
}

static QPixmap createSingleIconPixmap(int size, const QColor &col, double opacity=1.0)
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

static QPixmap createConsumeIconPixmap(int size, const QColor &col, double opacity=1.0)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    int border=2;
    if (22==size) {
        border=3;
    } else if (32==size) {
        border=4;
    } else if (48==size) {
        border=5;
    } else if (48==size) {
        border=7;
    } /*else if (64==size) {
        border=6;
    }*/
    p.setPen(QPen(col, size/8.0));
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    QRectF rect(border+0.5, border+0.5, size-(2*border), size-(2*border));
    double distanceX=fabs(cos(35.0))*(rect.width()/2);
    double distanceY=fabs(sin(35.0))*(rect.height()/2);
    double midX=rect.x()+(rect.width()/2);
    double midY=rect.y()+(rect.height()/2);
    p.drawArc(rect, 40*16, 290*16);
    p.drawLine(midX, midY, midX+distanceX, midY-distanceY);
    p.drawLine(midX, midY, midX+distanceX, midY+distanceY);
    p.drawPoint(midX, rect.y()+rect.height()/4);
    p.end();
    return pix;
}

#ifndef USE_SYSTEM_MENU_ICON
static QPixmap createMenuIconPixmap(int size, QColor col, double opacity=1.0)
{
    static const int constShadeFactor=75;

    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    int lineWidth=3;
    int space=2;
    int borderX=1;
    if (22==size) {
        lineWidth=3;
        space=3;
        borderX=2;
    } else if (32==size) {
        lineWidth=5;
        space=5;
        borderX=3;
    } else if (48==size) {
        lineWidth=8;
        space=6;
        borderX=4;
    } /*else if (64==size) {
        lineWidth=10;
        space=10;
        borderX=6;
    }*/

    int borderY=((size-((3*lineWidth)+(2*space)))/2.0)+0.5;

    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    bool light=isLight(col);
    if (light) {
        col=col.darker(constShadeFactor);
    } else {
        col=col.lighter(constShadeFactor);
    }
    for (int i=0; i<3; ++i) {
        int offset=i*(space+lineWidth);
        QRectF rect(borderX+0.5, borderY+offset, size-(2*borderX), lineWidth);
        QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
        col.setAlphaF(light ? 0.5 : 1.0);
        grad.setColorAt(0, col);
        col.setAlphaF(light ? 1.0 : 0.5);
        grad.setColorAt(1, col);
        p.fillPath(Utils::buildPath(rect, lineWidth/2.0), grad);
    }
    p.end();
    return pix;
}
#endif

static QColor calcIconColor()
{
    QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
    QColor text=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
    return clampColor(text, constDarkLimit, bgnd.value()<224 ? 32 : 48);
}

static Icon createSingleIcon(const QColor &stdColor)
{
    Icon icon;
    foreach (int s, constStdSmallSizes) {
        icon.addPixmap(createSingleIconPixmap(s, stdColor));
    }
    return icon;
}

static Icon createConsumeIcon(const QColor &stdColor)
{
    Icon icon;
    foreach (int s, constStdSmallSizes) {
        icon.addPixmap(createConsumeIconPixmap(s, stdColor));
    }
    return icon;
}

#ifndef USE_SYSTEM_MENU_ICON
static Icon createMenuIcon(const QColor &stdColor)
{
    Icon icon;
    foreach (int s, constStdSizes) {
        icon.addPixmap(createMenuIconPixmap(s, stdColor));
    }
    return icon;
}
#endif

static void recolourPix(QImage &img, const QColor &col, double opacity=1.0)
{
    unsigned char *data=img.bits();
    int numChannels=4;
    int w=img.width();
    int h=img.height();
    int stride=img.bytesPerLine();
    int r=col.red();
    int g=col.green();
    int b=col.blue();
    int width=w*numChannels;
    int offset=0;

    for(int row=0; row<h; ++row) {
        for(int column=0; column<width; column+=numChannels) {
            #if Q_BYTE_ORDER == Q_BIG_ENDIAN
            /* ARGB */
            data[offset+column]*=opacity;
            data[offset+column+1] = r;
            data[offset+column+2] = g;
            data[offset+column+3] = b;
            #else
            /* BGRA */
            data[offset+column] = b;
            data[offset+column+1] = g;
            data[offset+column+2] = r;
            data[offset+column+3]*=opacity;
            #endif
        }
        offset+=stride;
    }
}

static QPixmap recolour(const QImage &img, const QColor &col, double opacity=1.0)
{
    QImage i=img;
    if (i.depth()!=32) {
        i=i.convertToFormat(QImage::Format_ARGB32);
    }

    recolourPix(i, col, opacity);
    return QPixmap::fromImage(i);
}

static Icon createRecolourableIcon(const QString &name, const QColor &stdColor, int extraSize=24)
{
    if (QColor(Qt::black)==stdColor) {
        // Text colour is black, so is icon, therefore no need to recolour!!!
        return Icon::create(name, constStdSizes);
    }

    Icon icon;

    QList<int> sizes=QList<int>() << constStdSizes << extraSize;
    foreach (int s, sizes) {
        QImage img(QChar(':')+name+QString::number(s));
        if (!img.isNull()) {
            icon.addPixmap(recolour(img, stdColor));
        }
    }
    return icon;
}

#if !defined ENABLE_KDE_SUPPORT || defined Q_OS_MAC || defined Q_OS_WIN
#define ALWAYS_USE_MONO_ICONS
#endif

Icons::Icons()
{
    QColor stdColor=calcIconColor();
    QColor red(220, 0, 0);

    singleIcon=createSingleIcon(stdColor);
    consumeIcon=createConsumeIcon(stdColor);
    #ifdef USE_SYSTEM_MENU_ICON
    menuIcon=Icon("applications-system");
    #else
    menuIcon=createMenuIcon(stdColor);
    #endif

    QString iconFile=QString(CANTATA_SYS_ICONS_DIR+"stream.png");
    if (QFile::exists(iconFile)) {
        streamIcon.addFile(iconFile);
    }
    if (streamIcon.isNull()) {
        streamIcon=Icon("applications-internet");
    }
    podcastIcon=Icon("inode-directory");
    audioFileIcon=Icon("audio-x-generic");
    playlistFileIcon=Icon(QStringList() << "playlist" << "view-media-playlist" << "audio-x-mp3-playlist" << "audio-x-generic");
    folderIcon=Icon("inode-directory");
    dynamicRuleIcon=Icon(QStringList() << "dynamic-playlist" << "media-playlist-shuffle" << "text-x-generic");
    speakerIcon=Icon(QStringList() << "speaker" << "audio-speakers" << "gnome-volume-control");
    repeatIcon=createRecolourableIcon("repeat", stdColor);
    shuffleIcon=createRecolourableIcon("shuffle", stdColor);
    filesIcon=Icon(QStringList() << "folder-downloads" << "folder-download" << "folder" << "go-down");
    radioStreamIcon=Icon::create("radio", constStdSizes);
    addRadioStreamIcon=Icon::create("addradio", constStdSizes);
    albumIconSmall.addFile(":album32.svg");
    albumIconLarge.addFile(":album.svg");
    albumMonoIcon=MonoIcon::icon(":mono-album.svg", stdColor, stdColor);
    artistIcon=MonoIcon::icon(":artist.svg", stdColor, stdColor);
    genreIcon=MonoIcon::icon(":genre.svg", stdColor, stdColor);
    #if !defined ENABLE_KDE_SUPPORT && !defined ENABLE_UBUNTU
    appIcon=Icon("cantata");
    #endif

    lastFmIcon=MonoIcon::icon(FontAwesome::lastfmsquare, red, red);
    replacePlayQueueIcon=MonoIcon::icon(FontAwesome::play, stdColor, stdColor);
    appendToPlayQueueIcon=MonoIcon::icon(FontAwesome::plus, stdColor, stdColor);
    centrePlayQueueOnTrackIcon=MonoIcon::icon(Qt::RightToLeft==QApplication::layoutDirection() ? FontAwesome::chevronleft : FontAwesome::chevronright, stdColor, stdColor);
    savePlayQueueIcon=MonoIcon::icon(FontAwesome::save, stdColor, stdColor);
    clearListIcon=MonoIcon::icon(FontAwesome::remove, red, red);
    addNewItemIcon=MonoIcon::icon(FontAwesome::plussquare, stdColor, stdColor);
    editIcon=MonoIcon::icon(FontAwesome::edit, stdColor, stdColor);
    removeDynamicIcon=MonoIcon::icon(FontAwesome::minussquare, stdColor, stdColor);
    stopDynamicIcon=MonoIcon::icon(FontAwesome::stop, red, red);
    searchIcon=MonoIcon::icon(FontAwesome::search, stdColor, stdColor);
    addToFavouritesIcon=MonoIcon::icon(FontAwesome::heart, red, red);
    reloadIcon=MonoIcon::icon(FontAwesome::repeat, stdColor, stdColor);
    configureIcon=MonoIcon::icon(FontAwesome::cogs, stdColor, stdColor);
    connectIcon=MonoIcon::icon(FontAwesome::chevrondown, stdColor, stdColor);
    disconnectIcon=MonoIcon::icon(FontAwesome::eject, stdColor, stdColor);
    downloadIcon=MonoIcon::icon(FontAwesome::download, stdColor, stdColor);
    removeIcon=MonoIcon::icon(FontAwesome::minus, red, red);
    addIcon=MonoIcon::icon(FontAwesome::plus, stdColor, stdColor);
    addBookmarkIcon=MonoIcon::icon(FontAwesome::bookmark, stdColor, stdColor);
    audioListIcon=MonoIcon::icon(FontAwesome::music, stdColor, stdColor);
    playlistListIcon=MonoIcon::icon(FontAwesome::list, stdColor, stdColor);
    dynamicListIcon=MonoIcon::icon(FontAwesome::cube, stdColor, stdColor);
    rssListIcon=MonoIcon::icon(FontAwesome::rss, stdColor, stdColor);
    savedRssListIcon=MonoIcon::icon(FontAwesome::rsssquare, stdColor, stdColor);
    clockIcon=MonoIcon::icon(FontAwesome::clocko, stdColor, stdColor);
    folderListIcon=MonoIcon::icon(FontAwesome::foldero, stdColor, stdColor);
    streamListIcon=audioListIcon;
    streamCategoryIcon=folderListIcon;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    httpStreamIcon=MonoIcon::icon(FontAwesome::headphones, stdColor, stdColor);
    #endif
    #ifndef Q_OS_MAC
    Icon::setStd(Icon::Close, MonoIcon::icon(FontAwesome::close, red, red));
    #endif
    leftIcon=MonoIcon::icon(FontAwesome::chevronleft, stdColor, stdColor);
    rightIcon=MonoIcon::icon(FontAwesome::chevronright, stdColor, stdColor);
    upIcon=MonoIcon::icon(FontAwesome::chevronup, stdColor, stdColor);
    downIcon=MonoIcon::icon(FontAwesome::chevrondown, stdColor, stdColor);
    #ifndef ENABLE_KDE_SUPPORT
    PathRequester::setIcon(folderListIcon);
    #endif
    cancelIcon=MonoIcon::icon(FontAwesome::close, red, red);

    #if !defined ENABLE_KDE_SUPPORT && !defined Q_OS_WIN
    if (QLatin1String("gnome")==QIcon::themeName()) {
        QColor col=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
        contextIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), col, col);
    } else
    #endif
        contextIcon=Icon(QStringList() << "dialog-information" << "information");
}

void Icons::initSidebarIcons()
{
    #ifdef Q_OS_MAC
    QColor textCol=OSXStyle::self()->monoIconColor();
    QColor highlightedTexCol=OSXStyle::self()->viewPalette().highlightedText().color();
    #else
    QColor textCol=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
    QColor highlightedTexCol=QApplication::palette().color(QPalette::Active, QPalette::HighlightedText);
    #endif
    playqueueIcon=MonoIcon::icon(QLatin1String(":sidebar-playqueue"), textCol, highlightedTexCol);
    libraryIcon=MonoIcon::icon(QLatin1String(":sidebar-library"), textCol, highlightedTexCol);
    foldersIcon=MonoIcon::icon(QLatin1String(":sidebar-folders"), textCol, highlightedTexCol);
    playlistsIcon=MonoIcon::icon(QLatin1String(":sidebar-playlists"), textCol, highlightedTexCol);
    onlineIcon=MonoIcon::icon(QLatin1String(":sidebar-online"), textCol, highlightedTexCol);
    infoSidebarIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), textCol, highlightedTexCol);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesIcon=MonoIcon::icon(QLatin1String(":sidebar-devices"), textCol, highlightedTexCol);
    #endif
    searchTabIcon=MonoIcon::icon(QLatin1String(":sidebar-search"), textCol, highlightedTexCol);
}

void Icons::initToolbarIcons(const QColor &toolbarText)
{
    QColor stdColor=calcIconColor();
    bool rtl=QApplication::isRightToLeft();
    #if defined Q_OS_MAC
    QColor col=OSXStyle::self()->monoIconColor();
    #elif defined Q_OS_WIN
    QColor col=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
    #else
    QColor col=GtkStyle::isActive() ? GtkStyle::symbolicColor() : toolbarText;
    #endif
    toolbarPrevIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-next" : ":media-prev"), col, col);
    toolbarPlayIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-play-rtl" : ":media-play"), col, col);
    toolbarPauseIcon=MonoIcon::icon(QLatin1String(":media-pause"), col, col);
    toolbarStopIcon=MonoIcon::icon(QLatin1String(":media-stop"), col, col);
    toolbarNextIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-prev" : ":media-next"), col, col);
    infoIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), col, col);
    #ifdef USE_SYSTEM_MENU_ICON
    toolbarMenuIcon=MonoIcon::icon(QLatin1String(":menu-icon"), col, col);
    menuIcon=MonoIcon::icon(QLatin1String(":menu-icon"), stdColor, stdColor);
    #else
    if (col==stdColor) {
        toolbarMenuIcon=menuIcon;
    } else {
        toolbarMenuIcon=createMenuIcon(col);
    }
    #endif
}

const Icon &Icons::albumIcon(int size, bool mono) const
{
    return !mono || albumMonoIcon.isNull()
                ? size<48 ? albumIconSmall : albumIconLarge
                : albumMonoIcon;
}
