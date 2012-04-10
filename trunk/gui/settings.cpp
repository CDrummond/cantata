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

#include "settings.h"
#include "config.h"
#include "musiclibraryitemalbum.h"
#include "fancytabwidget.h"
#include "albumsmodel.h"
#include "itemview.h"
#include "mpdparseutils.h"
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
    , ver(-1)
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

bool Settings::splitterAutoHide()
{
    return GET_BOOL("splitterAutoHide", false);
}

QSize Settings::mainWindowSize()
{
    return GET_SIZE("mainWindowSize");
}

QSize Settings::mainWindowCollapsedSize()
{
    return GET_SIZE("mainWindowCollapsedSize");
}

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

bool Settings::stopDynamizerOnExit()
{
    return GET_BOOL("stopDynamizerOnExit", false);
}

bool Settings::smallPlaybackButtons()
{
    return GET_BOOL("smallPlaybackButtons", false);
}

bool Settings::smallControlButtons()
{
    return GET_BOOL("smallControlButtons", false);
}

const QString & Settings::mpdDir()
{
    if (mpdDirSetting.isEmpty()) {
        mpdDirSetting=MPDParseUtils::fixPath(GET_STRING("mpdDir", "/var/lib/mpd/music/"));
    }
    return mpdDirSetting;
}

bool Settings::storeCoversInMpdDir()
{
    return GET_BOOL("storeCoversInMpdDir", true);
}

bool Settings::storeLyricsInMpdDir()
{
    return GET_BOOL("storeLyricsInMpdDir", true);
}

int Settings::libraryView()
{
    return GET_INT("libraryView", (int)(version()>=CANTATA_MAKE_VERSION(0, 5, 0) ? ItemView::Mode_Tree : ItemView::Mode_List));
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
    return GET_INT("playlistsView", (int)(version()>=CANTATA_MAKE_VERSION(0, 5, 0) ? ItemView::Mode_Tree : ItemView::Mode_List));
}

int Settings::streamsView()
{
    return GET_INT("streamsView", (int)(version()>=CANTATA_MAKE_VERSION(0, 5, 0) ? ItemView::Mode_Tree : ItemView::Mode_List));
}

