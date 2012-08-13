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
#include <QtCore/QFile>
#include <QtCore/QDir>

Settings * Settings::self()
{
    #ifdef ENABLE_KDE_SUPPORT
    return instance;
    #else
    static Settings *instance=0;
    if (!instance) {
        instance=new Settings;
    }
    return instance;
    #endif
}

struct MpdDefaults
{
    MpdDefaults()
        : host("localhost")
        , dir("/var/lib/mpd/music/")
        , port(6600) {
    }

    static QString getVal(const QString &line) {
        QStringList parts=line.split('\"');
        return parts.count()>1 ? parts[1] : QString();
    }

    void read() {
        QFile f("/etc/mpd.conf");

        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            while (!f.atEnd()) {
                QString line = f.readLine().trimmed();
                if (line.startsWith('#')) {
                    continue;
                } else if (line.startsWith(QLatin1String("music_directory"))) {
                    QString val=getVal(line);
                    if (!val.isEmpty() && QDir(val).exists()) {
                        dir=val;
                    }
                } else if (line.startsWith(QLatin1String("bind_to_address"))) {
                    QString val=getVal(line);
                    if (!val.isEmpty() && val!=QLatin1String("any")) {
                        host=val;
                    }
                } else if (line.startsWith(QLatin1String("port"))) {
                    int val=getVal(line).toInt();
                    if (val>0) {
                        port=val;
                    }
                } else if (line.startsWith(QLatin1String("password"))) {
                    QString val=getVal(line);
                    if (!val.isEmpty()) {
                        QStringList parts=val.split('@');
                        if (parts.count()) {
                            passwd=parts[0];
                        }
                    }
                }
            }
        }
    }

    QString host;
    QString dir;
    QString passwd;
    int port;
};

static MpdDefaults mpdDefaults;

#ifdef ENABLE_KDE_SUPPORT
#define CFG_GET_STRING(CFG, KEY, DEF)     (CFG.readEntry(KEY, QString(DEF)))
#define CFG_GET_STRINGLIST(CFG, KEY, DEF) (CFG.readEntry(KEY, DEF))
#define CFG_GET_BOOL(CFG, KEY, DEF)       (CFG.readEntry(KEY, DEF))
#define CFG_GET_INT(CFG, KEY, DEF)        (CFG.readEntry(KEY, DEF))
#define CFG_GET_BYTE_ARRAY(CFG, KEY)      (CFG.readEntry(KEY, QByteArray()))
#define CFG_GET_SIZE(CFG, KEY)            (CFG.readEntry(KEY, QSize()))
#define CFG_SET_VALUE(CFG, KEY, V)        (CFG.writeEntry(KEY, V))
#define HAS_GROUP(GRP)                    (KGlobal::config()->hasGroup(GRP))
#define REMOVE_GROUP(GRP)                 (KGlobal::config()->deleteGroup(GRP))
#define REMOVE_ENTRY(KEY)                 (cfg.deleteEntry(KEY))
#define GET_STRING(KEY, DEF)              CFG_GET_STRING(cfg, KEY, DEF)
#define GET_STRINGLIST(KEY, DEF)          CFG_GET_STRINGLIST(cfg, KEY, DEF)
#define GET_BOOL(KEY, DEF)                CFG_GET_BOOL(cfg, KEY, DEF)
#define GET_INT(KEY, DEF)                 CFG_GET_INT(cfg, KEY, DEF)
#define GET_BYTE_ARRAY(KEY)               CFG_GET_BYTE_ARRAY(cfg, KEY)
#define GET_SIZE(KEY)                     CFG_GET_SIZE(cfg, KEY)
#define SET_VALUE(KEY, V)                 CFG_SET_VALUE(cfg, KEY, V)
#define HAS_ENTRY(KEY)                    (cfg.hasKey(KEY))
#else
#define GET_STRING(KEY, DEF)     (cfg.contains(KEY) ? cfg.value(KEY).toString() : QString(DEF))
#define GET_STRINGLIST(KEY, DEF) (cfg.contains(KEY) ? cfg.value(KEY).toStringList() : DEF)
#define GET_BOOL(KEY, DEF)       (cfg.contains(KEY) ? cfg.value(KEY).toBool() : DEF)
#define GET_INT(KEY, DEF)        (cfg.contains(KEY) ? cfg.value(KEY).toInt() : DEF)
#define GET_BYTE_ARRAY(KEY)      (cfg.value(KEY).toByteArray())
#define GET_SIZE(KEY)            (cfg.contains(KEY) ? cfg.value(KEY).toSize() : QSize())
#define SET_VALUE(KEY, V)        (cfg.setValue(KEY, V))
#define HAS_GROUP(GRP)           (-1!=cfg.childGroups().indexOf(GRP))
#define REMOVE_GROUP(GRP)        (cfg.remove(GRP))
#define REMOVE_ENTRY(KEY)        (cfg.remove(KEY))
#define HAS_ENTRY(KEY)           (cfg.contains(KEY))
#endif

