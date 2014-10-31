/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "treeview.h"
#include "config.h"
#include "gui/settings.h"
#include "support/globalstatic.h"
#include "support/utils.cpp"
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

static void updateMonoSvgIcon(Icon &i, const QString &type, const QString &name, const QColor &color, QIcon::Mode mode)
{
    int darkValue=constDarkValue;
    int lightValue=constLightValue;

    if (isVeryDark(color)) {
        QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
        if (bgnd.value()<224) {
            darkValue=48;
        }
    } else if (isVeryLight(color)) {
        QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
        if (bgnd.value()<224) {
            lightValue=232;
        }
    }

    if (darkValue==constDarkValue && isVeryDark(color)) {
        i.addFile(":"+type+"-"+name+"-dark", QSize(), mode);
    } else if (lightValue==constLightValue && isVeryLight(color)) {
        i.addFile(":"+type+"-"+name+"-light", QSize(), mode);
    } else { // Neither black nor white, so we need to recolour...
        Icon std;
        std.addFile(":"+type+"-"+name+"-dark", QSize(), mode);
        // Now recolour the icon!
        QColor col=clampColor(color, constDarkLimit, darkValue, constLightLimit, lightValue);
        foreach (int s, constMonoSvgSizes) {
            QImage img=std.pixmap(s, s, mode).toImage().convertToFormat(QImage::Format_ARGB32);
            recolourPix(img, col);
            i.addPixmap(QPixmap::fromImage(img), mode);
        }
    }
}

static Icon loadMonoSvgIcon(const QString &type, const QString &name, const QColor &normal, const QColor &selected)
{
    Icon i;
    updateMonoSvgIcon(i, type, name, normal, QIcon::Normal);
    if (normal!=selected) {
        updateMonoSvgIcon(i, type, name, selected, QIcon::Selected);
    }
    return i;
}

static Icon loadSidebarIcon(const QString &name, const QColor &normal, const QColor &selected)
{
    return loadMonoSvgIcon(QLatin1String("sidebar"), name, normal, selected);
}

#if !defined Q_OS_WIN && !defined ENABLE_UBUNTU
static void setDisabledOpacity(Icon &icon)
{
    static const double constDisabledOpacity=0.5;
    Icon copy;
    for (int i=0; i<2; ++i) {
        QIcon::State state=(QIcon::State)i;
        for (int j=0; j<4; ++j) {
            QIcon::Mode mode=(QIcon::Mode)j;
            foreach (const int sz, constMonoSvgSizes) {
                if (QIcon::Disabled==mode) {
                    QPixmap pix=icon.pixmap(QSize(sz, sz), QIcon::Normal, state);
                    if (!pix.isNull()) {
                        copy.addPixmap(QPixmap::fromImage(TreeView::setOpacity(pix.toImage(), constDisabledOpacity)), mode, state);
                    }
                } else {
                    copy.addPixmap(icon.pixmap(QSize(sz, sz), mode, state), mode, state);
                }
            }
        }
    }
    icon=copy;
}

static Icon loadMediaIcon(const QString &name, const QColor &normal, const QColor &selected)
{
    Icon icon=loadMonoSvgIcon(QLatin1String("media"), name, normal, selected);
    setDisabledOpacity(icon);
    return icon;
}
#endif

