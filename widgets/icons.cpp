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

#include "icons.h"
#include "gui/settings.h"
#include "support/globalstatic.h"
#include "support/utils.h"
#include "support/pathrequester.h"
#include "support/monoicon.h"
#if !defined Q_OS_WIN && !defined Q_OS_MAC && !defined ENABLE_UBUNTU
#include "support/gtkstyle.h"
#endif
#ifdef Q_OS_MAC
#include "support/osxstyle.h"
#endif
#include <QApplication>
#include <QPalette>
#include <QDir>
#include <QFile>

GLOBAL_STATIC(Icons, instance)

static QList<int> constStdSizes=QList<int>() << 16 << 22 << 32 << 48; // << 64;

static const int constDarkLimit=80;
static const int constDarkValue=64;
static const int constLightLimit=240;
static const int constLightValue=240;

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

static QColor calcIconColor()
{
    QColor bgnd=QApplication::palette().color(QPalette::Active, QPalette::Background);
    QColor text=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
    return clampColor(text, constDarkLimit, bgnd.value()<224 ? 32 : 48);
}

#if !defined ENABLE_KDE_SUPPORT || defined Q_OS_MAC || defined Q_OS_WIN
#define ALWAYS_USE_MONO_ICONS
#endif

Icons::Icons()
{
    QColor stdColor=calcIconColor();
    QColor selColor=QApplication::palette().color(QPalette::Active, QPalette::HighlightedText);
    QColor red(220, 0, 0);

    singleIcon=MonoIcon::icon(FontAwesome::ex_one, stdColor, selColor);
    consumeIcon=MonoIcon::icon(":consume.svg", stdColor, selColor);
    #ifdef USE_SYSTEM_MENU_ICON
    menuIcon=Icon("applications-system");
    #else
    menuIcon=MonoIcon::icon(FontAwesome::bars, stdColor, selColor);
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
    repeatIcon=MonoIcon::icon(FontAwesome::refresh, stdColor, selColor);
    shuffleIcon=MonoIcon::icon(FontAwesome::random, stdColor, selColor);
    filesIcon=Icon(QStringList() << "folder-downloads" << "folder-download" << "folder" << "go-down");
    radioStreamIcon=Icon::create("radio", constStdSizes);
    addRadioStreamIcon=Icon::create("addradio", constStdSizes);
    albumIconSmall.addFile(":album32.svg");
    albumIconLarge.addFile(":album.svg");
    albumMonoIcon=MonoIcon::icon(":mono-album.svg", stdColor, selColor);
    artistIcon=MonoIcon::icon(":artist.svg", stdColor, selColor);
    genreIcon=MonoIcon::icon(":genre.svg", stdColor, selColor);
    #if !defined ENABLE_KDE_SUPPORT && !defined ENABLE_UBUNTU
    appIcon=Icon("cantata");
    #endif

    lastFmIcon=MonoIcon::icon(FontAwesome::lastfmsquare, red, red);
    replacePlayQueueIcon=MonoIcon::icon(FontAwesome::play, stdColor, selColor);
    appendToPlayQueueIcon=MonoIcon::icon(FontAwesome::plus, stdColor, selColor);
    centrePlayQueueOnTrackIcon=MonoIcon::icon(Qt::RightToLeft==QApplication::layoutDirection() ? FontAwesome::chevronleft : FontAwesome::chevronright, stdColor, selColor);
    savePlayQueueIcon=MonoIcon::icon(FontAwesome::save, stdColor, selColor);
    clearListIcon=MonoIcon::icon(FontAwesome::remove, red, red);
    addNewItemIcon=MonoIcon::icon(FontAwesome::plussquare, stdColor, selColor);
    editIcon=MonoIcon::icon(FontAwesome::edit, stdColor, selColor);
    removeDynamicIcon=MonoIcon::icon(FontAwesome::minussquare, stdColor, selColor);
    stopDynamicIcon=MonoIcon::icon(FontAwesome::stop, red, red);
    searchIcon=MonoIcon::icon(FontAwesome::search, stdColor, selColor);
    addToFavouritesIcon=MonoIcon::icon(FontAwesome::heart, red, red);
    reloadIcon=MonoIcon::icon(FontAwesome::repeat, stdColor, selColor);
    configureIcon=MonoIcon::icon(FontAwesome::cogs, stdColor, selColor);
    connectIcon=MonoIcon::icon(FontAwesome::plug, stdColor, selColor);
    disconnectIcon=MonoIcon::icon(FontAwesome::eject, stdColor, selColor);
    downloadIcon=MonoIcon::icon(FontAwesome::download, stdColor, selColor);
    removeIcon=MonoIcon::icon(FontAwesome::minus, red, red);
    addIcon=MonoIcon::icon(FontAwesome::plus, stdColor, selColor);
    addBookmarkIcon=MonoIcon::icon(FontAwesome::bookmark, stdColor, selColor);
    audioListIcon=MonoIcon::icon(FontAwesome::music, stdColor, selColor);
    playlistListIcon=MonoIcon::icon(FontAwesome::list, stdColor, selColor);
    dynamicListIcon=MonoIcon::icon(FontAwesome::cube, stdColor, selColor);
    rssListIcon=MonoIcon::icon(FontAwesome::rss, stdColor, selColor);
    savedRssListIcon=MonoIcon::icon(FontAwesome::rsssquare, stdColor, selColor);
    clockIcon=MonoIcon::icon(FontAwesome::clocko, stdColor, selColor);
    folderListIcon=MonoIcon::icon(FontAwesome::foldero, stdColor, selColor);
    streamListIcon=audioListIcon;
    streamCategoryIcon=folderListIcon;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    httpStreamIcon=MonoIcon::icon(FontAwesome::headphones, stdColor, selColor);
    #endif
    #ifndef Q_OS_MAC
    Icon::setStd(Icon::Close, MonoIcon::icon(FontAwesome::close, red, red));
    #endif
    leftIcon=MonoIcon::icon(FontAwesome::chevronleft, stdColor, selColor);
    rightIcon=MonoIcon::icon(FontAwesome::chevronright, stdColor, selColor);
    upIcon=MonoIcon::icon(FontAwesome::chevronup, stdColor, selColor);
    downIcon=MonoIcon::icon(FontAwesome::chevrondown, stdColor, selColor);
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

    #ifdef Q_OS_LINUX
    if (Settings::self()->useStandardIcons()) {
        toolbarPrevIcon=Icon::getMediaIcon("media-skip-backward");
        toolbarPlayIcon=Icon::getMediaIcon("media-playback-start");
        toolbarPauseIcon=Icon::getMediaIcon("media-playback-pause");
        toolbarStopIcon=Icon::getMediaIcon("media-playback-stop");
        toolbarNextIcon=Icon::getMediaIcon("media-skip-forward");
        infoIcon=Icon("dialog-information");
        toolbarMenuIcon=Icon("applications-system");
        menuIcon=MonoIcon::icon(QLatin1String(":menu-icon"), stdColor, stdColor);
        return;
    }
    #endif

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
        toolbarMenuIcon=MonoIcon::icon(FontAwesome::bars, col, col);
    }
    #endif
}

const Icon &Icons::albumIcon(int size, bool mono) const
{
    return !mono || albumMonoIcon.isNull()
                ? size<48 ? albumIconSmall : albumIconLarge
                : albumMonoIcon;
}