Settings::Settings()
    : isFirstRun(false)
    , timer(0)
    , ver(-1)
    #ifdef ENABLE_KDE_SUPPORT
    , cfg(KGlobal::config(), "General")
    , wallet(0)
    #endif
{
    #ifdef CANTATA_ANDROID
    cfg.setPath(QSettings::NativeFormat, QSettings::UserScope, getConfigDir());
    #endif
    // Only need to read system defaults if we have not previously been configured...
    if (version()<CANTATA_MAKE_VERSION(0, 8, 0)
            ? GET_STRING("connectionHost", QString()).isEmpty()
            : !HAS_GROUP(MPDConnectionDetails::configGroupName())) {
        mpdDefaults.read();
    }
}

Settings::~Settings()
{
    #ifdef ENABLE_KDE_SUPPORT
    delete wallet;
    #endif
}

QString Settings::currentConnection()
{
    return GET_STRING("currentConnection", QString());
}

MPDConnectionDetails Settings::connectionDetails(const QString &name)
{
    MPDConnectionDetails details;
    if (version()<CANTATA_MAKE_VERSION(0, 8, 0) || (name.isEmpty() && !HAS_GROUP(MPDConnectionDetails::configGroupName(name)))) {
        details.hostname=GET_STRING("connectionHost", name.isEmpty() ? mpdDefaults.host : QString());
        #ifdef ENABLE_KDE_SUPPORT
        if (GET_BOOL("connectionPasswd", false)) {
            if (openWallet()) {
                wallet->readPassword("mpd", details.password);
            }
        } else if (name.isEmpty()) {
           details.password=mpdDefaults.passwd;
        }
        #else
        details.password=GET_STRING("connectionPasswd", name.isEmpty() ? mpdDefaults.passwd : QString());
        #endif
        details.port=GET_INT("connectionPort", name.isEmpty() ? mpdDefaults.port : 6600);
        details.dir=MPDParseUtils::fixPath(GET_STRING("mpdDir", mpdDefaults.dir));
    } else {
        QString n=MPDConnectionDetails::configGroupName(name);
        details.name=name;
        if (HAS_GROUP(n)) {
            #ifdef ENABLE_KDE_SUPPORT
            KConfigGroup grp(KGlobal::config(), n);
            details.hostname=CFG_GET_STRING(grp, "host", name.isEmpty() ? mpdDefaults.host : QString());
            details.port=CFG_GET_INT(grp, "port", name.isEmpty() ? mpdDefaults.port : 6600);
            details.dir=MPDParseUtils::fixPath(CFG_GET_STRING(grp, "dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music"));
            if (KWallet::Wallet::isEnabled()) {
                if (CFG_GET_BOOL(grp, "passwd", false)) {
                    if (openWallet()) {
                        wallet->readPassword(name.isEmpty() ? "mpd" : name, details.password);
                    }
                } else if (name.isEmpty()) {
                    details.password=mpdDefaults.passwd;
                }
            } else {
                details.password=CFG_GET_STRING(grp, "pass", name.isEmpty() ? mpdDefaults.passwd : QString());
            }
            #else
            cfg.beginGroup(n);
            details.hostname=GET_STRING("host", name.isEmpty() ? mpdDefaults.host : QString());
            details.port=GET_INT("port", name.isEmpty() ? mpdDefaults.port : 6600);
            details.dir=MPDParseUtils::fixPath(GET_STRING("dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music"));
            details.password=GET_STRING("passwd", name.isEmpty() ? mpdDefaults.passwd : QString());
            cfg.endGroup();
            #endif
        }
    }
    details.dirReadable=details.dir.isEmpty() ? false : QDir(details.dir).isReadable();
    return details;
}

