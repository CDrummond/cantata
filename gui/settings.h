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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "support/configuration.h"
#include "config.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"

class Settings
{
public:
    enum StartupState
    {
        SS_ShowMainWindow,
        SS_HideMainWindow,
        SS_Previous
    };

    static Settings *self();

    Settings();
    ~Settings();

    QString currentConnection();
    MPDConnectionDetails connectionDetails(const QString &name=Settings::self()->currentConnection());
    QList<MPDConnectionDetails> allConnections();
    bool showPlaylist();
    bool showFullScreen();
    QByteArray headerState(const QString &key);
    QByteArray splitterState();
    bool splitterAutoHide();
    QSize mainWindowSize();
    QSize mainWindowCollapsedSize();
    bool maximized();
    bool useSystemTray();
    bool minimiseOnClose();
    bool showPopups();
    bool stopOnExit();
    bool storeCoversInMpdDir();
    bool storeLyricsInMpdDir();
    QString coverFilename();
    int sidebar();
    QSet<QString> composerGenres();
    QSet<QString> singleTracksFolders();
    MPDParseUtils::CueSupport cueSupport();
    QStringList lyricProviders();
    QStringList wikipediaLangs();
    bool wikipediaIntroOnly();
    int contextBackdrop();
    int contextBackdropOpacity();
    int contextBackdropBlur();
    QString contextBackdropFile();
    bool contextDarkBackground();
    int contextZoom();
    QString contextSlimPage();
    QByteArray contextSplitterState();
    bool contextAlwaysCollapsed();
    int contextSwitchTime();
    bool contextAutoScroll();
    int contextTrackView();
    QString page();
    QStringList hiddenPages();
    #ifdef ENABLE_DEVICES_SUPPORT
    bool overwriteSongs();
    bool showDeleteAction();
    #endif
    int version();
    int stopFadeDuration();
    int httpAllocatedPort();
    QString httpInterface();
    int playQueueView();
    bool playQueueAutoExpand();
    bool playQueueStartClosed();
    bool playQueueScroll();
    int playQueueBackground();
    int playQueueBackgroundOpacity();
    int playQueueBackgroundBlur();
    QString playQueueBackgroundFile();
    bool playQueueConfirmClear();
    bool playQueueSearch();
    bool playListsStartClosed();
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool playStream();
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    bool cdAuto();
    bool paranoiaFull();
    bool paranoiaNeverSkip();
    int paranoiaOffset();
    #endif
    #if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
    bool useCddb();
    #endif
    #ifdef CDDB_FOUND
    QString cddbHost();
    int cddbPort();
    #endif
    bool forceSingleClick();
    bool startHidden();
    bool showTimeRemaining();
    QStringList hiddenStreamCategories();
    QStringList hiddenOnlineProviders();
    #if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
    bool inhibitSuspend();
    #endif
    int rssUpdate();
    QDateTime lastRssUpdate();
    QString podcastDownloadPath();
    int podcastAutoDownloadLimit();
    int volumeStep();
    StartupState startupState();
    QString searchCategory();
    bool fetchCovers();
    QString lang();
    bool showCoverWidget();
    bool showStopButton();
    bool showRatingWidget();
    bool showTechnicalInfo();
    bool infoTooltips();
    QSet<QString> ignorePrefixes();
    bool mpris();
    QString style();
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    bool showMenubar();
    #endif
    bool useOriginalYear();
    bool responsiveSidebar();

