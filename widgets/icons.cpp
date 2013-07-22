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

#include "icons.h"
#include "config.h"
#include "settings.h"
#include <QApplication>
#include <QPixmap>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QDir>
#include <QFile>
#include <math.h>
#ifndef Q_OS_WIN
#include "gtkstyle.h"
#endif

#ifdef ENABLE_KDE_SUPPORT
K_GLOBAL_STATIC(Icons, instance)
#endif

Icons * Icons::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Icons *instance=0;
    if(!instance) {
        instance=new Icons;
    }
    return instance;
    #endif
}

static QList<int> constStdSizes=QList<int>() << 16 << 22 << 32 << 48;
static const double constDisabledOpacity=0.5;
static const int constShadeFactor=75;

static bool inline isLight(const QColor &col)
{
    return col.red()>100 && col.blue()>100 && col.green()>100;
}

static bool inline isVeryLight(const QColor &col, int limit=200)
{
    return col.red()>=limit && col.blue()>=limit && col.green()>=limit;
}

static bool inline isVeryDark(const QColor &col, int limit=80)
{
    return col.red()<limit && col.blue()<limit && col.green()<limit;
}

static const int constDarkLimit=80;
static const int constDarkValue=64;
static const int constLightLimit=240;
static const int constLightValue=240;

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
    }
    p.setPen(QPen(col, size/10.0));
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

static QPainterPath buildPath(const QRectF &r, double radius)
{
    QPainterPath path;
    double diameter(radius*2);

    path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
    path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
    path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
    path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
    path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
    return path;
}

static QPixmap createMenuIconPixmap(int size, QColor col, double opacity=1.0)
{
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
    }
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
        p.fillPath(buildPath(rect, lineWidth/2.0), grad);
    }
    p.end();
    return pix;
}

static void calcIconColors(QColor &stdColor, QColor &highlightColor)
{
    QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
    QColor text=QApplication::palette().color(QPalette::Active, QPalette::ButtonText);
    stdColor=clampColor(text, constDarkLimit, bgnd.value()<224 ? 32 : 48);
    highlightColor=isLight(stdColor) ? stdColor.lighter(constShadeFactor) : stdColor.darker(constShadeFactor);
}

static Icon createSingleIcon(const QColor &stdColor, const QColor &highlightColor)
{
    Icon icon;

    foreach (int s, constStdSizes) {
        icon.addPixmap(createSingleIconPixmap(s, stdColor));
        icon.addPixmap(createSingleIconPixmap(s, stdColor, constDisabledOpacity), QIcon::Disabled);
        icon.addPixmap(createSingleIconPixmap(s, highlightColor), QIcon::Active);
    }

    return icon;
}

static Icon createConsumeIcon(const QColor &stdColor, const QColor &highlightColor)
{
    Icon icon;

    foreach (int s, constStdSizes) {
        icon.addPixmap(createConsumeIconPixmap(s, stdColor));
        icon.addPixmap(createConsumeIconPixmap(s, stdColor, constDisabledOpacity), QIcon::Disabled);
        icon.addPixmap(createConsumeIconPixmap(s, highlightColor), QIcon::Active);
    }

    return icon;
}

static Icon createMenuIcon(const QColor &stdColor, const QColor &highlightColor)
{
    Icon icon;

    foreach (int s, constStdSizes) {
        icon.addPixmap(createMenuIconPixmap(s, stdColor));
        icon.addPixmap(createMenuIconPixmap(s, stdColor, constDisabledOpacity), QIcon::Disabled);
        icon.addPixmap(createMenuIconPixmap(s, highlightColor), QIcon::Active);
    }

    return icon;
}

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
            icon.addPixmap(recolour(img, stdColor));
            icon.addPixmap(recolour(img, stdColor, constDisabledOpacity), QIcon::Disabled);
            icon.addPixmap(recolour(img, highlightColor), QIcon::Active);
        }
    }
    return icon;
}

static QColor stdColor;
static QColor highlightColor;

