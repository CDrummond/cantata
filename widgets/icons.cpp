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

    singleIcon=MonoIcon::icon(FontAwesome::ex_one, stdColor);
    consumeIcon=MonoIcon::icon(":consume.svg", stdColor);
    #ifdef USE_SYSTEM_MENU_ICON
    menuIcon=Icon("applications-system");
    #else
    menuIcon=MonoIcon::icon(FontAwesome::bars, stdColor);
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
    repeatIcon=MonoIcon::icon(FontAwesome::refresh, stdColor);
    shuffleIcon=MonoIcon::icon(FontAwesome::random, stdColor);
    filesIcon=Icon(QStringList() << "folder-downloads" << "folder-download" << "folder" << "go-down");
    radioStreamIcon=Icon::create("radio", constStdSizes);
    addRadioStreamIcon=Icon::create("addradio", constStdSizes);
    albumIconSmall.addFile(":album32.svg");
    albumIconLarge.addFile(":album.svg");
    albumMonoIcon=MonoIcon::icon(":mono-album.svg", stdColor);
    artistIcon=MonoIcon::icon(":artist.svg", stdColor);
    genreIcon=MonoIcon::icon(":genre.svg", stdColor);
    #if !defined ENABLE_KDE_SUPPORT && !defined ENABLE_UBUNTU
    appIcon=Icon("cantata");
    #endif

    lastFmIcon=MonoIcon::icon(FontAwesome::lastfmsquare, MonoIcon::constRed, MonoIcon::constRed);
    replacePlayQueueIcon=MonoIcon::icon(FontAwesome::play, stdColor);
    appendToPlayQueueIcon=MonoIcon::icon(FontAwesome::plus, stdColor);
    centrePlayQueueOnTrackIcon=MonoIcon::icon(Qt::RightToLeft==QApplication::layoutDirection() ? FontAwesome::chevronleft : FontAwesome::chevronright, stdColor);
    savePlayQueueIcon=MonoIcon::icon(FontAwesome::save, stdColor);
    clearListIcon=MonoIcon::icon(FontAwesome::remove, MonoIcon::constRed, MonoIcon::constRed);
    addNewItemIcon=MonoIcon::icon(FontAwesome::plussquare, stdColor);
    editIcon=MonoIcon::icon(FontAwesome::edit, stdColor);
    removeDynamicIcon=MonoIcon::icon(FontAwesome::minussquare, stdColor);
    stopDynamicIcon=MonoIcon::icon(FontAwesome::stop, MonoIcon::constRed, MonoIcon::constRed);
    searchIcon=MonoIcon::icon(FontAwesome::search, stdColor);
    addToFavouritesIcon=MonoIcon::icon(FontAwesome::heart, MonoIcon::constRed, MonoIcon::constRed);
    reloadIcon=MonoIcon::icon(FontAwesome::repeat, stdColor);
    configureIcon=MonoIcon::icon(FontAwesome::cogs, stdColor);
    connectIcon=MonoIcon::icon(FontAwesome::plug, stdColor);
    disconnectIcon=MonoIcon::icon(FontAwesome::eject, stdColor);
    downloadIcon=MonoIcon::icon(FontAwesome::download, stdColor);
    removeIcon=MonoIcon::icon(FontAwesome::minus, MonoIcon::constRed, MonoIcon::constRed);
    addIcon=MonoIcon::icon(FontAwesome::plus, stdColor);
    addBookmarkIcon=MonoIcon::icon(FontAwesome::bookmark, stdColor);
    audioListIcon=MonoIcon::icon(FontAwesome::music, stdColor);
    playlistListIcon=MonoIcon::icon(FontAwesome::list, stdColor);
    dynamicListIcon=MonoIcon::icon(FontAwesome::cube, stdColor);
    rssListIcon=MonoIcon::icon(FontAwesome::rss, stdColor);
    savedRssListIcon=MonoIcon::icon(FontAwesome::rsssquare, stdColor);
    clockIcon=MonoIcon::icon(FontAwesome::clocko, stdColor);
    folderListIcon=MonoIcon::icon(FontAwesome::foldero, stdColor);
    streamListIcon=audioListIcon;
    streamCategoryIcon=folderListIcon;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    httpStreamIcon=MonoIcon::icon(FontAwesome::headphones, stdColor);
    #endif
    #ifndef Q_OS_MAC
    Icon::setStd(Icon::Close, MonoIcon::icon(FontAwesome::close, MonoIcon::constRed, MonoIcon::constRed));
    #endif
    leftIcon=MonoIcon::icon(FontAwesome::chevronleft, stdColor);
    rightIcon=MonoIcon::icon(FontAwesome::chevronright, stdColor);
    upIcon=MonoIcon::icon(FontAwesome::chevronup, stdColor);
    downIcon=MonoIcon::icon(FontAwesome::chevrondown, stdColor);
    #ifndef ENABLE_KDE_SUPPORT
    PathRequester::setIcon(folderListIcon);
    #endif
    cancelIcon=MonoIcon::icon(FontAwesome::close, MonoIcon::constRed, MonoIcon::constRed);

    #if !defined ENABLE_KDE_SUPPORT && !defined Q_OS_WIN
    if (QLatin1String("gnome")==QIcon::themeName()) {
        QColor col=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
        contextIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), col);
    } else
    #endif
        contextIcon=Icon(QStringList() << "dialog-information" << "information");
}

