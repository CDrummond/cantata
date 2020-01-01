/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/monoicon.h"
#if !defined Q_OS_WIN && !defined Q_OS_MAC
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

#if defined Q_OS_MAC || defined Q_OS_WIN
#define ALWAYS_USE_MONO_ICONS
#endif

Icons::Icons()
{
    QColor stdColor=Utils::monoIconColor();

    singleIcon=MonoIcon::icon(FontAwesome::ex_one, stdColor);
    consumeIcon=MonoIcon::icon(":consume.svg", stdColor);
    menuIcon=MonoIcon::icon(FontAwesome::bars, stdColor);

    QString iconFile=QString(CANTATA_SYS_ICONS_DIR+"stream.png");
    if (QFile::exists(iconFile)) {
        streamIcon.addFile(iconFile);
    }
    if (streamIcon.isNull()) {
        streamIcon=MonoIcon::icon(FontAwesome::globe, stdColor);
    }
    podcastIcon=MonoIcon::icon(FontAwesome::podcast, stdColor);
    repeatIcon=MonoIcon::icon(FontAwesome::refresh, stdColor);
    shuffleIcon=MonoIcon::icon(FontAwesome::random, stdColor);
    albumIconSmall.addFile(":album32.svg");
    albumIconLarge.addFile(":album.svg");
    albumMonoIcon=MonoIcon::icon(":mono-album.svg", stdColor);
    artistIcon=MonoIcon::icon(":artist.svg", stdColor);
    genreIcon=MonoIcon::icon(":genre.svg", stdColor);
    #if defined Q_OS_MAC || defined Q_OS_WIN
    appIcon.addFile(":cantata.svg");
    appIcon.addFile(":cantata16.png");
    appIcon.addFile(":cantata22.png");
    appIcon.addFile(":cantata24.png");
    appIcon.addFile(":cantata32.png");
    appIcon.addFile(":cantata48.png");
    appIcon.addFile(":cantata64.png");
    appIcon.addFile(":cantata128.png");
    #else
    appIcon=QIcon::fromTheme("cantata");
    #endif

    replacePlayQueueIcon=MonoIcon::icon(FontAwesome::play, stdColor);
    appendToPlayQueueIcon=MonoIcon::icon(FontAwesome::plus, stdColor);
    centrePlayQueueOnTrackIcon=MonoIcon::icon(Qt::RightToLeft==QApplication::layoutDirection() ? FontAwesome::chevronleft : FontAwesome::chevronright, stdColor);
    savePlayQueueIcon=MonoIcon::icon(FontAwesome::save, stdColor);
    cutIcon=MonoIcon::icon(FontAwesome::remove, MonoIcon::constRed, MonoIcon::constRed);
    addNewItemIcon=MonoIcon::icon(FontAwesome::plussquare, stdColor);
    editIcon=MonoIcon::icon(FontAwesome::edit, stdColor);
    stopDynamicIcon=MonoIcon::icon(FontAwesome::stop, MonoIcon::constRed, MonoIcon::constRed);
    searchIcon=MonoIcon::icon(FontAwesome::search, stdColor);
    addToFavouritesIcon=MonoIcon::icon(FontAwesome::heart, MonoIcon::constRed, MonoIcon::constRed);
    reloadIcon=MonoIcon::icon(FontAwesome::repeat, stdColor);
    configureIcon=MonoIcon::icon(FontAwesome::cogs, stdColor);
    connectIcon=MonoIcon::icon(FontAwesome::plug, stdColor);
    disconnectIcon=MonoIcon::icon(FontAwesome::eject, stdColor);
    downloadIcon=MonoIcon::icon(FontAwesome::download, stdColor);
    removeIcon=MonoIcon::icon(FontAwesome::minussquare, MonoIcon::constRed, MonoIcon::constRed);
    minusIcon=MonoIcon::icon(FontAwesome::minus, MonoIcon::constRed, MonoIcon::constRed);
    addIcon=MonoIcon::icon(FontAwesome::plus, stdColor);
    addBookmarkIcon=MonoIcon::icon(FontAwesome::bookmark, stdColor);
    audioListIcon=MonoIcon::icon(FontAwesome::music, stdColor);
    playlistListIcon=MonoIcon::icon(FontAwesome::list, stdColor);
    smartPlaylistIcon=MonoIcon::icon(FontAwesome::graduationcap, stdColor);
    rssListIcon=MonoIcon::icon(FontAwesome::rss, stdColor);
    savedRssListIcon=MonoIcon::icon(FontAwesome::rsssquare, stdColor);
    clockIcon=MonoIcon::icon(FontAwesome::clocko, stdColor);
    folderListIcon=MonoIcon::icon(FontAwesome::foldero, stdColor);
    refreshIcon=MonoIcon::icon(FontAwesome::refresh, stdColor);
    streamListIcon=audioListIcon;
    streamCategoryIcon=folderListIcon;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    httpStreamIcon=MonoIcon::icon(FontAwesome::headphones, stdColor);
    #endif
    leftIcon=MonoIcon::icon(FontAwesome::chevronleft, stdColor);
    rightIcon=MonoIcon::icon(FontAwesome::chevronright, stdColor);
    upIcon=MonoIcon::icon(FontAwesome::chevronup, stdColor);
    downIcon=MonoIcon::icon(FontAwesome::chevrondown, stdColor);
    cancelIcon=MonoIcon::icon(FontAwesome::close, MonoIcon::constRed, MonoIcon::constRed);
}

void Icons::initSidebarIcons()
{
    #ifdef Q_OS_MAC
    QColor iconCol=OSXStyle::self()->monoIconColor();
    #else
    QColor iconCol=Utils::monoIconColor();
    #endif
    playqueueIcon=MonoIcon::icon(QLatin1String(":sidebar-playqueue"), iconCol);
    libraryIcon=MonoIcon::icon(QLatin1String(":sidebar-library"), iconCol);
    foldersIcon=MonoIcon::icon(QLatin1String(":sidebar-folders"), iconCol);
    playlistsIcon=MonoIcon::icon(QLatin1String(":sidebar-playlists"), iconCol);
    onlineIcon=MonoIcon::icon(QLatin1String(":sidebar-online"), iconCol);
    infoSidebarIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), iconCol);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesIcon=MonoIcon::icon(QLatin1String(":sidebar-devices"), iconCol);
    #endif
    searchTabIcon=MonoIcon::icon(QLatin1String(":sidebar-search"), iconCol);
}

void Icons::initToolbarIcons(const QColor &toolbarText)
{
    bool rtl=QApplication::isRightToLeft();

    toolbarPrevIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-next" : ":media-prev"), toolbarText);
    toolbarPlayIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-play-rtl" : ":media-play"), toolbarText);
    toolbarPauseIcon=MonoIcon::icon(QLatin1String(":media-pause"), toolbarText);
    toolbarStopIcon=MonoIcon::icon(QLatin1String(":media-stop"), toolbarText);
    toolbarNextIcon=MonoIcon::icon(QLatin1String(rtl ? ":media-prev" : ":media-next"), toolbarText);
    infoIcon=MonoIcon::icon(QLatin1String(":sidebar-info"), toolbarText);
    toolbarMenuIcon=MonoIcon::icon(FontAwesome::bars, toolbarText);
}

const QIcon & Icons::albumIcon(int size, bool mono) const
{
    return !mono || albumMonoIcon.isNull()
                ? size<48 ? albumIconSmall : albumIconLarge
                : albumMonoIcon;
}