QList<MPDConnectionDetails> Settings::allConnections()
{
    #ifdef ENABLE_KDE_SUPPORT
    QStringList groups=KGlobal::config()->groupList();
    #else
    QStringList groups=cfg.childGroups();
    #endif
    QList<MPDConnectionDetails> connections;
    foreach (const QString &grp, groups) {
        if (HAS_GROUP(grp) && grp.startsWith("Connection")) {
            connections.append(connectionDetails(grp=="Connection" ? QString() : grp.mid(11)));
        }
    }

    if (connections.isEmpty()) {
        // If we are empty, add at lease the default connection...
        connections.append(connectionDetails());
    }
    return connections;
}

#ifdef ENABLE_KDE_SUPPORT
bool Settings::openWallet()
{
    if (wallet) {
        return true;
    }

    wallet=KWallet::Wallet::openWallet(KWallet::Wallet::LocalWallet(), QApplication::activeWindow() ? QApplication::activeWindow()->winId() : 0);
    if (wallet) {
        if (!wallet->hasFolder(PACKAGE_NAME)) {
            wallet->createFolder(PACKAGE_NAME);
        }
        wallet->setFolder(PACKAGE_NAME);
        return true;
    }

    return false;
}
#endif

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

#ifndef Q_OS_WIN
bool Settings::stopDynamizerOnExit()
{
    return GET_BOOL("stopDynamizerOnExit", false);
}
#endif

bool Settings::smallPlaybackButtons()
{
    return GET_BOOL("smallPlaybackButtons", false);
}

bool Settings::smallControlButtons()
{
    return GET_BOOL("smallControlButtons", false);
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
    #ifdef CANTATA_ANDROID
    return ItemView::Mode_List;
    #else
    return GET_INT("libraryView", (int)(version()>=CANTATA_MAKE_VERSION(0, 5, 0) ? ItemView::Mode_Tree : ItemView::Mode_List));
    #endif
}

int Settings::albumsView()
{
    #ifdef CANTATA_ANDROID
    return ItemView::Mode_IconTop;
    #else
    return GET_INT("albumsView", (int)ItemView::Mode_IconTop);
    #endif
}

int Settings::folderView()
{
    #ifdef CANTATA_ANDROID
    return ItemView::Mode_List;
    #else
    return GET_INT("folderView", (int)ItemView::Mode_Tree);
    #endif
}

int Settings::playlistsView()
{
    #ifdef CANTATA_ANDROID
    return ItemView::Mode_List;
    #else
    return GET_INT("playlistsView", (int)(version()>=CANTATA_MAKE_VERSION(0, 5, 0) ? ItemView::Mode_Tree : ItemView::Mode_List));
    #endif
}

int Settings::streamsView()
{
    #ifdef CANTATA_ANDROID
    return ItemView::Mode_List;
    #else
    return GET_INT("streamsView", (int)(version()>=CANTATA_MAKE_VERSION(0, 5, 0) ? ItemView::Mode_Tree : ItemView::Mode_List));
    #endif
}

bool Settings::libraryArtistImage()
{
    return GET_BOOL("libraryArtistImage", false);
}