void Icons::initSidebarIcons()
{
    #ifdef Q_OS_MAC
    QColor textCol=OSXStyle::self()->monoIconColor();
    #else
    QColor textCol=QApplication::palette().color(QPalette::Active, QPalette::WindowText);
    #endif
    playqueueIcon=MonoIcon::icon(QLatin1String(":sidebar-playqueue"), textCol);
    libraryIcon=MonoIcon::icon(QLatin1String(":sidebar-library"), textCol);
    foldersIcon=MonoIcon::icon(QLatin1String(":sidebar-folders"), textCol);
    playlistsIcon=MonoIcon::icon(QLatin1String(":sidebar-playlists"), textCol);
    onlineIcon=MonoIcon::icon(QLatin1String(":sidebar-online"), textCol);
    infoSidebarIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), textCol);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesIcon=MonoIcon::icon(QLatin1String(":sidebar-devices"), textCol);
    #endif
    searchTabIcon=MonoIcon::icon(QLatin1String(":sidebar-search"), textCol);
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
        menuIcon=MonoIcon::icon(QLatin1String(":menu-icon"), stdColor);
        return;
    }
    #endif

    toolbarPrevIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-next" : ":media-prev"), toolbarText);
    toolbarPlayIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-play-rtl" : ":media-play"), toolbarText);
    toolbarPauseIcon=MonoIcon::icon(QLatin1String(":media-pause"), toolbarText);
    toolbarStopIcon=MonoIcon::icon(QLatin1String(":media-stop"), toolbarText);
    toolbarNextIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-prev" : ":media-next"), toolbarText);
    infoIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), toolbarText);
    #ifdef USE_SYSTEM_MENU_ICON
    toolbarMenuIcon=MonoIcon::icon(QLatin1String(":menu-icon"), toolbarText);
    menuIcon=MonoIcon::icon(QLatin1String(":menu-icon"), stdColor);
    #else
    if (toolbarText==stdColor) {
        toolbarMenuIcon=menuIcon;
    } else {
        toolbarMenuIcon=MonoIcon::icon(FontAwesome::bars, toolbarText);
    }
    #endif
}

const Icon &Icons::albumIcon(int size, bool mono) const
{
    return !mono || albumMonoIcon.isNull()
                ? size<48 ? albumIconSmall : albumIconLarge
                : albumMonoIcon;
}