int Settings::libraryCoverSize()
{
    return GET_INT("libraryCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
}

int Settings::albumsCoverSize()
{
    return GET_INT("albumsCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
}

int Settings::albumSort()
{
    if (version()<CANTATA_MAKE_VERSION(0, 6, 0)) {
        return GET_BOOL("albumFirst", true) ? 0 : 1;
    }
    return GET_INT("albumSort", 0);
}

int Settings::sidebar()
{
    return GET_INT("sidebar", (int)(FancyTabWidget::Mode_LargeSidebar));
}

bool Settings::libraryYear()
{
    return GET_BOOL("libraryYear", true);
}

bool Settings::groupSingle()
{
    return GET_BOOL("groupSingle", MPDParseUtils::groupSingle());
}

bool Settings::groupMultiple()
{
    return GET_BOOL("groupMultiple", MPDParseUtils::groupMultiple());
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

int Settings::lyricsZoom()
{
    return GET_INT("lyricsZoom", 0);
}

int Settings::infoZoom()
{
    return GET_INT("infoZoom", 0);
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

bool Settings::mpris()
{
    return GET_BOOL("mpris", true);
}

bool Settings::dockManager()
{
    return GET_BOOL("dockManager", false);
}

#ifdef ENABLE_DEVICES_SUPPORT
bool Settings::overwriteSongs()
{
    return GET_BOOL("overwriteSongs", false);
}

bool Settings::showDeleteAction()
{
    return GET_BOOL("showDeleteAction", false);
}

int Settings::devicesView()
{
    return GET_INT("devicesView", (int)ItemView::Mode_Tree);

}
#endif

int Settings::version()
{
    if (-1==ver) {
        QStringList parts=GET_STRING("version", QLatin1String(PACKAGE_VERSION)).split('.');
        if (3==parts.size()) {
            ver=CANTATA_MAKE_VERSION(parts.at(0).toInt(), parts.at(1).toInt(), parts.at(2).toInt());
        } else {
            ver=CANTATA_MAKE_VERSION(0, 5, 0);
            SET_VALUE("version", PACKAGE_VERSION);
        }
    }
    return ver;
}

int Settings::stopFadeDuration()
{
    int def=version()<CANTATA_MAKE_VERSION(0, 4, 0) ? MinFade : DefaultFade;
    int v=GET_INT("stopFadeDuration", def);

    if (0!=v && (v<MinFade || v>MaxFade)) {
        v=DefaultFade;
    }
    return v;
}

int Settings::httpPort()
{
    return GET_INT("httpPort", 9001);
}

bool Settings::enableHttp()
{
    return GET_BOOL("enableHttp", false);
}

bool Settings::alwaysUseHttp()
{
    return GET_BOOL("alwaysUseHttp", false);
}

bool Settings::playQueueGrouped()
{
    return GET_BOOL("playQueueGrouped", true);
}

bool Settings::playQueueAutoExpand()
{
    return GET_BOOL("playQueueAutoExpand", true);
}

bool Settings::playQueueStartClosed()
{
    return GET_BOOL("playQueueStartClosed", version()<CANTATA_MAKE_VERSION(0, 6, 1));
}

bool Settings::playQueueScroll()
{
    return GET_BOOL("playQueueScroll", true);
}

bool Settings::playListsStartClosed()
{
    return GET_BOOL("playListsStartClosed", true);
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

void Settings::saveSplitterAutoHide(bool v)
{
    SET_VALUE("splitterAutoHide", v);
}

void Settings::saveMainWindowSize(const QSize &v)
{
    SET_VALUE("mainWindowSize", v);
}

void Settings::saveMainWindowCollapsedSize(const QSize &v)
{
    SET_VALUE("mainWindowCollapsedSize", v);
}

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

void Settings::saveStopDynamizerOnExit(bool v)
{
    SET_VALUE("stopDynamizerOnExit", v);
}

void Settings::saveSmallPlaybackButtons(bool v)
{
    SET_VALUE("smallPlaybackButtons", v);
}

void Settings::saveSmallControlButtons(bool v)
{
    SET_VALUE("smallControlButtons", v);
}

void Settings::saveMpdDir(const QString &v)
{
    mpdDirSetting=MPDParseUtils::fixPath(v);
    SET_VALUE("mpdDir", mpdDirSetting);
}

void Settings::saveStoreCoversInMpdDir(bool v)
{
    SET_VALUE("storeCoversInMpdDir", v);
}

void Settings::saveStoreLyricsInMpdDir(bool v)
{
    SET_VALUE("storeLyricsInMpdDir", v);
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

void Settings::saveStreamsView(int v)
{
    SET_VALUE("streamsView", v);
}

void Settings::saveLibraryCoverSize(int v)
{
    SET_VALUE("libraryCoverSize", v);
}

void Settings::saveAlbumsCoverSize(int v)
{
    SET_VALUE("albumsCoverSize", v);
}

void Settings::saveAlbumSort(int v)
{
    SET_VALUE("albumSort", v);
}

void Settings::saveSidebar(int v)
{
    SET_VALUE("sidebar", v);
}

void Settings::saveLibraryYear(bool v)
{
    SET_VALUE("libraryYear", v);
}

void Settings::saveGroupSingle(bool v)
{
    SET_VALUE("groupSingle", v);
}

void Settings::saveGroupMultiple(bool v)
{
    SET_VALUE("groupMultiple", v);
}

void Settings::saveLyricProviders(const QStringList &p)
{
    SET_VALUE("lyricProviders", p);
}

void Settings::saveLyricsZoom(int v)
{
    SET_VALUE("lyricsZoom", v);
}

void Settings::saveInfoZoom(int v)
{
    SET_VALUE("infoZoom", v);
}

void Settings::savePage(const QString &v)
{
    SET_VALUE("page", v);
}

void Settings::saveHiddenPages(const QStringList &p)
{
    SET_VALUE("hiddenPages", p);
}

void Settings::saveMpris(bool v)
{
    SET_VALUE("mpris", v);
}

void Settings::saveDockManager(bool v)
{
    SET_VALUE("dockManager", v);
}

#ifdef ENABLE_DEVICES_SUPPORT
void Settings::saveOverwriteSongs(bool v)
{
    SET_VALUE("overwriteSongs", v);
}

void Settings::saveShowDeleteAction(bool v)
{
    SET_VALUE("showDeleteAction", v);
}

void Settings::saveDevicesView(int v)
{
    SET_VALUE("devicesView", v);
}
#endif

void Settings::saveStopFadeDuration(int v)
{
    if (v<=MinFade) {
        v=0;
    } else if(v>MaxFade) {
        v=MaxFade;
    }
    SET_VALUE("stopFadeDuration", v);
}

void Settings::saveHttpPort(int v)
{
    SET_VALUE("httpPort", v);
}

void Settings::saveEnableHttp(bool v)
{
    SET_VALUE("enableHttp", v);
}

void Settings::saveAlwaysUseHttp(bool v)
{
    SET_VALUE("alwaysUseHttp", v);
}

void Settings::savePlayQueueGrouped(bool v)
{
    SET_VALUE("playQueueGrouped", v);
}

void Settings::savePlayQueueAutoExpand(bool v)
{
    SET_VALUE("playQueueAutoExpand", v);
}

void Settings::savePlayQueueStartClosed(bool v)
{
    SET_VALUE("playQueueStartClosed", v);
}

void Settings::savePlayQueueScroll(bool v)
{
    SET_VALUE("playQueueScroll", v);
}

void Settings::savePlayListsStartClosed(bool v)
{
    SET_VALUE("playListsStartClosed", v);
}

void Settings::save(bool force)
{
    if (force) {
        SET_VALUE("version", PACKAGE_VERSION);
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