int Settings::libraryCoverSize()
{
    #ifdef CANTATA_ANDROID
    return MusicLibraryItemAlbum::CoverAuto;
    #else
    return GET_INT("libraryCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
    #endif
}

int Settings::albumsCoverSize()
{
    #ifdef CANTATA_ANDROID
    return MusicLibraryItemAlbum::CoverAuto;
    #else
    return GET_INT("albumsCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
    #endif
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

bool Settings::lyricsBgnd()
{
    return GET_BOOL("lyricsBgnd", true);
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
        isFirstRun=!HAS_ENTRY("version");
        QStringList parts=GET_STRING("version", QLatin1String(PACKAGE_VERSION)).split('.');
        if (3==parts.size()) {
            ver=CANTATA_MAKE_VERSION(parts.at(0).toInt(), parts.at(1).toInt(), parts.at(2).toInt());
        } else {
            ver=CANTATA_MAKE_VERSION(0, 8, 0);
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

#ifdef TAGLIB_FOUND
int Settings::httpPort()
{
    return GET_INT("httpPort", 9001);
}

QString Settings::httpAddress()
{
    return GET_STRING("httpAddress", QString());
}

bool Settings::enableHttp()
{
    return GET_BOOL("enableHttp", false);
}

bool Settings::alwaysUseHttp()
{
    return GET_BOOL("alwaysUseHttp", false);
}
#endif

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

#ifdef PHONON_FOUND
bool Settings::playStream()
{
    return GET_BOOL("playStream", false);
}

QString Settings::streamUrl()
{
    return GET_STRING("streamUrl", QString());
}
#endif

void Settings::removeConnectionDetails(const QString &v)
{
    REMOVE_GROUP(MPDConnectionDetails::configGroupName(v));
    #ifdef ENABLE_KDE_SUPPORT
    KGlobal::config()->sync();
    #endif
}

void Settings::saveConnectionDetails(const MPDConnectionDetails &v)
{
    if (v.name.isEmpty()) {
        REMOVE_ENTRY("connectionHost");
        REMOVE_ENTRY("connectionPasswd");
        REMOVE_ENTRY("connectionPort");
        REMOVE_ENTRY("mpdDir");
    }

    QString n=MPDConnectionDetails::configGroupName(v.name);
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup grp(KGlobal::config(), n);
    CFG_SET_VALUE(grp, "host", v.hostname);
    CFG_SET_VALUE(grp, "port", (int)v.port);
    CFG_SET_VALUE(grp, "dir", v.dir);
    if (KWallet::Wallet::isEnabled()) {
        CFG_SET_VALUE(grp, "passwd", !v.password.isEmpty());
        QString walletEntry=v.name.isEmpty() ? "mpd" : v.name;
        if (v.password.isEmpty()) {
            if (wallet) {
                wallet->removeEntry(walletEntry);
            }
        }
        else if (openWallet()) {
            wallet->writePassword(walletEntry, v.password);
        }
    } else {
        CFG_SET_VALUE(grp, "pass", v.password);
    }
    #else
    cfg.beginGroup(n);
    SET_VALUE("host", v.hostname);
    SET_VALUE("port", (int)v.port);
    SET_VALUE("dir", v.dir);
    SET_VALUE("passwd", v.password);
    cfg.endGroup();
    #endif
}

void Settings::saveCurrentConnection(const QString &v)
{
    SET_VALUE("currentConnection", v);
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

#ifndef Q_OS_WIN
void Settings::saveStopDynamizerOnExit(bool v)
{
    SET_VALUE("stopDynamizerOnExit", v);
}
#endif

void Settings::saveSmallPlaybackButtons(bool v)
{
    SET_VALUE("smallPlaybackButtons", v);
}

void Settings::saveSmallControlButtons(bool v)
{
    SET_VALUE("smallControlButtons", v);
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

void Settings::saveLibraryArtistImage(bool v)
{
    SET_VALUE("libraryArtistImage", v);
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

void Settings::saveLyricsBgnd(bool v)
{
    SET_VALUE("lyricsBgnd", v);
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
    } else if (v>MaxFade) {
        v=MaxFade;
    }
    SET_VALUE("stopFadeDuration", v);
}

#ifdef TAGLIB_FOUND
void Settings::saveHttpPort(int v)
{
    SET_VALUE("httpPort", v);
}

void Settings::saveHttpAddress(const QString &v)
{
    SET_VALUE("httpAddress", v);
}

void Settings::saveEnableHttp(bool v)
{
    SET_VALUE("enableHttp", v);
}

void Settings::saveAlwaysUseHttp(bool v)
{
    SET_VALUE("alwaysUseHttp", v);
}
#endif

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

#ifdef PHONON_FOUND
void Settings::savePlayStream(bool v)
{
    SET_VALUE("playStream", v);
}

void Settings::saveStreamUrl(const QString &v)
{
    SET_VALUE("streamUrl", v);
}
#endif

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

#ifdef CANTATA_ANDROID
QString Settings::getConfigDir()
{
    QString cfgDir;

    if (cfgDir.isEmpty()) {
        if (QDir("/mnt/sdcard/cantata").exists()) {
            cfgDir="/mnt/sdcard/cantata/";
        } else {
            if (QDir("/mnt/external1").exists()) { // Xoom's SDCard is here...
                cfgDir="/mnt/external1/cantata/";
            } else {
                cfgDir="/mnt/sdcard/cantata/";
            }
        }

        if (!QDir(cfgDir).exists()) {
            QDir(cfgDir).mkpath(cfgDir);
        }
    }

    return cfgDir;
}
#endif