static void updateSidebarIcon(Icon &i, const QString &name, const QColor &color, QIcon::Mode mode)
{
    QColor adjusted=color;
    int darkValue=constDarkValue;
    int lightValue=constLightValue;

    if (isVeryDark(adjusted)) {
        QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
        if (bgnd.value()<224) {
            darkValue=48;
        }
    } else if (isVeryLight(adjusted)) {
        QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
        if (bgnd.value()<224) {
            lightValue=232;
        }
    }

    if (darkValue==constDarkValue && isVeryDark(adjusted)) {
        i.addFile(":sidebar-"+name+"-dark", QSize(), mode);
    } else if (lightValue==constLightValue && isVeryLight(adjusted)) {
        i.addFile(":sidebar-"+name+"-light", QSize(), mode);
    } else { // Neither black nor white, so we need to recolour...
        Icon std;
        std.addFile(":sidebar-"+name+"-dark", QSize(), mode);
        // Now recolour the icon!
        QList<int> sizes=QList<int>() << 16 << 22 << 32 << 48 << 64;
        QColor col=clampColor(adjusted, constDarkLimit, darkValue, constLightLimit, lightValue);
        foreach (int s, sizes) {
            QImage img=std.pixmap(s, s, mode).toImage().convertToFormat(QImage::Format_ARGB32);
            recolourPix(img, col);
            i.addPixmap(QPixmap::fromImage(img), mode);
        }
    }
}

static Icon loadSidebarIcon(const QString &name, const QColor &normal, const QColor &selected)
{
    Icon i;
    updateSidebarIcon(i, name, normal, QIcon::Normal);
    if (normal!=selected) {
        updateSidebarIcon(i, name, selected, QIcon::Selected);
    }
    return i;
}