    void removeConnectionDetails(const QString &v);
    void saveConnectionDetails(const MPDConnectionDetails &v);
    void saveCurrentConnection(const QString &v);
    void saveShowPlaylist(bool v);
    void saveShowFullScreen(bool v);
    void saveStopOnExit(bool v);
    void saveHeaderState(const QString &key, const QByteArray &v);
    void saveSplitterState(const QByteArray &v);
    void saveSplitterAutoHide(bool v);
    void saveMainWindowSize(const QSize &v);
    void saveMainWindowCollapsedSize(const QSize &v);
    void saveMaximized(bool v);
    void saveUseSystemTray(bool v);
    void saveMinimiseOnClose(bool v);
    void saveShowPopups(bool v);
    void saveStoreCoversInMpdDir(bool v);
    void saveStoreLyricsInMpdDir(bool v);
    void saveCoverFilename(const QString &v);
    void saveLibraryArtistImage(bool v);
    void saveSidebar(int v);
    void saveComposerGenres(const QSet<QString> &v);
    void saveSingleTracksFolders(const QSet<QString> &v);
    void saveCueSupport(MPDParseUtils::CueSupport v);
    void saveLyricProviders(const QStringList &v);
    void saveWikipediaLangs(const QStringList &v);
    void saveWikipediaIntroOnly(bool v);
    void saveContextBackdrop(int v);
    void saveContextBackdropOpacity(int v);
    void saveContextBackdropBlur(int v);
    void saveContextBackdropFile(const QString &v);
    void saveContextDarkBackground(bool v);
    void saveContextZoom(int v);
    void saveContextSlimPage(const QString &v);
    void saveContextSplitterState(const QByteArray &v);
    void saveContextAlwaysCollapsed(bool v);
    void saveContextSwitchTime(int v);
    void saveContextAutoScroll(bool v);
    void saveContextTrackView(int v);
    void savePage(const QString &v);
    void saveHiddenPages(const QStringList &v);
    #ifdef ENABLE_DEVICES_SUPPORT
    void saveOverwriteSongs(bool v);
    void saveShowDeleteAction(bool v);
    #endif
    void saveStopFadeDuration(int v);
    void saveHttpAllocatedPort(int v);
    void saveHttpInterface(const QString &v);
    void savePlayQueueView(int v);
    void savePlayQueueAutoExpand(bool v);
    void savePlayQueueStartClosed(bool v);
    void savePlayQueueScroll(bool v);
    void savePlayQueueBackground(int v);
    void savePlayQueueBackgroundOpacity(int v);
    void savePlayQueueBackgroundBlur(int v);
    void savePlayQueueBackgroundFile(const QString &v);
    void savePlayQueueConfirmClear(bool v);
    void savePlayQueueSearch(bool v);
    void savePlayListsStartClosed(bool v);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    void savePlayStream(bool v);
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    void saveCdAuto(bool v);
    void saveParanoiaFull(bool v);
    void saveParanoiaNeverSkip(bool v);
    void saveParanoiaOffset(int v);
    #endif
    #if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
    void saveUseCddb(bool v);
    #endif
    #ifdef CDDB_FOUND
    void saveCddbHost(const QString &v);
    void saveCddbPort(int v);
    #endif
    void saveForceSingleClick(bool v);
    void saveStartHidden(bool v);
    void saveShowTimeRemaining(bool v);
    void saveHiddenStreamCategories(const QStringList &v);
    void saveHiddenOnlineProviders(const QStringList &v);
    #if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
    void saveInhibitSuspend(bool v);
    #endif
    void saveRssUpdate(int v);
    void saveLastRssUpdate(const QDateTime &v);
    void savePodcastDownloadPath(const QString &v);
    void savePodcastAutoDownloadLimit(int v);
    void saveVolumeStep(int v);
    void saveStartupState(int v);
    void saveSearchCategory(const QString &v);
    void saveFetchCovers(bool v);
    void saveLang(const QString &v);
    void saveShowCoverWidget(bool v);
    void saveShowStopButton(bool v);
    void saveShowRatingWidget(bool v);
    void saveShowTechnicalInfo(bool v);
    void saveInfoTooltips(bool v);
    void saveIgnorePrefixes(const QSet<QString> &v);
    void saveMpris(bool v);
    void saveReplayGain(const QString &conn, const QString &v);
    void saveStyle(const QString &v);
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    void saveShowMenubar(bool v);
    #endif
    void saveUseOriginalYear(bool v);
    void saveResponsiveSidebar(bool v);
    void save();
    void clearVersion();

    bool firstRun() const { return AP_Configured!=state; }

private:
    int getBoolAsInt(const QString &key, int def);

private:
    enum AppState {
        AP_FirstRun,
        AP_NotConfigured,
        AP_Configured
    };

    AppState state;
    int ver;
    Configuration cfg;
};

#endif