Icons::Icons()
{
    QColor stdColor=calcIconColor();
    singleIcon=createSingleIcon(stdColor);
    consumeIcon=createConsumeIcon(stdColor);
    #ifdef USE_SYSTEM_MENU_ICON
    menuIcon=Icon("applications-system");
    #else
    menuIcon=createMenuIcon(stdColor);
    #endif

    #ifdef ENABLE_STREAMS
    streamCategoryIcon=Icon(QLatin1String("oxygen")==Icon::currentTheme().toLower() ? "inode-directory" : "folder-music");
    #endif

    QString iconFile=QString(CANTATA_SYS_ICONS_DIR+"stream.png");
    if (QFile::exists(iconFile)) {
        streamIcon.addFile(iconFile);
    }
    if (streamIcon.isNull()) {
        streamIcon=Icon("applications-internet");
    }
    albumIcon=Icon("media-optical");
    podcastIcon=Icon("inode-directory");
    downloadedPodcastEpisodeIcon=Icon("document-save-as");
    audioFileIcon=Icon("audio-x-generic");
    playlistIcon=Icon("view-media-playlist");
    folderIcon=Icon("inode-directory");
    dynamicRuleIcon=Icon("media-playlist-shuffle");
    configureIcon=Icon("configure");
    connectIcon=Icon("dialog-ok");
    disconnectIcon=Icon("media-eject");
    speakerIcon=Icon("speaker");
    editIcon=Icon("document-edit");
    searchIcon=Icon("edit-find");
    clearListIcon=Icon("edit-clear-list");
    repeatIcon=createRecolourableIcon("repeat", stdColor);
    shuffleIcon=createRecolourableIcon("shuffle", stdColor);
    filesIcon=Icon("document-multiple");
    cancelIcon=Icon("dialog-cancel");
    importIcon=Icon("document-import");
    if (editIcon.isNull()) {
        editIcon=Icon("text-editor");
    }
    if (importIcon.isNull()) {
        importIcon=Icon("down");
    }
    radioStreamIcon=Icon::create("radio", constStdSizes);
    addRadioStreamIcon=Icon::create("addradio", constStdSizes);
    artistIcon.addFile(":artist.svg");
    lastFmIcon.addFile(":lastfm.svg");
    #ifndef ENABLE_KDE_SUPPORT
    #ifndef ENABLE_UBUNTU
    appIcon=Icon("cantata");
    #endif // ENABLE_UBUNTU
    shortcutsIcon=Icon("preferences-desktop-keyboard");
    #if !defined Q_OS_WIN && !defined Q_OS_MAC && !defined ENABLE_UBUNTU
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
        if (connectIcon.isNull()) {
            connectIcon=Icon("go-bottom");
        }
    }
    if (speakerIcon.isNull()) {
        speakerIcon=Icon("audio-speakers");
        if (speakerIcon.isNull()) {
            speakerIcon=Icon("gnome-volume-control");
        }
    }
    #ifdef ENABLE_DYNAMIC
    if (dynamicRuleIcon.isNull()) {
        dynamicRuleIcon=Icon("text-x-generic");
    }
    #endif
    if (playlistIcon.isNull()) {
        playlistIcon=Icon("audio-x-mp3-playlist");
        if (playlistIcon.isNull()) {
            playlistIcon=audioFileIcon;
        }
    }
    if (editIcon.isNull()) {
        editIcon=Icon("gtk-edit");
    }
    if (clearListIcon.isNull()) {
        clearListIcon=Icon("gtk-delete");
    }
    if (filesIcon.isNull()) {
        filesIcon=Icon("empty");
    }
    if (cancelIcon.isNull()) {
        cancelIcon=Icon("gtk-cancel");
    }
    #endif // Q_OS_WIN && Q_OS_MAC
    #endif // ENABLE_KDE_SUPPORT

    #ifdef ENABLE_STREAMS
    if (streamCategoryIcon.isNull()) {
        streamCategoryIcon=folderIcon;
    }
    #endif
}

static bool monoSb=true;
bool Icons::monoSidebarIcons()
{
    return monoSb;
}

void Icons::initSidebarIcons()
{
    if (Settings::self()->monoSidebarIcons()) {
        monoSb=true;
        #ifdef Q_OS_MAC
        QColor textCol=OSXStyle::self()->monoIconColor();
        QColor highlightedTexCol=OSXStyle::self()->viewPalette().highlightedText().color();
        #else
        QColor textCol=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
        QColor highlightedTexCol=QApplication::palette().color(QPalette::Active, QPalette::HighlightedText);
        #endif
        playqueueIcon=loadSidebarIcon(QLatin1String("playqueue"), textCol, highlightedTexCol);
        artistsIcon=loadSidebarIcon(QLatin1String("artists"), textCol, highlightedTexCol);
        albumsIcon=loadSidebarIcon(QLatin1String("albums"), textCol, highlightedTexCol);
        foldersIcon=loadSidebarIcon(QLatin1String("folders"), textCol, highlightedTexCol);
        playlistsIcon=loadSidebarIcon(QLatin1String("playlists"), textCol, highlightedTexCol);
        #ifdef ENABLE_DYNAMIC
        dynamicIcon=loadSidebarIcon(QLatin1String("dynamic"), textCol, highlightedTexCol);
        #endif
        #ifdef ENABLE_STREAMS
        streamsIcon=loadSidebarIcon(QLatin1String("streams"), textCol, highlightedTexCol);
        #endif
        #ifdef ENABLE_ONLINE_SERVICES
        onlineIcon=loadSidebarIcon(QLatin1String("online"), textCol, highlightedTexCol);
        #endif
        infoSidebarIcon=loadSidebarIcon(QLatin1String("info"), textCol, highlightedTexCol);
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesIcon=loadSidebarIcon(QLatin1String("devices"), textCol, highlightedTexCol);
        #endif
        searchTabIcon=loadSidebarIcon(QLatin1String("search"), textCol, highlightedTexCol);
    } else {
        monoSb=false;
        playqueueIcon=Icon("media-playback-start");
        artistsIcon=artistIcon;
        albumsIcon=albumIcon;
        foldersIcon=Icon("inode-directory");
        playlistsIcon=playlistIcon;
        #ifdef ENABLE_DYNAMIC
        dynamicIcon=dynamicRuleIcon;
        #endif
        #ifdef ENABLE_STREAMS
        streamsIcon=radioStreamIcon;
        #endif
        #ifdef ENABLE_ONLINE_SERVICES
        onlineIcon=Icon("applications-internet");
        #endif
        if (QLatin1String("gnome")==Icon::currentTheme().toLower()) {
            QColor col=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
            infoSidebarIcon=loadSidebarIcon("info", col, col);
        } else {
            infoSidebarIcon=Icon("dialog-information");
        }
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesIcon=Icon("multimedia-player");
        #endif
        searchTabIcon=searchIcon;
    }
}