Icons::Icons()
{
    calcIconColors(stdColor, highlightColor);
    singleIcon=createSingleIcon(stdColor, highlightColor);
    consumeIcon=createConsumeIcon(stdColor, highlightColor);
    menuIcon=createMenuIcon(stdColor, highlightColor);
    libraryIcon=Icon("cantata-view-media-library");
    streamCategoryIcon=Icon(QLatin1String("oxygen")==Icon::currentTheme().toLower() ? "inode-directory" : "folder-music");
    radioStreamIcon=Icon("cantata-view-radiostream");
    addRadioStreamIcon=Icon("cantata-radiostream-add");
    QString iconFile=QString(INSTALL_PREFIX"/share/")+QCoreApplication::applicationName()+"/streamicons/stream.png";
    if (QFile::exists(iconFile)) {
        streamIcon.addFile(iconFile);
    }
    if (streamIcon.isNull()) {
        streamIcon=Icon("applications-internet");
    }
    artistIcon=Icon("view-media-artist");
    albumIcon=Icon("media-optical");
    audioFileIcon=Icon("audio-x-generic");
    playlistIcon=Icon("view-media-playlist");
    dynamicRuleIcon=Icon("media-playlist-shuffle");
    configureIcon=Icon("configure");
    connectIcon=Icon("dialog-ok");
    disconnectIcon=Icon("media-eject");
    speakerIcon=Icon("speaker");
    variousArtistsIcon=Icon("cantata-view-media-artist-various");
    editIcon=Icon("document-edit");
    searchIcon=Icon("edit-find");
    clearListIcon=Icon("edit-clear-list");
    repeatIcon=createRecolourableIcon("repeat", stdColor, highlightColor);
    shuffleIcon=createRecolourableIcon("shuffle", stdColor, highlightColor);
    magnatuneIcon.addFile(":magnatune.svg");
    soundCloudIcon.addFile(":soundcloud.svg");
    jamendoIcon.addFile(":jamendo.svg");
    filesIcon=Icon("document-multiple");
    cancelIcon=Icon("dialog-cancel");
    importIcon=Icon("document-import");
    if (editIcon.isNull()) {
        editIcon=Icon("text-editor");
    }
    if (importIcon.isNull()) {
        importIcon=Icon("down");
    }
    #ifndef ENABLE_KDE_SUPPORT
    appIcon.addFile(":cantata.svg");
    QList<int> appSizes=QList<int>() << 16 << 22 << 32 << 48 << 64;
    foreach (int s, appSizes) {
        appIcon.addFile(QString(":cantata%1.png").arg(s), QSize(s, s));
    }

    shortcutsIcon=Icon("preferences-desktop-keyboard");
    if (libraryIcon.isNull()) {
        libraryIcon=Icon::create("lib", constStdSizes);
    }
    if (radioStreamIcon.isNull()) {
        radioStreamIcon=Icon::create("radio", constStdSizes);
    }
    if (addRadioStreamIcon.isNull()) {
        addRadioStreamIcon=Icon::create("addradio", constStdSizes);
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
    if (dynamicRuleIcon.isNull()) {
        dynamicRuleIcon=Icon("text-x-generic");
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
        clearListIcon=Icon("edit-clear");
    }
    if (filesIcon.isNull()) {
        filesIcon=Icon("empty");
    }
    if (cancelIcon.isNull()) {
        cancelIcon=Icon("gtk-cancel");
    }
    #endif // Q_OS_WIN
    #endif // ENABLE_KDE_SUPPORT

    if (streamCategoryIcon.isNull()) {
        streamCategoryIcon=libraryIcon;
    }
}

void Icons::initSidebarIcons()
{
    if (Settings::self()->monoSidebarIcons()) {
        QColor textCol=QApplication::palette().color(QPalette::Active, QPalette::ButtonText);
        QColor highlightedTexCol=QApplication::palette().color(QPalette::Active, QPalette::HighlightedText);
        playqueueIcon=loadSidebarIcon("playqueue", textCol, highlightedTexCol);
        artistsIcon=loadSidebarIcon("artists", textCol, highlightedTexCol);
        albumsIcon=loadSidebarIcon("albums", textCol, highlightedTexCol);
        foldersIcon=loadSidebarIcon("folders", textCol, highlightedTexCol);
        playlistsIcon=loadSidebarIcon("playlists", textCol, highlightedTexCol);
        dynamicIcon=loadSidebarIcon("dynamic", textCol, highlightedTexCol);
        streamsIcon=loadSidebarIcon("streams", textCol, highlightedTexCol);
        onlineIcon=loadSidebarIcon("online", textCol, highlightedTexCol);
        infoSidebarIcon=loadSidebarIcon("info", textCol, highlightedTexCol);
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesIcon=loadSidebarIcon("devices", textCol, highlightedTexCol);
        #endif
    } else {
        playqueueIcon=Icon("media-playback-start");
        artistsIcon=artistIcon;
        albumsIcon=albumIcon;
        foldersIcon=Icon("inode-directory");
        playlistsIcon=playlistIcon;
        dynamicIcon=dynamicRuleIcon;
        streamsIcon=radioStreamIcon;
        onlineIcon=Icon("applications-internet");
        if (QLatin1String("gnome")==Icon::currentTheme().toLower()) {
            QColor col=QApplication::palette().color(QPalette::Active, QPalette::ButtonText);
            infoSidebarIcon=loadSidebarIcon("info", col, col);
        } else {
            infoSidebarIcon=Icon("dialog-information");
        }
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesIcon=Icon("multimedia-player");
        #endif
    }
}

#if !defined ENABLE_KDE_SUPPORT && !defined Q_OS_WIN
// For some reason, the -symbolic icons on Ubuntu have a lighter colour when disabled!
// This looks odd to me, so base the disabled icon on the enabled version but with opacity
// set to default value...
static void setDisabledOpacity(Icon &icon)
{
    Icon copy;
    for (int i=0; i<2; ++i) {
        QIcon::State state=(QIcon::State)i;
        for (int j=0; j<4; ++j) {
            QIcon::Mode mode=(QIcon::Mode)j;
            QList<int> sizes=constStdSizes;
            foreach (const int sz, sizes) {
                if (QIcon::Disabled==mode) {
                    QPixmap pix=icon.pixmap(QSize(sz, sz), QIcon::Normal, state);
                    if (!pix.isNull()) {
                        QPixmap dis(sz, sz);
                        dis.fill(Qt::transparent);
                        QPainter p(&dis);
                        p.setOpacity(constDisabledOpacity);
                        p.drawPixmap(0, 0, pix);
                        p.end();
                        copy.addPixmap(dis, mode, state);
                    }
                } else {
                    copy.addPixmap(icon.pixmap(QSize(sz, sz), mode, state), mode, state);
                }
            }
        }
    }
    icon=copy;
}
#else
#define setDisabledOpacity(A) ;
#endif

void Icons::initToolbarIcons(const QColor &color, bool forceLight)
{
    bool light=forceLight || isLight(color);

    if (light) {
        QColor col(Qt::white);
        QColor highlight(col.darker(constShadeFactor));
        toolbarMenuIcon=createMenuIcon(col, highlight);
    } else {
        toolbarMenuIcon=menuIcon;
    }

    #ifndef Q_OS_WIN
    if (light && GtkStyle::useSymbolicIcons()) {
        toolbarPrevIcon=Icon("media-skip-backward-symbolic");
        toolbarPlayIcon=Icon("media-playback-start-symbolic");
        toolbarPauseIcon=Icon("media-playback-pause-symbolic");
        toolbarStopIcon=Icon("media-playback-stop-symbolic");
        toolbarNextIcon=Icon("media-skip-forward-symbolic");
        toolbarVolumeMutedIcon=Icon("audio-volume-muted-symbolic");
        toolbarVolumeLowIcon=Icon("audio-volume-low-symbolic");
        toolbarVolumeMediumIcon=Icon("audio-volume-medium-symbolic");
        toolbarVolumeHighIcon=Icon("audio-volume-high-symbolic");
        QColor col(196, 196, 196);
        infoIcon=loadSidebarIcon("info", col, col);
    } else
    #endif
        if (QLatin1String("gnome")==Icon::currentTheme().toLower()) {
            QColor col=QApplication::palette().color(QPalette::Active, QPalette::ButtonText);
            infoIcon=loadSidebarIcon("info", col, col);
        }

    if (infoIcon.isNull()) {
        infoIcon=Icon("dialog-information");
    }

    #if !defined ENABLE_KDE_SUPPORT && !defined Q_OS_WIN
    if (QLatin1String("gnome")==Icon::currentTheme().toLower()) {
        QColor col=QApplication::palette().color(QPalette::Active, QPalette::ButtonText);
        contextIcon=loadSidebarIcon("info", col, col);
    } else
    #endif
        contextIcon=Icon("dialog-information");

    if (toolbarPrevIcon.isNull()) {
        toolbarPrevIcon=Icon::getMediaIcon("media-skip-backward");
    } else {
        setDisabledOpacity(toolbarPrevIcon);
    }
    if (toolbarPlayIcon.isNull()) {
        toolbarPlayIcon=Icon::getMediaIcon("media-playback-start");
    } else {
        setDisabledOpacity(toolbarPlayIcon);
    }
    if (toolbarPauseIcon.isNull()) {
        toolbarPauseIcon=Icon::getMediaIcon("media-playback-pause");
    } else {
        setDisabledOpacity(toolbarPauseIcon);
    }
    if (toolbarStopIcon.isNull()) {
        toolbarStopIcon=Icon::getMediaIcon("media-playback-stop");
    } else {
        setDisabledOpacity(toolbarStopIcon);
    }
    if (toolbarNextIcon.isNull()) {
        toolbarNextIcon=Icon::getMediaIcon("media-skip-forward");
    } else {
        setDisabledOpacity(toolbarNextIcon);
    }
    if (toolbarVolumeMutedIcon.isNull()) {
        toolbarVolumeMutedIcon=Icon("audio-volume-muted");
    }
    if (toolbarVolumeLowIcon.isNull()) {
        toolbarVolumeLowIcon=Icon("audio-volume-low");
    }
    if (toolbarVolumeMediumIcon.isNull()) {
        toolbarVolumeMediumIcon=Icon("audio-volume-medium");
    }
    if (toolbarVolumeHighIcon.isNull()) {
        toolbarVolumeHighIcon=Icon("audio-volume-high");
    }
}
