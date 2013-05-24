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
#include <QApplication>
#include <QWidget>
#include <QTimer>

K_GLOBAL_STATIC(Settings, instance)
#endif
#include <QFile>
#include <QDir>
#include <qglobal.h>

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

bool Settings::showFullScreen()
{
    return GET_BOOL("showFullScreen", false);
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

bool Settings::stopAfterCurrent()
{
    return GET_BOOL("stopAfterCurrent", false);
}

bool Settings::storeCoversInMpdDir()
{
    return GET_BOOL("storeCoversInMpdDir", true);
}

bool Settings::storeLyricsInMpdDir()
{
    return GET_BOOL("storeLyricsInMpdDir", true);
}

bool Settings::storeStreamsInMpdDir()
{
    #ifdef Q_OS_WIN
    bool def=false;
    #else
    bool def=version()>=CANTATA_MAKE_VERSION(0, 9, 50);
    #endif
    return GET_BOOL("storeStreamsInMpdDir", def);
}

int Settings::libraryView()
{
    int v=version();
    QString def=ItemView::modeStr(v>=CANTATA_MAKE_VERSION(0, 5, 0)
                                    ? (v<CANTATA_MAKE_VERSION(0, 9, 50) ? ItemView::Mode_SimpleTree : ItemView::Mode_DetailedTree)
                                    : ItemView::Mode_List);
    return ItemView::toMode(GET_STRING("libraryView", def));
}

int Settings::albumsView()
{
    return ItemView::toMode(GET_STRING("albumsView", ItemView::modeStr(ItemView::Mode_IconTop)));
}

int Settings::folderView()
{
    return ItemView::toMode(GET_STRING("folderView", ItemView::modeStr(ItemView::Mode_SimpleTree)));
}

int Settings::playlistsView()
{
    int v=version();
    QString def=ItemView::modeStr(v>=CANTATA_MAKE_VERSION(0, 5, 0)
                                    ? v<CANTATA_MAKE_VERSION(0, 9, 50)
                                        ? ItemView::Mode_SimpleTree
                                        : v<CANTATA_MAKE_VERSION(1, 0, 51)
                                          ? ItemView::Mode_GroupedTree
                                          : ItemView::Mode_DetailedTree
                                    : ItemView::Mode_List);
    return ItemView::toMode(GET_STRING("playlistsView", def));
}

int Settings::streamsView()
{
    int v=version();
    QString def=ItemView::modeStr(v>=CANTATA_MAKE_VERSION(0, 5, 0)
                                    ? (v<CANTATA_MAKE_VERSION(0, 9, 50) ? ItemView::Mode_SimpleTree : ItemView::Mode_DetailedTree)
                                    : ItemView::Mode_List);
    return ItemView::toMode(GET_STRING("streamsView", def));
}

int Settings::onlineView()
{
    return ItemView::toMode(GET_STRING("onlineView", ItemView::modeStr(ItemView::Mode_DetailedTree)));
}

bool Settings::libraryArtistImage()
{
    return GET_BOOL("libraryArtistImage", false);
}

int Settings::libraryCoverSize()
{
    int size=GET_INT("libraryCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
    return size>MusicLibraryItemAlbum::CoverExtraLarge || size<0 ? MusicLibraryItemAlbum::CoverMedium : size;
}

int Settings::albumsCoverSize()
{
    int size=GET_INT("albumsCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
    return size>MusicLibraryItemAlbum::CoverExtraLarge || size<0 ? MusicLibraryItemAlbum::CoverMedium : size;
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

QStringList Settings::wikipediaLangs()
{
    QStringList def=QStringList() << "en:en";
    return GET_STRINGLIST("wikipediaLangs", def);
}

bool Settings::wikipediaIntroOnly()
{
    return GET_BOOL("wikipediaIntroOnly", true);
}

bool Settings::contextBackdrop()
{
    return GET_BOOL("contextBackdrop", true);
}

bool Settings::contextDarkBackground()
{
    return GET_BOOL("contextDarkBackground", false);
}

int Settings::contextZoom()
{
    return GET_INT("contextZoom", 0);
}

QString Settings::page()
{
    return GET_STRING("page", QString());
}

QStringList Settings::hiddenPages()
{
    QStringList def;
    def << "FolderPage";
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
    if (version()<CANTATA_MAKE_VERSION(1, 0, 51)) {
        int v=GET_INT("devicesView", (int)ItemView::Mode_DetailedTree);
        modified=true;
        SET_VALUE("devicesView", ItemView::modeStr((ItemView::Mode)v));
        return v;
    } else {
        return ItemView::toMode(GET_STRING("devicesView", ItemView::modeStr(ItemView::Mode_DetailedTree)));
    }
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
            ver=CANTATA_MAKE_VERSION(0, 9, 0);
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
int Settings::httpAllocatedPort()
{
    return GET_INT("httpAllocatedPort", 0);
}

QString Settings::httpInterface()
{
    return GET_STRING("httpInterface", QString());
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

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
bool Settings::cdAuto()
{
    return GET_BOOL("cdAuto", true);
}
#ifdef LAME_FOUND
bool Settings::cdEncode()
{
    return GET_BOOL("cdEncode", true);
}
#endif
#endif

#if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
bool Settings::useCddb()
{
    return GET_BOOL("useCddb", true);
}
#endif

#ifdef CDDB_FOUND
QString Settings::cddbHost()
{
    return GET_STRING("cddbHost", QString("freedb.freedb.org"));
}

int Settings::cddbPort()
{
    return GET_INT("cddbPort", 8880);
}

bool Settings::paranoiaFull()
{
    return GET_BOOL("paranoiaFull", true);
}

bool Settings::paranoiaNeverSkip()
{
    return GET_BOOL("paranoiaNeverSkip", true);
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

bool Settings::monoSidebarIcons()
{
    #ifdef Q_OS_WIN
    return GET_BOOL("monoSidebarIcons", false);
    #else
    return GET_BOOL("monoSidebarIcons", true);
    #endif
}

QString Settings::amazonAccessKey()
{
    return GET_STRING("amazonAccessKey", QString());
}

QString Settings::amazonSecretAccessKey()
{
    return GET_STRING("amazonSecretAccessKey", QString());
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

#define SET_VALUE_MOD(KEY) if (v!=KEY()) { modified=true; SET_VALUE(#KEY, v); }
#define SET_ITEMVIEW_MODE_VALUE_MOD(KEY) if (v!=KEY()) { modified=true; SET_VALUE(#KEY, ItemView::modeStr((ItemView::Mode)v)); }

void Settings::saveCurrentConnection(const QString &v)
{
    SET_VALUE_MOD(currentConnection)
}

void Settings::saveShowPlaylist(bool v)
{
    SET_VALUE_MOD(showPlaylist)
}

void Settings::saveShowFullScreen(bool v)
{
    SET_VALUE_MOD(showFullScreen)
}

void Settings::savePlayQueueHeaderState(const QByteArray &v)
{
    SET_VALUE_MOD(playQueueHeaderState)
}

void Settings::saveSplitterState(const QByteArray &v)
{
    SET_VALUE_MOD(splitterState)
}

void Settings::saveSplitterAutoHide(bool v)
{
    SET_VALUE_MOD(splitterAutoHide)
}

void Settings::saveMainWindowSize(const QSize &v)
{
    SET_VALUE_MOD(mainWindowSize)
}

void Settings::saveMainWindowCollapsedSize(const QSize &v)
{
    SET_VALUE_MOD(mainWindowCollapsedSize)
}

void Settings::saveUseSystemTray(bool v)
{
    SET_VALUE_MOD(useSystemTray)
}

void Settings::saveMinimiseOnClose(bool v)
{
    SET_VALUE_MOD(minimiseOnClose)
}

void Settings::saveShowPopups(bool v)
{
    SET_VALUE_MOD(showPopups)
}

void Settings::saveStopOnExit(bool v)
{
    SET_VALUE_MOD(stopOnExit)
}

void Settings::saveStopDynamizerOnExit(bool v)
{
    SET_VALUE_MOD(stopDynamizerOnExit)
}

void Settings::saveStopAfterCurrent(bool v)
{
    SET_VALUE_MOD(stopAfterCurrent)
}

void Settings::saveStoreCoversInMpdDir(bool v)
{
    SET_VALUE_MOD(storeCoversInMpdDir)
}

void Settings::saveStoreLyricsInMpdDir(bool v)
{
    SET_VALUE_MOD(storeLyricsInMpdDir)
}

void Settings::saveStoreStreamsInMpdDir(bool v)
{
    SET_VALUE_MOD(storeStreamsInMpdDir)
}

void Settings::saveLibraryView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(libraryView)
}

void Settings::saveAlbumsView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(albumsView)
}

void Settings::saveFolderView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(folderView)
}

void Settings::savePlaylistsView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(playlistsView)
}

void Settings::saveStreamsView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(streamsView)
}

void Settings::saveOnlineView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(onlineView)
}

void Settings::saveLibraryArtistImage(bool v)
{
    SET_VALUE_MOD(libraryArtistImage)
}

void Settings::saveLibraryCoverSize(int v)
{
    SET_VALUE_MOD(libraryCoverSize)
}

void Settings::saveAlbumsCoverSize(int v)
{
    SET_VALUE_MOD(albumsCoverSize)
}

void Settings::saveAlbumSort(int v)
{
    SET_VALUE_MOD(albumSort)
}

void Settings::saveSidebar(int v)
{
    SET_VALUE_MOD(sidebar)
}

void Settings::saveLibraryYear(bool v)
{
    SET_VALUE_MOD(libraryYear)
}

void Settings::saveGroupSingle(bool v)
{
    SET_VALUE_MOD(groupSingle)
}

void Settings::saveGroupMultiple(bool v)
{
    SET_VALUE_MOD(groupMultiple)
}

void Settings::saveLyricProviders(const QStringList &v)
{
    SET_VALUE_MOD(lyricProviders)
}

void Settings::saveWikipediaLangs(const QStringList &v)
{
    SET_VALUE_MOD(wikipediaLangs)
}

void Settings::saveWikipediaIntroOnly(bool v)
{
    SET_VALUE_MOD(wikipediaIntroOnly)
}

void Settings::saveContextBackdrop(bool v)
{
    SET_VALUE_MOD(contextBackdrop)
}

void Settings::saveContextDarkBackground(bool v)
{
    SET_VALUE_MOD(contextDarkBackground)
}

void Settings::saveContextZoom(int v)
{
    SET_VALUE_MOD(contextZoom)
}

void Settings::savePage(const QString &v)
{
    SET_VALUE_MOD(page)
}

void Settings::saveHiddenPages(const QStringList &v)
{
    SET_VALUE_MOD(hiddenPages)
}

void Settings::saveGnomeMediaKeys(bool v)
{
    SET_VALUE_MOD(gnomeMediaKeys)
}

#ifdef ENABLE_DEVICES_SUPPORT
void Settings::saveOverwriteSongs(bool v)
{
    SET_VALUE_MOD(overwriteSongs)
}

void Settings::saveShowDeleteAction(bool v)
{
    SET_VALUE_MOD(showDeleteAction)
}

void Settings::saveDevicesView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(devicesView)
}
#endif

void Settings::saveStopFadeDuration(int v)
{
    if (v<=MinFade) {
        v=0;
    } else if (v>MaxFade) {
        v=MaxFade;
    }
    SET_VALUE_MOD(stopFadeDuration)
}

#ifdef TAGLIB_FOUND
void Settings::saveHttpAllocatedPort(int v)
{
    SET_VALUE_MOD(httpAllocatedPort)
}

void Settings::saveHttpInterface(const QString &v)
{
    SET_VALUE_MOD(httpInterface)
}
#endif

void Settings::savePlayQueueGrouped(bool v)
{
    SET_VALUE_MOD(playQueueGrouped)
}

void Settings::savePlayQueueAutoExpand(bool v)
{
    SET_VALUE_MOD(playQueueAutoExpand)
}

void Settings::savePlayQueueStartClosed(bool v)
{
    SET_VALUE_MOD(playQueueStartClosed)
}

void Settings::savePlayQueueScroll(bool v)
{
    SET_VALUE_MOD(playQueueScroll)
}

void Settings::savePlayListsStartClosed(bool v)
{
    SET_VALUE_MOD(playListsStartClosed)
}

#ifdef PHONON_FOUND
void Settings::savePlayStream(bool v)
{
    SET_VALUE_MOD(playStream)
}

void Settings::saveStreamUrl(const QString &v)
{
    SET_VALUE_MOD(streamUrl)
}
#endif

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
void Settings::saveCdAuto(bool v)
{
    SET_VALUE_MOD(cdAuto)
}
#ifdef LAME_FOUND
void Settings::saveCdEncode(bool v)
{
    SET_VALUE_MOD(cdEncode)
}
#endif
#endif

#if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
void Settings::saveUseCddb(bool v)
{
    SET_VALUE_MOD(useCddb)
}
#endif

#ifdef CDDB_FOUND
void Settings::saveCddbHost(const QString &v)
{
    SET_VALUE_MOD(cddbHost)
}

void Settings::saveCddbPort(int v)
{
    SET_VALUE_MOD(cddbPort)
}

void Settings::saveParanoiaFull(bool v)
{
    SET_VALUE_MOD(paranoiaFull)
}

void Settings::saveParanoiaNeverSkip(bool v)
{
    SET_VALUE_MOD(paranoiaNeverSkip)
}
#endif

void Settings::saveForceSingleClick(bool v)
{
    SET_VALUE_MOD(forceSingleClick)
}

void Settings::saveStartHidden(bool v)
{
    SET_VALUE_MOD(startHidden);
}

void Settings::saveMonoSidebarIcons(bool v)
{
    SET_VALUE_MOD(monoSidebarIcons);
}

void Settings::save(bool force)
{
    if (force) {
        if (version()!=PACKAGE_VERSION || isFirstRun) {
            modified=true;
            SET_VALUE("version", PACKAGE_VERSION_STRING);
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
