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

#include "settings.h"
#include "models/sqllibrarymodel.h"
#include "support/fancytabwidget.h"
#include "widgets/itemview.h"
#include "mpd-interface/mpdparseutils.h"
#include "support/utils.h"
#include "support/globalstatic.h"
#include "db/librarydb.h"
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
    : state(AP_Configured)
    , ver(-1)
{
    // Call 'version' so that it initialises 'ver' and 'state'
    version();
    // Only need to read system defaults if we have not previously been configured...
    if (!cfg.hasGroup(MPDConnectionDetails::configGroupName())) {
        mpdDefaults.read();
    }
}

Settings::~Settings()
{
    save();
}

QString Settings::currentConnection()
{
    return cfg.get("currentConnection", QString());
}

MPDConnectionDetails Settings::connectionDetails(const QString &name)
{
    MPDConnectionDetails details;
    QString n=MPDConnectionDetails::configGroupName(name);
    details.name=name;
    if (!cfg.hasGroup(n)) {
        details.name=QString();
        n=MPDConnectionDetails::configGroupName(details.name);
    }
    if (cfg.hasGroup(n)) {
        Configuration grp(n);
        details.hostname=grp.get("host", name.isEmpty() ? mpdDefaults.host : QString());
        details.port=grp.get("port", name.isEmpty() ? mpdDefaults.port : 6600);
        details.dir=grp.getDirPath("dir", name.isEmpty() ? mpdDefaults.dir : "/var/lib/mpd/music");
        details.password=grp.get("passwd", name.isEmpty() ? mpdDefaults.passwd : QString());
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        details.streamUrl=grp.get("streamUrl", QString());
        #endif
        details.replayGain=grp.get("replayGain", QString());
        details.applyReplayGain=grp.get("applyReplayGain", true);
        // if the setting hasn't been set before, we set it to true to allow
        // for easy migration of existing settings
        details.allowLocalStreaming=grp.get("allowLocalStreaming", true);
        details.autoUpdate=grp.get("autoUpdate", false);
    } else {
        details.hostname=mpdDefaults.host;
        details.port=mpdDefaults.port;
        details.dir=mpdDefaults.dir;
        details.password=mpdDefaults.passwd;
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        details.streamUrl=QString();
        #endif
        details.applyReplayGain=true;
        details.allowLocalStreaming=true;
        details.autoUpdate=false;
    }
    details.setDirReadable();
    return details;
}

