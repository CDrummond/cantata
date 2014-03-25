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

#include "settings.h"
#include "musiclibraryitemalbum.h"
#include "fancytabwidget.h"
#include "albumsmodel.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "utils.h"
#include "mediakeys.h"
#include "globalstatic.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KConfig>
#ifdef ENABLE_KWALLET
#include <kwallet.h>
#endif
#include <QApplication>
#include <QWidget>
#include <QTimer>
#else
#include "mediakeys.h"
#endif
#include <QFile>
#include <QDir>
#include <qglobal.h>

GLOBAL_STATIC(Settings, instance)

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

    enum Details {
        DT_DIR    = 0x01,
        DT_ADDR   = 0x02,
        DT_PORT   = 0x04,
        DT_PASSWD = 0x08,

        DT_ALL    = DT_DIR|DT_ADDR|DT_PORT|DT_PASSWD
    };

    void read() {
        QFile f("/etc/mpd.conf");

        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            int details=0;
            while (!f.atEnd()) {
                QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (line.startsWith('#')) {
                    continue;
                } else if (!(details&DT_DIR) && line.startsWith(QLatin1String("music_directory"))) {
                    QString val=getVal(line);
                    if (!val.isEmpty() && QDir(val).exists()) {
                        dir=Utils::fixPath(val);
                        details|=DT_DIR;
                    }
                } else if (!(details&DT_ADDR) && line.startsWith(QLatin1String("bind_to_address"))) {
                    QString val=getVal(line);
                    if (!val.isEmpty() && val!=QLatin1String("any")) {
                        host=val;
                        details|=DT_ADDR;
                    }
                } else if (!(details&DT_PORT) && line.startsWith(QLatin1String("port"))) {
                    int val=getVal(line).toInt();
                    if (val>0) {
                        port=val;
                        details|=DT_PORT;
                    }
                } else if (!(details&DT_PASSWD) && line.startsWith(QLatin1String("password"))) {
                    QString val=getVal(line);
                    if (!val.isEmpty()) {
                        QStringList parts=val.split('@');
                        if (!parts.isEmpty()) {
                            passwd=parts[0];
                            details|=DT_PASSWD;
                        }
                    }
                }
                if (details==DT_ALL) {
                    break;
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

static QString getStartupStateStr(Settings::StartupState s)
{
    switch (s) {
    case Settings::SS_ShowMainWindow: return QLatin1String("show");
    case Settings::SS_HideMainWindow: return QLatin1String("hide");
    default:
    case Settings::SS_Previous: return QLatin1String("prev");
    }
}

static Settings::StartupState getStartupState(const QString &str)
{
    for (int i=0; i<=Settings::SS_Previous; ++i) {
        if (getStartupStateStr((Settings::StartupState)i)==str) {
            return (Settings::StartupState)i;
        }
    }
    return Settings::SS_Previous;
}

Settings::Settings()
    : isFirstRun(false)
    , modified(false)
    , timer(0)
    , ver(-1)
    #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
    , wallet(0)
    #endif
{
    // Only need to read system defaults if we have not previously been configured...
    if (version()<CANTATA_MAKE_VERSION(0, 8, 0)
            ? cfg.get("connectionHost", QString()).isEmpty()
            : !cfg.hasGroup(MPDConnectionDetails::configGroupName())) {
        mpdDefaults.read();
    }
}

Settings::~Settings()
{
    #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
    delete wallet;
    #endif
}

QString Settings::currentConnection()
{
    return cfg.get("currentConnection", QString());
}

MPDConnectionDetails Settings::connectionDetails(const QString &name)
{
    MPDConnectionDetails details;
    if (version()<CANTATA_MAKE_VERSION(0, 8, 0) || (name.isEmpty() && !cfg.hasGroup(MPDConnectionDetails::configGroupName(name)))) {
        details.hostname=cfg.get("connectionHost", name.isEmpty() ? mpdDefaults.host : QString());
        #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
        if (cfg.get("connectionPasswd", false)) {
            if (openWallet()) {
                wallet->readPassword("mpd", details.password);
            }
        } else if (name.isEmpty()) {
           details.password=mpdDefaults.passwd;
        }
        #else
        details.password=cfg.get("connectionPasswd", name.isEmpty() ? mpdDefaults.passwd : QString());
        #endif
        details.port=cfg.get("connectionPort", name.isEmpty() ? mpdDefaults.port : 6600);
        #ifdef Q_OS_WIN32
        details.dir=Utils::fixPath(QDir::fromNativeSeparators(cfg.get("mpdDir", mpdDefaults.dir)));
        #else
        details.dir=Utils::fixPath(cfg.get("mpdDir", mpdDefaults.dir));
        #endif
        details.dynamizerPort=0;
    } else {
        QString n=MPDConnectionDetails::configGroupName(name);
        details.name=name;
        if (!cfg.hasGroup(n)) {
            details.name=QString();
            n=MPDConnectionDetails::configGroupName(details.name);
        }
        if (cfg.hasGroup(n)) {
            cfg.beginGroup(n);
            details.hostname=cfg.get("host", name.isEmpty() ? mpdDefaults.host : QString());
            details.port=cfg.get("port", name.isEmpty() ? mpdDefaults.port : 6600);
            #ifdef Q_OS_WIN32
            details.dir=Utils::fixPath(QDir::fromNativeSeparators(cfg.get("dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music")));
            #else
            details.dir=Utils::fixPath(cfg.get("dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music"));
            #endif
            #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
            if (KWallet::Wallet::isEnabled()) {
                if (cfg.get("passwd", false)) {
                    if (openWallet()) {
                        wallet->readPassword(name.isEmpty() ? "mpd" : name, details.password);
                    }
                } else if (name.isEmpty()) {
                    details.password=mpdDefaults.passwd;
                }
            } else
            #endif // ENABLE_KWALLET
            details.password=cfg.get("passwd", name.isEmpty() ? mpdDefaults.passwd : QString());
            details.dynamizerPort=cfg.get("dynamizerPort", 0);
            details.coverName=cfg.get("coverName", QString());
            #ifdef ENABLE_HTTP_STREAM_PLAYBACK
            details.streamUrl=cfg.get("streamUrl", QString());
            #endif
            cfg.endGroup();
        } else {
            details.hostname=mpdDefaults.host;
            details.port=mpdDefaults.port;
            details.dir=mpdDefaults.dir;
            details.password=mpdDefaults.passwd;
            details.dynamizerPort=0;
            details.coverName=QString();
            #ifdef ENABLE_HTTP_STREAM_PLAYBACK
            details.streamUrl=QString();
            #endif
        }
    }
    details.setDirReadable();
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
        if (cfg.hasGroup(grp) && grp.startsWith("Connection")) {
            connections.append(connectionDetails(grp=="Connection" ? QString() : grp.mid(11)));
        }
    }

    if (connections.isEmpty()) {
        // If we are empty, add at lease the default connection...
        connections.append(connectionDetails());
    }
    return connections;
}

#if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
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

#ifndef ENABLE_KDE_SUPPORT
QString Settings::iconTheme()
{
    return cfg.get("iconTheme", QString());
}
#endif

bool Settings::showFullScreen()
{
    return cfg.get("showFullScreen", false);
}

QByteArray Settings::headerState(const QString &key)
{
    if (version()<CANTATA_MAKE_VERSION(1, 2, 54)) {
        return QByteArray();
    }
    return cfg.get(key+"HeaderState", QByteArray());
}

QByteArray Settings::splitterState()
{
    return cfg.get("splitterState", QByteArray());
}

bool Settings::splitterAutoHide()
{
    return cfg.get("splitterAutoHide", false);
}

QSize Settings::mainWindowSize()
{
    return cfg.get("mainWindowSize", QSize());
}

bool Settings::useSystemTray()
{
    return cfg.get("useSystemTray", false);
}

bool Settings::minimiseOnClose()
{
    return cfg.get("minimiseOnClose", true);
}

bool Settings::showPopups()
{
    return cfg.get("showPopups", false);
}

bool Settings::stopOnExit()
{
    return cfg.get("stopOnExit", false);
}

bool Settings::stopDynamizerOnExit()
{
    return cfg.get("stopDynamizerOnExit", false);
}

bool Settings::storeCoversInMpdDir()
{
    return cfg.get("storeCoversInMpdDir", true);
}

bool Settings::storeLyricsInMpdDir()
{
    return cfg.get("storeLyricsInMpdDir", true);
}

bool Settings::storeStreamsInMpdDir()
{
    #ifdef Q_OS_WIN
    bool def=false;
    #else
    bool def=version()>=CANTATA_MAKE_VERSION(0, 9, 50);
    #endif
    return cfg.get("storeStreamsInMpdDir", def);
}

bool Settings::storeBackdropsInMpdDir()
{
    return cfg.get("storeBackdropsInMpdDir", false);
}

int Settings::libraryView()
{
    int v=version();
    QString def=ItemView::modeStr(v<CANTATA_MAKE_VERSION(0, 9, 50) ? ItemView::Mode_SimpleTree : ItemView::Mode_DetailedTree);
    return ItemView::toMode(cfg.get("libraryView", def));
}

int Settings::albumsView()
{
    return ItemView::toMode(cfg.get("albumsView", ItemView::modeStr(ItemView::Mode_IconTop)));
}

int Settings::folderView()
{
    int v=version();
    QString def=ItemView::modeStr(v<CANTATA_MAKE_VERSION(1, 0, 51) ? ItemView::Mode_SimpleTree : ItemView::Mode_DetailedTree);
    return ItemView::toMode(cfg.get("folderView", def));
}

int Settings::playlistsView()
{
    int v=version();
    QString def=ItemView::modeStr(v<CANTATA_MAKE_VERSION(0, 9, 50)
                                    ? ItemView::Mode_SimpleTree
                                    : v<CANTATA_MAKE_VERSION(1, 0, 51)
                                        ? ItemView::Mode_GroupedTree
                                        : ItemView::Mode_DetailedTree);
    return ItemView::toMode(cfg.get("playlistsView", def));
}

int Settings::streamsView()
{
    int v=version();
    QString def=ItemView::modeStr(v<CANTATA_MAKE_VERSION(0, 9, 50) ? ItemView::Mode_SimpleTree : ItemView::Mode_DetailedTree);
    return ItemView::toMode(cfg.get("streamsView", def));
}

int Settings::onlineView()
{
    return ItemView::toMode(cfg.get("onlineView", ItemView::modeStr(ItemView::Mode_DetailedTree)));
}

bool Settings::libraryArtistImage()
{
    if (cfg.get("libraryArtistImage", false)) {
        int view=libraryView();
        return ItemView::Mode_SimpleTree!=view && ItemView::Mode_BasicTree!=view;
    }
    return false;
}

int Settings::libraryCoverSize()
{
    int size=cfg.get("libraryCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
    return size>MusicLibraryItemAlbum::CoverExtraLarge || size<0 ? MusicLibraryItemAlbum::CoverMedium : size;
}

int Settings::albumsCoverSize()
{
    int size=cfg.get("albumsCoverSize", (int)(MusicLibraryItemAlbum::CoverMedium));
    return size>MusicLibraryItemAlbum::CoverExtraLarge || size<0 ? MusicLibraryItemAlbum::CoverMedium : size;
}

int Settings::albumSort()
{
    return cfg.get("albumSort", 0);
}

int Settings::sidebar()
{
    if (version()<CANTATA_MAKE_VERSION(1, 2, 52)) {
        switch (cfg.get("sidebar", 1)) {
        default:
        case 1: return FancyTabWidget::Side|FancyTabWidget::Large;
        case 2: return FancyTabWidget::Side|FancyTabWidget::Small;
        case 3: return FancyTabWidget::Side|FancyTabWidget::Tab;
        case 4: return FancyTabWidget::Top|FancyTabWidget::Tab;
        case 5: return FancyTabWidget::Top|FancyTabWidget::Tab|FancyTabWidget::IconOnly;
        case 6: return FancyTabWidget::Bot|FancyTabWidget::Tab;
        case 7: return FancyTabWidget::Bot|FancyTabWidget::Tab|FancyTabWidget::IconOnly;
        case 8: return FancyTabWidget::Side|FancyTabWidget::Large|FancyTabWidget::IconOnly;
        case 9: return FancyTabWidget::Side|FancyTabWidget::Small|FancyTabWidget::IconOnly;
        case 10: return FancyTabWidget::Side|FancyTabWidget::Tab|FancyTabWidget::IconOnly;
        case 11: return FancyTabWidget::Bot|FancyTabWidget::Large;
        case 12: return FancyTabWidget::Bot|FancyTabWidget::Large|FancyTabWidget::IconOnly;
        case 13: return FancyTabWidget::Top|FancyTabWidget::Large;
        case 14: return FancyTabWidget::Top|FancyTabWidget::Large|FancyTabWidget::IconOnly;
        case 15: return FancyTabWidget::Bot|FancyTabWidget::Small;
        case 16: return FancyTabWidget::Bot|FancyTabWidget::Small|FancyTabWidget::IconOnly;
        case 17: return FancyTabWidget::Top|FancyTabWidget::Small;
        case 18: return FancyTabWidget::Top|FancyTabWidget::Small|FancyTabWidget::IconOnly;
        }
    } else {
        return cfg.get("sidebar", (int)(FancyTabWidget::Side|FancyTabWidget::Large))&FancyTabWidget::All_Mask;
    }
}

bool Settings::libraryYear()
{
    return cfg.get("libraryYear", true);
}

bool Settings::groupSingle()
{
    return cfg.get("groupSingle", MPDParseUtils::groupSingle());
}

bool Settings::groupMultiple()
{
    return cfg.get("groupMultiple", MPDParseUtils::groupMultiple());
}

bool Settings::useComposer()
{
    return cfg.get("useComposer", Song::useComposer());
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
    return cfg.get("lyricProviders", def);
}

QStringList Settings::wikipediaLangs()
{
    QStringList def=QStringList() << "en:en";
    return cfg.get("wikipediaLangs", def);
}

bool Settings::wikipediaIntroOnly()
{
    return cfg.get("wikipediaIntroOnly", true);
}

int Settings::contextBackdrop()
{
    if (version()<CANTATA_MAKE_VERSION(1, 0, 53)) {
        return cfg.get("contextBackdrop", true) ? 1 : 1;
    }
    return  cfg.get("contextBackdrop", 1);
}

int Settings::contextBackdropOpacity()
{
    return cfg.get("contextBackdropOpacity", 15, 0, 100);
}

int Settings::contextBackdropBlur()
{
    return cfg.get("contextBackdropBlur", 0, 0, 20);
}

QString Settings::contextBackdropFile()
{
    return cfg.get("contextBackdropFile", QString());
}

bool Settings::contextDarkBackground()
{
    return cfg.get("contextDarkBackground", false);
}

int Settings::contextZoom()
{
    return cfg.get("contextZoom", 0);
}

QString Settings::contextSlimPage()
{
    return cfg.get("contextSlimPage", QString());
}

QByteArray Settings::contextSplitterState()
{
    return cfg.get("contextSplitterState", QByteArray());
}

bool Settings::contextAlwaysCollapsed()
{
    return cfg.get("contextAlwaysCollapsed", false);
}


int Settings::contextSwitchTime()
{
    return cfg.get("contextSwitchTime", 0, 0, 5000);
}

bool Settings::contextAutoScroll()
{
    return cfg.get("contextAutoScroll", false);
}

QString Settings::page()
{
    return cfg.get("page", QString());
}

QStringList Settings::hiddenPages()
{
    QStringList def=QStringList() << "PlayQueuePage" << "FolderPage" << "SearchPage" << "ContextPage";
    QStringList config=cfg.get("hiddenPages", def);
    if (version()<CANTATA_MAKE_VERSION(1, 2, 51) && !config.contains("SearchPage")) {
        config.append("SearchPage");
    }
    // If splitter auto-hide is enabled, then playqueue cannot be in sidebar!
    if (splitterAutoHide() && !config.contains("PlayQueuePage")) {
        config << "PlayQueuePage";
    }
    return config;
}

#ifndef ENABLE_KDE_SUPPORT
QString Settings::mediaKeysIface()
{
    #if defined Q_OS_WIN
    return cfg.get("mediaKeysIface", MediaKeys::toString(MediaKeys::QxtInterface));
    #else
    return cfg.get("mediaKeysIface", MediaKeys::toString(MediaKeys::GnomeInteface));
    #endif
}
#endif

#ifdef ENABLE_DEVICES_SUPPORT
bool Settings::overwriteSongs()
{
    return cfg.get("overwriteSongs", false);
}

bool Settings::showDeleteAction()
{
    return cfg.get("showDeleteAction", false);
}

int Settings::devicesView()
{
    if (version()<CANTATA_MAKE_VERSION(1, 0, 51)) {
        int v=cfg.get("devicesView", (int)ItemView::Mode_DetailedTree);
        modified=true;
        cfg.set("devicesView", ItemView::modeStr((ItemView::Mode)v));
        return v;
    } else {
        return ItemView::toMode(cfg.get("devicesView", ItemView::modeStr(ItemView::Mode_DetailedTree)));
    }
}
#endif

int Settings::searchView()
{
    return ItemView::toMode(cfg.get("searchView", ItemView::modeStr(ItemView::Mode_List)));
}

int Settings::version()
{
    if (-1==ver) {
        isFirstRun=!cfg.hasEntry("version");
        QStringList parts=cfg.get("version", QLatin1String(PACKAGE_VERSION_STRING)).split('.');
        if (3==parts.size()) {
            ver=CANTATA_MAKE_VERSION(parts.at(0).toInt(), parts.at(1).toInt(), parts.at(2).toInt());
        } else {
            ver=PACKAGE_VERSION;
            cfg.set("version", PACKAGE_VERSION_STRING);
        }
    }
    return ver;
}

int Settings::stopFadeDuration()
{
    int v=cfg.get("stopFadeDuration", (int)DefaultFade);
    if (0!=v && (v<MinFade || v>MaxFade)) {
        v=DefaultFade;
    }
    return v;
}

int Settings::httpAllocatedPort()
{
    return cfg.get("httpAllocatedPort", 0);
}

QString Settings::httpInterface()
{
    return cfg.get("httpInterface", QString());
}

bool Settings::alwaysUseHttp()
{
    #ifdef ENABLE_HTTP_SERVER
    return cfg.get("alwaysUseHttp", false);
    #else
    return false;
    #endif
}

bool Settings::playQueueGrouped()
{
    return cfg.get("playQueueGrouped", true);
}

bool Settings::playQueueAutoExpand()
{
    return cfg.get("playQueueAutoExpand", true);
}

bool Settings::playQueueStartClosed()
{
    return cfg.get("playQueueStartClosed", false);
}

bool Settings::playQueueScroll()
{
    return cfg.get("playQueueScroll", true);
}

int Settings::playQueueBackground()
{
    if (version()<CANTATA_MAKE_VERSION(1, 0, 53)) {
        return cfg.get("playQueueBackground", false) ? 1 : 1;
    }
    return  cfg.get("playQueueBackground", 0);
}

int Settings::playQueueBackgroundOpacity()
{
    return cfg.get("playQueueBackgroundOpacity", 15, 0, 100);
}

int Settings::playQueueBackgroundBlur()
{
    return cfg.get("playQueueBackgroundBlur", 0, 0, 20);
}

QString Settings::playQueueBackgroundFile()
{
    return cfg.get("playQueueBackgroundFile", QString());
}

bool Settings::playQueueConfirmClear()
{
    return cfg.get("playQueueConfirmClear", true);
}

bool Settings::playListsStartClosed()
{
    return cfg.get("playListsStartClosed", true);
}

#ifdef ENABLE_HTTP_STREAM_PLAYBACK
bool Settings::playStream()
{
    return cfg.get("playStream", false);
}
#endif

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
bool Settings::cdAuto()
{
    return cfg.get("cdAuto", true);
}

bool Settings::paranoiaFull()
{
    return cfg.get("paranoiaFull", true);
}

bool Settings::paranoiaNeverSkip()
{
    return cfg.get("paranoiaNeverSkip", true);
}
#endif

#if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
bool Settings::useCddb()
{
    return cfg.get("useCddb", true);
}
#endif

#ifdef CDDB_FOUND
QString Settings::cddbHost()
{
    return cfg.get("cddbHost", QString("freedb.freedb.org"));
}

int Settings::cddbPort()
{
    return cfg.get("cddbPort", 8880);
}
#endif

bool Settings::forceSingleClick()
{
    return cfg.get("forceSingleClick", true);
}

bool Settings::startHidden()
{
    return cfg.get("startHidden", false);
}

bool Settings::monoSidebarIcons()
{
    #ifdef Q_OS_WIN
    return cfg.get("monoSidebarIcons", false);
    #else
    return cfg.get("monoSidebarIcons", true);
    #endif
}

bool Settings::showTimeRemaining()
{
    return cfg.get("showTimeRemaining", false);
}

QStringList Settings::hiddenStreamCategories()
{
    return cfg.get("hiddenStreamCategories", QStringList());
}

QStringList Settings::hiddenOnlineProviders()
{
    return cfg.get("hiddenOnlineProviders", QStringList());
}

#ifndef Q_OS_WIN32
bool Settings::inhibitSuspend()
{
    return cfg.get("inhibitSuspend", false);
}
#endif

int Settings::rssUpdate()
{
    return cfg.get("rssUpdate", 0, 0, 7*24*60);
}

QDateTime Settings::lastRssUpdate()
{
    return cfg.get("lastRssUpdate", QDateTime());
}

QString Settings::podcastDownloadPath()
{
    return Utils::fixPath(cfg.get("podcastDownloadPath", Utils::fixPath(QDir::homePath())+QLatin1String("Podcasts/")));
}

bool Settings::podcastAutoDownload()
{
    return cfg.get("podcastAutoDownload", false);
}

int Settings::maxCoverUpdatePerIteration()
{
    return cfg.get("maxCoverUpdatePerIteration", 10, 1, 50);
}

int Settings::coverCacheSize()
{
    return cfg.get("coverCacheSize", 10, 1, 512);
}

QStringList Settings::cueFileCodecs()
{
    return cfg.get("cueFileCodecs", QStringList());
}

bool Settings::networkAccessEnabled()
{
    return cfg.get("networkAccessEnabled", true);
}

int Settings::volumeStep()
{
    return cfg.get("volumeStep", 5, 1, 20);
}

Settings::StartupState Settings::startupState()
{
    return getStartupState(cfg.get("startupState", getStartupStateStr(SS_Previous)));
}

int Settings::undoSteps()
{
    return cfg.get("undoSteps", 10, 0, 20);
}

QString Settings::searchCategory()
{
    return cfg.get("searchCategory", QString());
}

bool Settings::cacheScaledCovers()
{
    return cfg.get("cacheScaledCovers", true);
}

bool Settings::fetchCovers()
{
    return cfg.get("fetchCovers", true);
}

int Settings::mpdPoll()
{
    return cfg.get("mpdPoll", 0, 0, 60);
}

int Settings::mpdListSize()
{
    return cfg.get("mpdListSize", 10000, 100, 65535);
}

#ifndef ENABLE_KDE_SUPPORT
QString Settings::lang()
{
    return cfg.get("lang", QString());
}
#endif

bool Settings::alwaysUseLsInfo()
{
    return cfg.get("alwaysUseLsInfo", false);
}

bool Settings::showMenubar()
{
    return cfg.get("showMenubar", false);
}

int Settings::menu()
{
    #if defined Q_OS_WIN
    int def=MC_Button;
    #elif defined Q_OS_MAC
    int def=MC_Bar;
    #else
    int def=Utils::Gnome==Utils::currentDe() ? MC_Button : Utils::Unity==Utils::currentDe() ? MC_Bar : (MC_Bar|MC_Button);
    #endif
    int v=cfg.get("menu", def)&(MC_Bar|MC_Button);
    return 0==v ? MC_Bar : v;
}

void Settings::removeConnectionDetails(const QString &v)
{
    if (v==currentConnection()) {
        saveCurrentConnection(QString());
    }
    cfg.removeGroup(MPDConnectionDetails::configGroupName(v));
    #ifdef ENABLE_KDE_SUPPORT
    KGlobal::config()->sync();
    #endif
    modified=true;
}

void Settings::saveConnectionDetails(const MPDConnectionDetails &v)
{
    if (v.name.isEmpty()) {
        cfg.removeEntry("connectionHost");
        cfg.removeEntry("connectionPasswd");
        cfg.removeEntry("connectionPort");
        cfg.removeEntry("mpdDir");
    }

    QString n=MPDConnectionDetails::configGroupName(v.name);

    cfg.beginGroup(n);
    cfg.set("host", v.hostname);
    cfg.set("port", (int)v.port);
    cfg.set("dir", v.dir);
    #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
    if (KWallet::Wallet::isEnabled()) {
        cfg.set("passwd", !v.password.isEmpty());
        QString walletEntry=v.name.isEmpty() ? "mpd" : v.name;
        if (v.password.isEmpty()) {
            if (wallet) {
                wallet->removeEntry(walletEntry);
            }
        }
        else if (openWallet()) {
            wallet->writePassword(walletEntry, v.password);
        }
    } else
    #endif // ENABLE_KWALLET
    cfg.set("passwd", v.password);
    cfg.set("dynamizerPort", (int)v.dynamizerPort);
    cfg.set("coverName", v.coverName);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    cfg.set("streamUrl", v.streamUrl);
    #endif
    cfg.endGroup();
    modified=true;
}

#define SET_VALUE_MOD(KEY) if (v!=KEY()) { modified=true; cfg.set(#KEY, v); }
#define SET_ITEMVIEW_MODE_VALUE_MOD(KEY) if (v!=KEY()) { modified=true; cfg.set(#KEY, ItemView::modeStr((ItemView::Mode)v)); }
#define SET_STARTUPSTATE_VALUE_MOD(KEY) if (v!=KEY()) { modified=true; cfg.set(#KEY, getStartupStateStr((StartupState)v)); }

void Settings::saveCurrentConnection(const QString &v)
{
    SET_VALUE_MOD(currentConnection)
}

void Settings::saveShowFullScreen(bool v)
{
    SET_VALUE_MOD(showFullScreen)
}

void Settings::saveHeaderState(const QString &key, const QByteArray &v)
{
    QByteArray current=headerState(key);
    if (current!=v) {
        modified=true;
        cfg.set(key+"HeaderState", v);
    }
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


void Settings::saveStoreBackdropsInMpdDir(bool v)
{
    SET_VALUE_MOD(storeBackdropsInMpdDir)
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

void Settings::saveUseComposer(bool v)
{
    SET_VALUE_MOD(useComposer)
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

void Settings::saveContextBackdrop(int v)
{
    SET_VALUE_MOD(contextBackdrop)
}

void Settings::saveContextBackdropOpacity(int v)
{
    SET_VALUE_MOD(contextBackdropOpacity)
}

void Settings::saveContextBackdropBlur(int v)
{
    SET_VALUE_MOD(contextBackdropBlur)
}

void Settings::saveContextBackdropFile(const QString &v)
{
    SET_VALUE_MOD(contextBackdropFile)
}

void Settings::saveContextDarkBackground(bool v)
{
    SET_VALUE_MOD(contextDarkBackground)
}

void Settings::saveContextZoom(int v)
{
    SET_VALUE_MOD(contextZoom)
}

void Settings::saveContextSlimPage(const QString &v)
{
    SET_VALUE_MOD(contextSlimPage)
}

void Settings::saveContextSplitterState(const QByteArray &v)
{
    SET_VALUE_MOD(contextSplitterState)
}

void Settings::saveContextAlwaysCollapsed(bool v)
{
    SET_VALUE_MOD(contextAlwaysCollapsed)
}

void Settings::saveContextSwitchTime(int v)
{
    SET_VALUE_MOD(contextSwitchTime);
}

void Settings::saveContextAutoScroll(bool v)
{
    SET_VALUE_MOD(contextAutoScroll);
}

void Settings::savePage(const QString &v)
{
    SET_VALUE_MOD(page)
}

void Settings::saveHiddenPages(const QStringList &v)
{
    SET_VALUE_MOD(hiddenPages)
}

#ifndef ENABLE_KDE_SUPPORT
void Settings::saveMediaKeysIface(const QString &v)
{
    SET_VALUE_MOD(mediaKeysIface)
}
#endif

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

void Settings::saveSearchView(int v)
{
    SET_ITEMVIEW_MODE_VALUE_MOD(searchView)
}

void Settings::saveStopFadeDuration(int v)
{
    if (v<=MinFade) {
        v=0;
    } else if (v>MaxFade) {
        v=MaxFade;
    }
    SET_VALUE_MOD(stopFadeDuration)
}

void Settings::saveHttpAllocatedPort(int v)
{
    SET_VALUE_MOD(httpAllocatedPort)
}

void Settings::saveHttpInterface(const QString &v)
{
    SET_VALUE_MOD(httpInterface)
}

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

void Settings::savePlayQueueBackground(int v)
{
    SET_VALUE_MOD(playQueueBackground)
}

void Settings::savePlayQueueBackgroundOpacity(int v)
{
    SET_VALUE_MOD(playQueueBackgroundOpacity)
}

void Settings::savePlayQueueBackgroundBlur(int v)
{
    SET_VALUE_MOD(playQueueBackgroundBlur)
}

void Settings::savePlayQueueBackgroundFile(const QString &v)
{
    SET_VALUE_MOD(playQueueBackgroundFile)
}

void Settings::savePlayQueueConfirmClear(bool v)
{
    SET_VALUE_MOD(playQueueConfirmClear);
}

void Settings::savePlayListsStartClosed(bool v)
{
    SET_VALUE_MOD(playListsStartClosed)
}

#ifdef ENABLE_HTTP_STREAM_PLAYBACK
void Settings::savePlayStream(bool v)
{
    SET_VALUE_MOD(playStream)
}
#endif

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
void Settings::saveCdAuto(bool v)
{
    SET_VALUE_MOD(cdAuto)
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

void Settings::saveShowTimeRemaining(bool v)
{
    SET_VALUE_MOD(showTimeRemaining);
}

void Settings::saveHiddenStreamCategories(const QStringList &v)
{
    SET_VALUE_MOD(hiddenStreamCategories);
}

void Settings::saveHiddenOnlineProviders(const QStringList &v)
{
    SET_VALUE_MOD(hiddenOnlineProviders);
}

#ifndef Q_OS_WIN32
void Settings::saveInhibitSuspend(bool v)
{
    SET_VALUE_MOD(inhibitSuspend);
}
#endif

void Settings::saveRssUpdate(int v)
{
    SET_VALUE_MOD(rssUpdate);
}

void Settings::saveLastRssUpdate(const QDateTime &v)
{
    SET_VALUE_MOD(lastRssUpdate);
}

void Settings::savePodcastDownloadPath(const QString &v)
{
    SET_VALUE_MOD(podcastDownloadPath);
}

void Settings::savePodcastAutoDownload(bool v)
{
    SET_VALUE_MOD(podcastAutoDownload);
}

void Settings::saveStartupState(int v)
{
    SET_STARTUPSTATE_VALUE_MOD(startupState);
}

void Settings::saveSearchCategory(const QString &v)
{
    SET_VALUE_MOD(searchCategory);
}

void Settings::saveCacheScaledCovers(bool v)
{
    SET_VALUE_MOD(cacheScaledCovers);
}

void Settings::saveFetchCovers(bool v)
{
    SET_VALUE_MOD(fetchCovers);
}

#ifndef ENABLE_KDE_SUPPORT
void Settings::saveLang(const QString &v)
{
    SET_VALUE_MOD(lang);
}
#endif

void Settings::saveShowMenubar(bool v)
{
    SET_VALUE_MOD(showMenubar);
}

void Settings::save(bool force)
{
    if (force) {
        if (version()!=PACKAGE_VERSION || isFirstRun) {
            modified=true;
            cfg.set("version", PACKAGE_VERSION_STRING);
            ver=PACKAGE_VERSION;
        }
        if (modified) {
            modified=false;
            cfg.sync();
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
