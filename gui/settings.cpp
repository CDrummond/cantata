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
#include "utils.h"
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

Settings::Settings()
    : isFirstRun(false)
    , modified(false)
    , timer(0)
    , ver(-1)
    #ifdef ENABLE_KDE_SUPPORT
    , cfg(KGlobal::config(), "General")
    , wallet(0)
    #endif
{
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
        details.dir=Utils::fixPath(GET_STRING("mpdDir", mpdDefaults.dir));
        details.dynamizerPort=0;
    } else {
        QString n=MPDConnectionDetails::configGroupName(name);
        details.name=name;
        if (!HAS_GROUP(n)) {
            details.name=QString();
            n=MPDConnectionDetails::configGroupName(details.name);
        }
        if (HAS_GROUP(n)) {
            #ifdef ENABLE_KDE_SUPPORT
            KConfigGroup grp(KGlobal::config(), n);
            details.hostname=CFG_GET_STRING(grp, "host", name.isEmpty() ? mpdDefaults.host : QString());
            details.port=CFG_GET_INT(grp, "port", name.isEmpty() ? mpdDefaults.port : 6600);
            details.dir=Utils::fixPath(CFG_GET_STRING(grp, "dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music"));
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
            details.dynamizerPort=CFG_GET_INT(grp, "dynamizerPort", 0);
            details.coverName=CFG_GET_STRING(grp, "coverName", QString());
            #else
            cfg.beginGroup(n);
            details.hostname=GET_STRING("host", name.isEmpty() ? mpdDefaults.host : QString());
            details.port=GET_INT("port", name.isEmpty() ? mpdDefaults.port : 6600);
            details.dir=Utils::fixPath(GET_STRING("dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music"));
            details.password=GET_STRING("passwd", name.isEmpty() ? mpdDefaults.passwd : QString());
            details.dynamizerPort=GET_INT("dynamizerPort", 0);
            details.coverName=GET_STRING("coverName", QString());
            cfg.endGroup();
            #endif
        } else {
            details.hostname=mpdDefaults.host;
            details.port=mpdDefaults.port;
            details.dir=mpdDefaults.dir;
            details.password=mpdDefaults.passwd;
            details.dynamizerPort=0;
            details.coverName=QString();
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
#else
QString Settings::iconTheme()
{
    return GET_STRING("iconTheme", QString());
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

bool Settings::minimiseOnClose()
{
    return GET_BOOL("minimiseOnClose", true);
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

bool Settings::libraryArtistImage()
{
    return GET_BOOL("libraryArtistImage", false);
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

bool Settings::gnomeMediaKeys()
{
    return GET_BOOL("gnomeMediaKeys", false);
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
        QStringList parts=GET_STRING("version", QLatin1String(PACKAGE_VERSION_STRING)).split('.');
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

bool Settings::forceSingleClick()
{
    return GET_BOOL("forceSingleClick", true);
}

bool Settings::startHidden()
{
    return GET_BOOL("startHidden", false);
}

void Settings::removeConnectionDetails(const QString &v)
{
    if (v==currentConnection()) {
        saveCurrentConnection(QString());
    }
    REMOVE_GROUP(MPDConnectionDetails::configGroupName(v));
    #ifdef ENABLE_KDE_SUPPORT
    KGlobal::config()->sync();
    #endif
    modified=true;
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
    CFG_SET_VALUE(grp, "dynamizerPort", (int)v.dynamizerPort);
    CFG_SET_VALUE(grp, "coverName", v.coverName);
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
    SET_VALUE("dynamizerPort", (int)v.dynamizerPort);
    SET_VALUE("coverName", v.coverName);
    cfg.endGroup();
    #endif
    modified=true;
}

#define SET_VALUE_MOD(KEY, VAL) modified=true; SET_VALUE(KEY, VAL)

void Settings::saveCurrentConnection(const QString &v)
{
    if (v!=currentConnection()) {
        SET_VALUE_MOD("currentConnection", v);
    }
}

void Settings::saveShowPlaylist(bool v)
{
    if (v!=showPlaylist()) {
        SET_VALUE_MOD("showPlaylist", v);
    }
}

void Settings::savePlayQueueHeaderState(const QByteArray &v)
{
    if (v!=playQueueHeaderState()) {
        SET_VALUE_MOD("playQueueHeaderState", v);
    }
}

void Settings::saveSplitterState(const QByteArray &v)
{
    if (v!=splitterState()) {
        SET_VALUE_MOD("splitterState", v);
    }
}

void Settings::saveSplitterAutoHide(bool v)
{
    if (v!=splitterAutoHide()) {
        SET_VALUE_MOD("splitterAutoHide", v);
    }
}

void Settings::saveMainWindowSize(const QSize &v)
{
    if (v!=mainWindowSize()) {
        SET_VALUE_MOD("mainWindowSize", v);
    }
}

void Settings::saveMainWindowCollapsedSize(const QSize &v)
{
    if (v!=mainWindowCollapsedSize()) {
        SET_VALUE_MOD("mainWindowCollapsedSize", v);
    }
}

void Settings::saveUseSystemTray(bool v)
{
    if (v!=useSystemTray()) {
        SET_VALUE_MOD("useSystemTray", v);
    }
}

void Settings::saveMinimiseOnClose(bool v)
{
    if (v!=minimiseOnClose()) {
        SET_VALUE_MOD("minimiseOnClose", v);
    }
}

void Settings::saveShowPopups(bool v)
{
    if (v!=showPopups()) {
        SET_VALUE_MOD("showPopups", v);
    }
}

void Settings::saveStopOnExit(bool v)
{
    if (v!=stopOnExit()) {
        SET_VALUE_MOD("stopOnExit", v);
    }
}

void Settings::saveStopDynamizerOnExit(bool v)
{
    if (v!=stopDynamizerOnExit()) {
        SET_VALUE_MOD("stopDynamizerOnExit", v);
    }
}

void Settings::saveSmallPlaybackButtons(bool v)
{
    if (v!=smallPlaybackButtons()) {
        SET_VALUE_MOD("smallPlaybackButtons", v);
    }
}

void Settings::saveSmallControlButtons(bool v)
{
    if (v!=smallControlButtons()) {
        SET_VALUE_MOD("smallControlButtons", v);
    }
}

void Settings::saveStoreCoversInMpdDir(bool v)
{
    if (v!=storeCoversInMpdDir()) {
        SET_VALUE_MOD("storeCoversInMpdDir", v);
    }
}

void Settings::saveStoreLyricsInMpdDir(bool v)
{
    if (v!=storeLyricsInMpdDir()) {
        SET_VALUE_MOD("storeLyricsInMpdDir", v);
    }
}

void Settings::saveLibraryView(int v)
{
    if (v!=libraryView()) {
        SET_VALUE_MOD("libraryView", v);
    }
}

void Settings::saveAlbumsView(int v)
{
    if (v!=albumsView()) {
        SET_VALUE_MOD("albumsView", v);
    }
}

void Settings::saveFolderView(int v)
{
    if (v!=folderView()) {
        SET_VALUE_MOD("folderView", v);
    }
}

void Settings::savePlaylistsView(int v)
{
    if (v!=playlistsView()) {
        SET_VALUE_MOD("playlistsView", v);
    }
}

void Settings::saveStreamsView(int v)
{
    if (v!=streamsView()) {
        SET_VALUE_MOD("streamsView", v);
    }
}

void Settings::saveLibraryArtistImage(bool v)
{
    if (v!=libraryArtistImage()) {
        SET_VALUE_MOD("libraryArtistImage", v);
    }
}

void Settings::saveLibraryCoverSize(int v)
{
    if (v!=libraryCoverSize()) {
        SET_VALUE_MOD("libraryCoverSize", v);
    }
}

void Settings::saveAlbumsCoverSize(int v)
{
    if (v!=albumsCoverSize()) {
        SET_VALUE_MOD("albumsCoverSize", v);
    }
}

void Settings::saveAlbumSort(int v)
{
    if (v!=albumSort()) {
        SET_VALUE_MOD("albumSort", v);
    }
}

void Settings::saveSidebar(int v)
{
    if (v!=sidebar()) {
        SET_VALUE_MOD("sidebar", v);
    }
}

void Settings::saveLibraryYear(bool v)
{
    if (v!=libraryYear()) {
        SET_VALUE_MOD("libraryYear", v);
    }
}

void Settings::saveGroupSingle(bool v)
{
    if (v!=groupSingle()) {
        SET_VALUE_MOD("groupSingle", v);
    }
}

void Settings::saveGroupMultiple(bool v)
{
    if (v!=groupMultiple()) {
        SET_VALUE_MOD("groupMultiple", v);
    }
}

void Settings::saveLyricProviders(const QStringList &p)
{
    if (p!=lyricProviders()) {
        SET_VALUE_MOD("lyricProviders", p);
    }
}

void Settings::saveLyricsZoom(int v)
{
    if (v!=lyricsZoom()) {
        SET_VALUE_MOD("lyricsZoom", v);
    }
}

void Settings::saveLyricsBgnd(bool v)
{
    if (v!=lyricsBgnd()) {
        SET_VALUE_MOD("lyricsBgnd", v);
    }
}

void Settings::saveInfoZoom(int v)
{
    if (v!=infoZoom()) {
        SET_VALUE_MOD("infoZoom", v);
    }
}

void Settings::savePage(const QString &v)
{
    if (v!=page()) {
        SET_VALUE_MOD("page", v);
    }
}

void Settings::saveHiddenPages(const QStringList &p)
{
    if (p!=hiddenPages()) {
        SET_VALUE_MOD("hiddenPages", p);
    }
}

void Settings::saveGnomeMediaKeys(bool v)
{
    if (v!=gnomeMediaKeys()) {
        SET_VALUE_MOD("gnomeMediaKeys", v);
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
void Settings::saveOverwriteSongs(bool v)
{
    if (v!=overwriteSongs()) {
        SET_VALUE_MOD("overwriteSongs", v);
    }
}

void Settings::saveShowDeleteAction(bool v)
{
    if (v!=showDeleteAction()) {
        SET_VALUE_MOD("showDeleteAction", v);
    }
}

void Settings::saveDevicesView(int v)
{
    if (v!=devicesView()) {
        SET_VALUE_MOD("devicesView", v);
    }
}
#endif

void Settings::saveStopFadeDuration(int v)
{
    if (v<=MinFade) {
        v=0;
    } else if (v>MaxFade) {
        v=MaxFade;
    }
    if (v!=stopFadeDuration()) {
        SET_VALUE_MOD("stopFadeDuration", v);
    }
}

#ifdef TAGLIB_FOUND
void Settings::saveHttpPort(int v)
{
    if (v!=httpPort()) {
        SET_VALUE_MOD("httpPort", v);
    }
}

void Settings::saveHttpAddress(const QString &v)
{
    if (v!=httpAddress()) {
        SET_VALUE_MOD("httpAddress", v);
    }
}

void Settings::saveEnableHttp(bool v)
{
    if (v!=enableHttp()) {
        SET_VALUE_MOD("enableHttp", v);
    }
}

void Settings::saveAlwaysUseHttp(bool v)
{
    if (v!=alwaysUseHttp()) {
        SET_VALUE_MOD("alwaysUseHttp", v);
    }
}
#endif

void Settings::savePlayQueueGrouped(bool v)
{
    if (v!=playQueueGrouped()) {
        SET_VALUE_MOD("playQueueGrouped", v);
    }
}

void Settings::savePlayQueueAutoExpand(bool v)
{
    if (v!=playQueueAutoExpand()) {
        SET_VALUE_MOD("playQueueAutoExpand", v);
    }
}

void Settings::savePlayQueueStartClosed(bool v)
{
    if (v!=playQueueStartClosed()) {
        SET_VALUE_MOD("playQueueStartClosed", v);
    }
}

void Settings::savePlayQueueScroll(bool v)
{
    if (v!=playQueueScroll()) {
        SET_VALUE_MOD("playQueueScroll", v);
    }
}

void Settings::savePlayListsStartClosed(bool v)
{
    if (v!=playListsStartClosed()) {
        SET_VALUE_MOD("playListsStartClosed", v);
    }
}

#ifdef PHONON_FOUND
void Settings::savePlayStream(bool v)
{
    if (v!=playStream()) {
        SET_VALUE_MOD("playStream", v);
    }
}

void Settings::saveStreamUrl(const QString &v)
{
    if (v!=streamUrl()) {
        SET_VALUE_MOD("streamUrl", v);
    }
}
#endif

void Settings::saveForceSingleClick(bool v)
{
    if (v!=forceSingleClick()) {
        SET_VALUE_MOD("forceSingleClick", v);
    }
}

void Settings::saveStartHidden(bool v)
{
    if (v!=startHidden()) {
        SET_VALUE_MOD("startHidden", v);
    }
}

void Settings::save(bool force)
{
    if (force) {
        if (version()!=PACKAGE_VERSION) {
            SET_VALUE_MOD("version", PACKAGE_VERSION_STRING);
            ver=PACKAGE_VERSION;
        }
        if (modified) {
            modified=false;
            CFG_SYNC;
        }
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