void Icons::initToolbarIcons(const QColor &toolbarText)
{
    #ifdef USE_SYSTEM_MENU_ICON
    Q_UNUSED(toolbarText)
    #endif
    QColor stdColor=calcIconColor();
    #if !defined Q_OS_WIN && !defined ENABLE_UBUNTU
    #ifndef Q_OS_MAC
    if (GtkStyle::useSymbolicIcons()) {
    #endif
        bool rtl=QApplication::isRightToLeft();
        #ifdef Q_OS_MAC
        QColor col=OSXStyle::self()->monoIconColor();
        #else
        QColor col=GtkStyle::symbolicColor();
        #endif
        toolbarPrevIcon=loadMediaIcon(QLatin1String(rtl ? "next" : "prev"), col, col);
        toolbarPlayIcon=loadMediaIcon(QLatin1String(rtl ? "play-rtl" : "play"), col, col);
        toolbarPauseIcon=loadMediaIcon(QLatin1String("pause"), col, col);
        toolbarStopIcon=loadMediaIcon(QLatin1String("stop"), col, col);
        toolbarNextIcon=loadMediaIcon(QLatin1String(rtl ? "prev" : "next"), col, col);
        infoIcon=loadSidebarIcon("info", col, col);
        #ifdef USE_SYSTEM_MENU_ICON
        toolbarMenuIcon=loadMonoSvgIcon(QLatin1String("menu"), QLatin1String("icon"), col, col);
        menuIcon=loadMonoSvgIcon(QLatin1String("menu"), QLatin1String("icon"), stdColor, stdColor);
        #else
        if (col==stdColor) {
            toolbarMenuIcon=menuIcon;
        } else {
            toolbarMenuIcon=createMenuIcon(col);
        }
        #endif
    #ifndef Q_OS_MAC
    } else
    #endif
    #endif
    {
        #ifdef USE_SYSTEM_MENU_ICON
        toolbarMenuIcon=menuIcon;
        #else
        if (toolbarText==stdColor) {
            toolbarMenuIcon=menuIcon;
        } else {
            toolbarMenuIcon=createMenuIcon(toolbarText);
        }
        #endif
        if (QLatin1String("gnome")==Icon::currentTheme().toLower()) {
            QColor col=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
            infoIcon=loadSidebarIcon("info", col, col);
        }
    }

    if (infoIcon.isNull()) {
        infoIcon=Icon("dialog-information");
    }

    #if !defined ENABLE_KDE_SUPPORT && !defined Q_OS_WIN
    if (QLatin1String("gnome")==Icon::currentTheme().toLower()) {
        QColor col=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
        contextIcon=loadSidebarIcon("info", col, col);
    } else
    #endif
        contextIcon=Icon("dialog-information");

    if (toolbarPrevIcon.isNull()) {
        toolbarPrevIcon=Icon::getMediaIcon("media-skip-backward");
    }
    if (toolbarPlayIcon.isNull()) {
        toolbarPlayIcon=Icon::getMediaIcon("media-playback-start");
    }
    if (toolbarPauseIcon.isNull()) {
        toolbarPauseIcon=Icon::getMediaIcon("media-playback-pause");
    }
    if (toolbarStopIcon.isNull()) {
        toolbarStopIcon=Icon::getMediaIcon("media-playback-stop");
    }
    if (toolbarNextIcon.isNull()) {
        toolbarNextIcon=Icon::getMediaIcon("media-skip-forward");
    }
}
