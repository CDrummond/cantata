/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "settings.h"
#include "config.h"
#include "musiclibraryitemalbum.h"
#include "fancytabwidget.h"
#include "albumsmodel.h"
#include "itemview.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KConfig>
#include "kwallet.h"
#include <QtGui/QApplication>
#include <QtGui/QWidget>
#include <QtCore/QTimer>

K_GLOBAL_STATIC(Settings, instance)
#endif

Settings * Settings::self()
{
#ifdef ENABLE_KDE_SUPPORT
    return instance;
#else
    static Settings *instance=0;;
    if(!instance) {
        instance=new Settings;
    }
    return instance;
#endif
}

Settings::Settings()
    : timer(0)
#ifdef ENABLE_KDE_SUPPORT
    , cfg(KGlobal::config(), "General")
    , wallet(0)
#endif
{
}

Settings::~Settings()
{
#ifdef ENABLE_KDE_SUPPORT
    delete wallet;
#endif
}

#ifdef ENABLE_KDE_SUPPORT
#define GET_STRING(KEY, DEF)     (cfg.readEntry(KEY, QString(DEF)))
#define GET_STRINGLIST(KEY, DEF) (cfg.readEntry(KEY, DEF))
#define GET_BOOL(KEY, DEF)       (cfg.readEntry(KEY, DEF))
#define GET_INT(KEY, DEF)        (cfg.readEntry(KEY, DEF))
#define GET_BYTE_ARRAY(KEY)      (cfg.readEntry(KEY, QByteArray()))
#define GET_SIZE(KEY)            (cfg.readEntry(KEY, QSize()))
#define SET_VALUE(KEY, V)        (cfg.writeEntry(KEY, V))
#else
#define GET_STRING(KEY, DEF)     (cfg.contains(KEY) ? cfg.value(KEY).toString() : QString(DEF))
#define GET_STRINGLIST(KEY, DEF) (cfg.contains(KEY) ? cfg.value(KEY).toStringList() : DEF)
#define GET_BOOL(KEY, DEF)       (cfg.contains(KEY) ? cfg.value(KEY).toBool() : DEF)
#define GET_INT(KEY, DEF)        (cfg.contains(KEY) ? cfg.value(KEY).toInt() : DEF)
#define GET_BYTE_ARRAY(KEY)      (cfg.value(KEY).toByteArray())
#define GET_SIZE(KEY)            (cfg.contains(KEY) ? cfg.value(KEY).toSize() : QSize())
#define SET_VALUE(KEY, V)        (cfg.setValue(KEY, V))
#endif

QString Settings::connectionHost()
{
    return GET_STRING("connectionHost", "localhost");
}

#ifdef ENABLE_KDE_SUPPORT
bool Settings::openWallet()
{
    if(wallet) {
        return true;
    }

    wallet=KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), QApplication::activeWindow() ? QApplication::activeWindow()->winId() : 0);
    if(wallet) {
        if(!wallet->hasFolder(PACKAGE_NAME)) {
            wallet->createFolder(PACKAGE_NAME);
        }
        wallet->setFolder(PACKAGE_NAME);
        return true;
    }

    return false;
}
#endif

QString Settings::connectionPasswd()
{
#ifdef ENABLE_KDE_SUPPORT
    if(passwd.isEmpty() && GET_BOOL("connectionPasswd", false) && openWallet()) {
        wallet->readPassword("mpd", passwd);
    }
    return passwd;
#else
    return GET_STRING("connectionPasswd", "");
#endif
}

int Settings::connectionPort()
{
    return GET_INT("connectionPort", 6600);
}

bool Settings::showPlaylist()
{
    return GET_BOOL("showPlaylist", true);
}

QByteArray Settings::playQueueHeaderState()
{
    return GET_BYTE_ARRAY("playQueueHeaderState");
}

QByteArray Settings::splitterState()
{
    return GET_BYTE_ARRAY("splitterState");
}

#ifndef ENABLE_KDE_SUPPORT
QSize Settings::mainWindowSize()
{
    return GET_SIZE("mainWindowSize");
}
#endif

bool Settings::useSystemTray()
{
    return GET_BOOL("useSystemTray", false);
}

bool Settings::showPopups()
{
    return GET_BOOL("showPopups", false);
}

bool Settings::stopOnExit()
{
    return GET_BOOL("stopOnExit", false);
}

QString Settings::mpdDir()
{
    QString d=GET_STRING("mpdDir", "/var/lib/mpd/music");
    if (!d.endsWith("/")) {
        d=d+"/";
    }
    return d;
}

int Settings::libraryView()
{
    return GET_INT("libraryView", (int)ItemView::Mode_List);
}

int Settings::albumsView()
{
    return GET_INT("albumsView", (int)ItemView::Mode_IconTop);
}