QList<MPDConnectionDetails> Settings::allConnections()
{
    QStringList groups=cfg.childGroups();
    QList<MPDConnectionDetails> connections;
    for (const QString &grp: groups) {
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

bool Settings::showPlaylist()
{
    return cfg.get("showPlaylist", true);
}

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

QSize Settings::mainWindowCollapsedSize()
{
    return cfg.get("mainWindowCollapsedSize", QSize());
}

bool Settings::maximized()
{
    return cfg.get("maximized", false);
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

bool Settings::storeCoversInMpdDir()
{
    return cfg.get("storeCoversInMpdDir", false);
}

bool Settings::storeLyricsInMpdDir()
{
    return cfg.get("storeLyricsInMpdDir", false);
}

QString Settings::coverFilename()
{
    QString name=cfg.get("coverFilename", QString());
    // name is empty, so for old configs try to get from MPD settings
    if (name.isEmpty() && version()<CANTATA_MAKE_VERSION(2, 2, 51)) {
        QStringList groups=cfg.childGroups();
        QList<MPDConnectionDetails> connections;
        for (const QString &grp: groups) {
            if (cfg.hasGroup(grp) && grp.startsWith("Connection")) {
                Configuration cfg(grp);
                name=cfg.get("coverName", QString());
                if (!name.isEmpty()) {
                    // Use 1st non-empty
                    saveCoverFilename(name);
                    return name;
                }
            }
        }
    }
    return name;
}

int Settings::sidebar()
{
    if (version()<CANTATA_MAKE_VERSION(1, 2, 52)) {
        return (int)(FancyTabWidget::Side|FancyTabWidget::Large);
    } else {
        return cfg.get("sidebar", (int)(FancyTabWidget::Side|FancyTabWidget::Large))&FancyTabWidget::All_Mask;
    }
}

QSet<QString> Settings::composerGenres()
{
    return cfg.get("composerGenres", Song::composerGenres().toList()).toSet();
}

QSet<QString> Settings::singleTracksFolders()
{
    return cfg.get("singleTracksFolders", QStringList()).toSet();
}

MPDParseUtils::CueSupport Settings::cueSupport()
{
    return MPDParseUtils::toCueSupport(cfg.get("cueSupport", MPDParseUtils::toStr(MPDParseUtils::Cue_Parse)));
}

QStringList Settings::lyricProviders()
{
    return cfg.get("lyricProviders", QStringList() << "lyrics.wikia.com" << "lyricstime.com" << "lyricsreg.com"
                                                   << "lyricsmania.com" << "metrolyrics.com" << "azlyrics.com"
                                                   << "songlyrics.com" << "elyrics.net" << "lyricsdownload.com"
                                                   << "lyrics.com" << "lyricsbay.com" << "directlyrics.com"
                                                   << "loudson.gs" << "teksty.org" << "tekstowo.pl (POLISH)"
                                                   << "vagalume.uol.com.br" << "vagalume.uol.com.br (PORTUGUESE)");
}

QStringList Settings::wikipediaLangs()
{
    return cfg.get("wikipediaLangs", QStringList() << "en:en");
}

bool Settings::wikipediaIntroOnly()
{
    return cfg.get("wikipediaIntroOnly", true);
}

int Settings::contextBackdrop()
{
    return getBoolAsInt("contextBackdrop", 0);
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
    return cfg.getFilePath("contextBackdropFile", QString());
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
    return version()<CANTATA_MAKE_VERSION(1, 3, 51) ? QByteArray() : cfg.get("contextSplitterState", QByteArray());
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

int Settings::contextTrackView()
{
    return cfg.get("contextTrackView", 0);
}

QString Settings::page()
{
    return cfg.get("page", QString());
}

QStringList Settings::hiddenPages()
{
    QStringList config=cfg.get("hiddenPages", QStringList() << "PlayQueuePage" << "ContextPage");
    // If splitter auto-hide is enabled, then playqueue cannot be in sidebar!
    if (splitterAutoHide() && !config.contains("PlayQueuePage")) {
        config << "PlayQueuePage";
    }
    return config;
}

#ifdef ENABLE_DEVICES_SUPPORT
bool Settings::overwriteSongs()
{
    return cfg.get("overwriteSongs", false);
}

bool Settings::showDeleteAction()
{
    return cfg.get("showDeleteAction", false);
}
#endif

int Settings::version()
{
    if (-1==ver) {
        state=cfg.hasEntry("version") ? AP_Configured : AP_FirstRun;
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
    return cfg.get("stopFadeDuration", (int)MPDConnection::DefaultFade, (int)MPDConnection::MinFade, (int)MPDConnection::MaxFade);
}

int Settings::httpAllocatedPort()
{
    return cfg.get("httpAllocatedPort", 0);
}

QString Settings::httpInterface()
{
    return cfg.get("httpInterface", QString());
}

int Settings::playQueueView()
{
    if (version()<CANTATA_MAKE_VERSION(1, 3, 53)) {
        return cfg.get("playQueueGrouped", true) ? ItemView::Mode_GroupedTree : ItemView::Mode_Table;
    }
    return ItemView::toMode(cfg.get("playQueueView", ItemView::modeStr(ItemView::Mode_GroupedTree)));
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
    return getBoolAsInt("playQueueBackground", 0);
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
    return cfg.getFilePath("playQueueBackgroundFile", QString());
}

bool Settings::playQueueConfirmClear()
{
    return cfg.get("playQueueConfirmClear", false);
}

bool Settings::playQueueSearch()
{
    return cfg.get("playQueueSearch", true);
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

int Settings::paranoiaOffset()
{
    return cfg.get("paranoiaOffset", 0);
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
    return useSystemTray() ? cfg.get("startHidden", false) : false;
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

#if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
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

int Settings::podcastAutoDownloadLimit()
{
    if (cfg.hasEntry("podcastAutoDownload")) {
        return cfg.get("podcastAutoDownload", false) ? 1000 : 0;
    }
    return cfg.get("podcastAutoDownloadLimit", 0, 0, 1000);
}

int Settings::volumeStep()
{
    return cfg.get("volumeStep", 5, 1, 20);
}

Settings::StartupState Settings::startupState()
{
    return getStartupState(cfg.get("startupState", getStartupStateStr(SS_Previous)));
}

QString Settings::searchCategory()
{
    return cfg.get("searchCategory", QString());
}

bool Settings::fetchCovers()
{
    return cfg.get("fetchCovers", true);
}

QString Settings::lang()
{
    return cfg.get("lang", QString());
}

bool Settings::showCoverWidget()
{
    return cfg.get("showCoverWidget", true);
}

bool Settings::showStopButton()
{
    return cfg.get("showStopButton", false);
}

bool Settings::showRatingWidget()
{
    return cfg.get("showRatingWidget", false);
}

bool Settings::showTechnicalInfo()
{
    return cfg.get("showTechnicalInfo", false);
}

bool Settings::infoTooltips()
{
    return cfg.get("infoTooltips", true);
}

QSet<QString> Settings::ignorePrefixes()
{
    return cfg.get("ignorePrefixes", Song::ignorePrefixes().toList()).toSet();
}

bool Settings::mpris()
{
    return cfg.get("mpris", true);
}

QString Settings::style()
{
    return cfg.get("style", QString());
}

#if !defined Q_OS_WIN && !defined Q_OS_MAC
bool Settings::showMenubar()
{
    return cfg.get("showMenubar", false);
}
#endif

bool Settings::useOriginalYear()
{
    return cfg.get("useOriginalYear", false);
}

bool Settings::responsiveSidebar()
{
    return cfg.get("responsiveSidebar", true);
}

void Settings::removeConnectionDetails(const QString &v)
{
    if (v==currentConnection()) {
        saveCurrentConnection(QString());
    }
    cfg.removeGroup(MPDConnectionDetails::configGroupName(v));
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
    Configuration grp(n);
    grp.set("host", v.hostname);
    grp.set("port", (int)v.port);
    if (v.dir.startsWith("http:", Qt::CaseInsensitive) || v.dir.startsWith("https:", Qt::CaseInsensitive)) {
        grp.set("dir", v.dir);
    } else {
        grp.setDirPath("dir", v.dir);
    }
    grp.set("passwd", v.password);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    grp.set("streamUrl", v.streamUrl);
    #endif
    grp.set("applyReplayGain", v.applyReplayGain);
    grp.set("allowLocalStreaming", v.allowLocalStreaming);
    grp.set("autoUpdate", v.autoUpdate);
}

void Settings::saveCurrentConnection(const QString &v)
{
    cfg.set("currentConnection", v);
}

void Settings::saveShowFullScreen(bool v)
{
    cfg.set("showFullScreen", v);
}

void Settings::saveShowPlaylist(bool v)
{
    cfg.set("showPlaylist", v);
}

void Settings::saveHeaderState(const QString &key, const QByteArray &v)
{
    cfg.set(key+"HeaderState", v);
}

void Settings::saveSplitterState(const QByteArray &v)
{
    cfg.set("splitterState", v);
}

void Settings::saveSplitterAutoHide(bool v)
{
    cfg.set("splitterAutoHide", v);
}

void Settings::saveMainWindowSize(const QSize &v)
{
    cfg.set("mainWindowSize", v);
}

void Settings::saveMaximized(bool v)
{
    cfg.set("maximized", v);
}

void Settings::saveMainWindowCollapsedSize(const QSize &v)
{
    if (v.width()>16 && v.height()>16) {
        cfg.set("mainWindowCollapsedSize", v);
    }
}

void Settings::saveUseSystemTray(bool v)
{
    cfg.set("useSystemTray", v);
}

void Settings::saveMinimiseOnClose(bool v)
{
    cfg.set("minimiseOnClose", v);
}

void Settings::saveShowPopups(bool v)
{
    cfg.set("showPopups", v);
}

void Settings::saveStopOnExit(bool v)
{
    cfg.set("stopOnExit", v);
}

void Settings::saveStoreCoversInMpdDir(bool v)
{
    cfg.set("storeCoversInMpdDir", v);
}

void Settings::saveStoreLyricsInMpdDir(bool v)
{
    cfg.set("storeLyricsInMpdDir", v);
}

void Settings::saveCoverFilename(const QString &v)
{
    cfg.set("coverFilename", v);
}

void Settings::saveSidebar(int v)
{
    cfg.set("sidebar", v);
}

void Settings::saveComposerGenres(const QSet<QString> &v)
{
    cfg.set("composerGenres", v.toList());
}

void Settings::saveSingleTracksFolders(const QSet<QString> &v)
{
    cfg.set("singleTracksFolders", v.toList());
}

void Settings::saveCueSupport(MPDParseUtils::CueSupport v)
{
    cfg.set("cueSupport", MPDParseUtils::toStr(v));
}

void Settings::saveLyricProviders(const QStringList &v)
{
    cfg.set("lyricProviders", v);
}

void Settings::saveWikipediaLangs(const QStringList &v)
{
    cfg.set("wikipediaLangs", v);
}

void Settings::saveWikipediaIntroOnly(bool v)
{
    cfg.set("wikipediaIntroOnly", v);
}

void Settings::saveContextBackdrop(int v)
{
    cfg.set("contextBackdrop", v);
}

void Settings::saveContextBackdropOpacity(int v)
{
    cfg.set("contextBackdropOpacity", v);
}

void Settings::saveContextBackdropBlur(int v)
{
    cfg.set("contextBackdropBlur", v);
}

void Settings::saveContextBackdropFile(const QString &v)
{
    cfg.setFilePath("contextBackdropFile", v);
}

void Settings::saveContextDarkBackground(bool v)
{
    cfg.set("contextDarkBackground", v);
}

void Settings::saveContextZoom(int v)
{
    cfg.set("contextZoom", v);
}

void Settings::saveContextSlimPage(const QString &v)
{
    cfg.set("contextSlimPage", v);
}

void Settings::saveContextSplitterState(const QByteArray &v)
{
    cfg.set("contextSplitterState", v);
}

void Settings::saveContextAlwaysCollapsed(bool v)
{
    cfg.set("contextAlwaysCollapsed", v);
}

void Settings::saveContextSwitchTime(int v)
{
    cfg.set("contextSwitchTime", v);
}

void Settings::saveContextAutoScroll(bool v)
{
    cfg.set("contextAutoScroll", v);
}

void Settings::saveContextTrackView(int v)
{
    cfg.set("contextTrackView", v);
}

void Settings::savePage(const QString &v)
{
    cfg.set("page", v);
}

void Settings::saveHiddenPages(const QStringList &v)
{
    cfg.set("hiddenPages", v);
}

#ifdef ENABLE_DEVICES_SUPPORT
void Settings::saveOverwriteSongs(bool v)
{
    cfg.set("overwriteSongs", v);
}

void Settings::saveShowDeleteAction(bool v)
{
    cfg.set("showDeleteAction", v);
}
#endif

void Settings::saveStopFadeDuration(int v)
{
    cfg.set("stopFadeDuration", v);
}

void Settings::saveHttpAllocatedPort(int v)
{
    cfg.set("httpAllocatedPort", v);
}

void Settings::saveHttpInterface(const QString &v)
{
    cfg.set("httpInterface", v);
}

void Settings::savePlayQueueView(int v)
{
    cfg.set("playQueueView", ItemView::modeStr((ItemView::Mode)v));
}

void Settings::savePlayQueueAutoExpand(bool v)
{
    cfg.set("playQueueAutoExpand", v);
}

void Settings::savePlayQueueStartClosed(bool v)
{
    cfg.set("playQueueStartClosed", v);
}

void Settings::savePlayQueueScroll(bool v)
{
    cfg.set("playQueueScroll", v);
}

void Settings::savePlayQueueBackground(int v)
{
    cfg.set("playQueueBackground", v);
}

void Settings::savePlayQueueBackgroundOpacity(int v)
{
    cfg.set("playQueueBackgroundOpacity", v);
}

void Settings::savePlayQueueBackgroundBlur(int v)
{
    cfg.set("playQueueBackgroundBlur", v);
}

void Settings::savePlayQueueBackgroundFile(const QString &v)
{
    cfg.setFilePath("playQueueBackgroundFile", v);
}

void Settings::savePlayQueueConfirmClear(bool v)
{
    cfg.set("playQueueConfirmClear", v);
}

void Settings::savePlayQueueSearch(bool v)
{
    cfg.set("playQueueSearch", v);
}

void Settings::savePlayListsStartClosed(bool v)
{
    cfg.set("playListsStartClosed", v);
}

#ifdef ENABLE_HTTP_STREAM_PLAYBACK
void Settings::savePlayStream(bool v)
{
    cfg.set("playStream", v);
}
#endif

#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
void Settings::saveCdAuto(bool v)
{
    cfg.set("cdAuto", v);
}

void Settings::saveParanoiaFull(bool v)
{
    cfg.set("paranoiaFull", v);
}

void Settings::saveParanoiaNeverSkip(bool v)
{
    cfg.set("paranoiaNeverSkip", v);
}

void Settings::saveParanoiaOffset(int v)
{
    cfg.set("paranoiaOffset", v);
}
#endif

#if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
void Settings::saveUseCddb(bool v)
{
    cfg.set("useCddb", v);
}
#endif

#ifdef CDDB_FOUND
void Settings::saveCddbHost(const QString &v)
{
    cfg.set("cddbHost", v);
}

void Settings::saveCddbPort(int v)
{
    cfg.set("cddbPort", v);
}
#endif

void Settings::saveForceSingleClick(bool v)
{
    cfg.set("forceSingleClick", v);
}

void Settings::saveStartHidden(bool v)
{
    cfg.set("startHidden", v);
}

void Settings::saveShowTimeRemaining(bool v)
{
    cfg.set("showTimeRemaining", v);
}

void Settings::saveHiddenStreamCategories(const QStringList &v)
{
    cfg.set("hiddenStreamCategories", v);
}

void Settings::saveHiddenOnlineProviders(const QStringList &v)
{
    cfg.set("hiddenOnlineProviders", v);
}

#if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
void Settings::saveInhibitSuspend(bool v)
{
    cfg.set("inhibitSuspend", v);
}
#endif

void Settings::saveRssUpdate(int v)
{
    cfg.set("rssUpdate", v);
}

void Settings::saveLastRssUpdate(const QDateTime &v)
{
    cfg.set("lastRssUpdate", v);
}

void Settings::savePodcastDownloadPath(const QString &v)
{
    cfg.set("podcastDownloadPath", v);
}

void Settings::savePodcastAutoDownloadLimit(int v)
{
    if (cfg.hasEntry("podcastAutoDownload")) {
        cfg.removeEntry("podcastAutoDownload");
    }
    cfg.set("podcastAutoDownloadLimit", v);
}

void Settings::saveVolumeStep(int v)
{
    cfg.set("volumeStep", v);
}

void Settings::saveStartupState(int v)
{
    cfg.set("startupState", getStartupStateStr((StartupState)v));
}

void Settings::saveSearchCategory(const QString &v)
{
    cfg.set("searchCategory", v);
}

void Settings::saveFetchCovers(bool v)
{
    cfg.set("fetchCovers", v);
}

void Settings::saveLang(const QString &v)
{
    cfg.set("lang", v);
}

void Settings::saveShowCoverWidget(bool v)
{
    cfg.set("showCoverWidget", v);
}

void Settings::saveShowStopButton(bool v)
{
    cfg.set("showStopButton", v);
}

void Settings::saveShowRatingWidget(bool v)
{
    cfg.set("showRatingWidget", v);
}

void Settings::saveShowTechnicalInfo(bool v)
{
    cfg.set("showTechnicalInfo", v);
}

void Settings::saveInfoTooltips(bool v)
{
    cfg.set("infoTooltips", v);
}

void Settings::saveIgnorePrefixes(const QSet<QString> &v)
{
    cfg.set("ignorePrefixes", v.toList());
}

void Settings::saveMpris(bool v)
{
    cfg.set("mpris", v);
}

void Settings::saveReplayGain(const QString &conn, const QString &v)
{
    QString n=MPDConnectionDetails::configGroupName(conn);
    Configuration grp(n);
    grp.set("replayGain", v);
}

void Settings::saveStyle(const QString &v)
{
    cfg.set("style", v);
}

#if !defined Q_OS_WIN && !defined Q_OS_MAC
void Settings::saveShowMenubar(bool v)
{
    cfg.set("showMenubar", v);
}
#endif

void Settings::saveUseOriginalYear(bool v)
{
    cfg.set("useOriginalYear", v);
}

void Settings::saveResponsiveSidebar(bool v)
{
    cfg.set("responsiveSidebar", v);
}

void Settings::save()
{
    if (AP_NotConfigured!=state) {
        if (version()!=PACKAGE_VERSION || AP_FirstRun==state) {
            cfg.set("version", PACKAGE_VERSION_STRING);
            ver=PACKAGE_VERSION;
        }
    }
    cfg.sync();
}

void Settings::clearVersion()
{
    cfg.removeEntry("version");
    state=AP_NotConfigured;
}

int Settings::getBoolAsInt(const QString &key, int def)
{
    // Old config, sometimes bool was used - which has now been converted
    // to an int...
    QString v=cfg.get(key, QString::number(def));
    if (QLatin1String("false")==v) {
        return 0;
    }
    if (QLatin1String("true")==v) {
        return 1;
    }
    return v.toInt();
}