int Settings::folderView()
{
    return GET_INT("folderView", (int)ItemView::Mode_Tree);
}

int Settings::playlistsView()
{
    return GET_INT("playlistsView", (int)ItemView::Mode_List);
}

int Settings::libraryCoverSize()
{
    return GET_INT("libraryCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
}

int Settings::albumsCoverSize()
{
    return GET_INT("albumsCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
}

int Settings::sidebar()
{
    return GET_INT("sidebar", (int)(FancyTabWidget::Mode_LargeSidebar));
}

QStringList Settings::lyricProviders()
{
    QStringList def;
    def << "lyrics.wikia.com"
        << "lyricstime.com"
        << "lyricsreg.com"
        << "lyricsmania.com"
        << "metrolyrics.com"
        << "azlyrics.com"
        << "songlyrics.com"
        << "elyrics.net"
        << "lyricsdownload.com"
        << "lyrics.com"
        << "lyricsbay.com"
        << "directlyrics.com"
        << "loudson.gs"
        << "teksty.org"
        << "tekstowo.pl (POLISH)"
        << "vagalume.uol.com.br"
        << "vagalume.uol.com.br (PORTUGUESE)";
    return GET_STRINGLIST("lyricProviders", def);
}

QString Settings::page()
{
    return GET_STRING("page", QString());
}

QStringList Settings::hiddenPages()
{
    QStringList def;
    def << "FolderPage"
        << "ServerInfoPage";
    return GET_STRINGLIST("hiddenPages", def);
}

void Settings::saveConnectionHost(const QString &v)
{
    SET_VALUE("connectionHost", v);
}

void Settings::saveConnectionPasswd(const QString &v)
{
#ifdef ENABLE_KDE_SUPPORT
    if(v!=passwd) {
        passwd=v;
        SET_VALUE("connectionPasswd", !passwd.isEmpty());

        if(passwd.isEmpty()) {
            if(wallet) {
                wallet->removeEntry("mpd");
            }
        }
        else if(openWallet()) {
            wallet->writePassword("mpd", passwd);
        }
    }
#else
    SET_VALUE("connectionPasswd", v);
#endif
}

void Settings::saveConnectionPort(int v)
{
    SET_VALUE("connectionPort", v);
}

void Settings::saveShowPlaylist(bool v)
{
    SET_VALUE("showPlaylist", v);
}

void Settings::savePlayQueueHeaderState(const QByteArray &v)
{
    SET_VALUE("playQueueHeaderState", v);
}

void Settings::saveSplitterState(const QByteArray &v)
{
    SET_VALUE("splitterState", v);
}

#ifndef ENABLE_KDE_SUPPORT
void Settings::saveMainWindowSize(const QSize &v)
{
    SET_VALUE("mainWindowSize", v);
}
#endif

void Settings::saveUseSystemTray(bool v)
{
    SET_VALUE("useSystemTray", v);
}

void Settings::saveShowPopups(bool v)
{
    SET_VALUE("showPopups", v);
}

void Settings::saveStopOnExit(bool v)
{
    SET_VALUE("stopOnExit", v);
}

void Settings::saveMpdDir(const QString &v)
{
    QString d(v);
    if (!d.isEmpty() && !d.endsWith("/")) {
        d=d+"/";
    }
    SET_VALUE("mpdDir", d);
}

void Settings::saveLibraryView(int v)
{
    SET_VALUE("libraryView", v);
}

void Settings::saveAlbumsView(int v)
{
    SET_VALUE("albumsView", v);
}

void Settings::saveFolderView(int v)
{
    SET_VALUE("folderView", v);
}

void Settings::savePlaylistsView(int v)
{
    SET_VALUE("playlistsView", v);
}

void Settings::saveLibraryCoverSize(int v)
{
    SET_VALUE("libraryCoverSize", v);
}

void Settings::saveAlbumsCoverSize(int v)
{
    SET_VALUE("albumsCoverSize", v);
}

void Settings::saveSidebar(int v)
{
    SET_VALUE("sidebar", v);
}

void Settings::saveLyricProviders(const QStringList &p)
{
    SET_VALUE("lyricProviders", p);
}

void Settings::savePage(const QString &v)
{
    SET_VALUE("page", v);
}

void Settings::saveHiddenPages(const QStringList &p)
{
    SET_VALUE("hiddenPages", p);
}

void Settings::save(bool force)
{
    if (force) {
#ifdef ENABLE_KDE_SUPPORT
        KGlobal::config()->sync();
#else
        cfg.sync();
#endif
        if (timer) {
            timer->stop();
        }
    } else {
        if (!timer) {
            timer=new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(actualSave()));
        }
        timer->start(30*1000);
    }
}

void Settings::actualSave()
{
    save(true);
}
