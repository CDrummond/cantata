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

#ifndef SETTINGS_H
#define SETTINGS_H

#include "support/configuration.h"
#include "config.h"
#include "mpd/mpdconnection.h"
#if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
namespace KWallet {
class Wallet;
}
#endif

class Settings
{
public:
    enum Constants
    {
        MinFade     = 400,
        MaxFade     = 4000,
        DefaultFade = 2000
    };

    enum StartupState
    {
        SS_ShowMainWindow,
        SS_HideMainWindow,
        SS_Previous
    };

    enum MenuControl
    {
        MC_Bar = 0x01,
        MC_Button = 0x02
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
    bool useSystemTray();
    bool minimiseOnClose();
    bool showPopups();
    bool stopOnExit();
    bool storeCoversInMpdDir();
    bool storeLyricsInMpdDir();
    bool storeStreamsInMpdDir();
    bool storeBackdropsInMpdDir();
    #ifndef ENABLE_UBUNTU
    int libraryView();
    int albumsView();
    int folderView();
    int playlistsView();
    int streamsView();
    int onlineView();
    #endif
    int albumSort();
    int sidebar();
    bool libraryYear();
    bool groupSingle();
    bool useComposer();
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
    #if !defined ENABLE_KDE_SUPPORT && !defined ENABLE_UBUNTU
    QString mediaKeysIface();
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    bool overwriteSongs();
    bool showDeleteAction();
    int devicesView();
    #endif
    #ifndef ENABLE_UBUNTU
    int searchView();
    #endif
    int version();
    int stopFadeDuration();
    int httpAllocatedPort();
    QString httpInterface();
    bool alwaysUseHttp();
    int playQueueView();
    bool playQueueAutoExpand();
    bool playQueueStartClosed();
    bool playQueueScroll();
    int playQueueBackground();
    int playQueueBackgroundOpacity();
    int playQueueBackgroundBlur();
    QString playQueueBackgroundFile();
    bool playQueueConfirmClear();
    bool playListsStartClosed();
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool playStream();
    bool stopHttpStreamOnPause();
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    bool cdAuto();
    bool paranoiaFull();
    bool paranoiaNeverSkip();
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
    bool monoSidebarIcons();
    bool showTimeRemaining();
    QStringList hiddenStreamCategories();
    QStringList hiddenOnlineProviders();
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    bool inhibitSuspend();
    #endif
    int rssUpdate();
    QDateTime lastRssUpdate();
    QString podcastDownloadPath();
    bool podcastAutoDownload();
    int maxCoverUpdatePerIteration();
    int coverCacheSize();
    QStringList cueFileCodecs();
    bool networkAccessEnabled();
    int volumeStep();
    StartupState startupState();
    int undoSteps();
    QString searchCategory();
    bool cacheScaledCovers();
    bool fetchCovers();
    int mpdPoll();
    int mpdListSize();
    #ifndef ENABLE_KDE_SUPPORT
    QString lang();
    #endif
    bool alwaysUseLsInfo();
    bool showMenubar();
    int menu();
    bool touchFriendly();
    bool filteredOnly();
    bool showCoverWidget();
    bool showStopButton();
    bool showRatingWidget();

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
    void saveUseSystemTray(bool v);
    void saveMinimiseOnClose(bool v);
    void saveShowPopups(bool v);
    void saveStoreCoversInMpdDir(bool v);
    void saveStoreLyricsInMpdDir(bool v);
    void saveStoreBackdropsInMpdDir(bool v);
    #ifndef ENABLE_UBUNTU
    void saveLibraryView(int v);
    void saveAlbumsView(int v);
    void saveFolderView(int v);
    void savePlaylistsView(int v);
    void saveStreamsView(int v);
    void saveOnlineView(int v);
    #endif
    void saveAlbumSort(int v);
    void saveSidebar(int v);
    void saveLibraryYear(bool v);
    void saveGroupSingle(bool v);
    void saveUseComposer(bool v);
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
    #if !defined ENABLE_KDE_SUPPORT && !defined ENABLE_UBUNTU
    void saveMediaKeysIface(const QString &v);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    void saveOverwriteSongs(bool v);
    void saveShowDeleteAction(bool v);
    void saveDevicesView(int v);
    #endif
    #ifndef ENABLE_UBUNTU
    void saveSearchView(int v);
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
    void savePlayListsStartClosed(bool v);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    void savePlayStream(bool v);
    #endif
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    void saveCdAuto(bool v);
    void saveParanoiaFull(bool v);
    void saveParanoiaNeverSkip(bool v);
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
    void saveMonoSidebarIcons(bool v);
    void saveShowTimeRemaining(bool v);
    void saveHiddenStreamCategories(const QStringList &v);
    void saveHiddenOnlineProviders(const QStringList &v);
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    void saveInhibitSuspend(bool v);
    #endif
    void saveRssUpdate(int v);
    void saveLastRssUpdate(const QDateTime &v);
    void savePodcastDownloadPath(const QString &v);
    void savePodcastAutoDownload(bool v);
    void saveStartupState(int v);
    void saveSearchCategory(const QString &v);
    void saveFetchCovers(bool v);
    #ifndef ENABLE_KDE_SUPPORT
    void saveLang(const QString &v);
    #endif
    void saveShowMenubar(bool v);
    void saveTouchFriendly(bool v);
    void saveFilteredOnly(bool v);
    void saveShowCoverWidget(bool v);
    void saveShowStopButton(bool v);
    void saveShowRatingWidget(bool v);
    void save();
    #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
    bool openWallet();
    #else
    QString iconTheme();
    #endif
    int id3v2Version();

    bool firstRun() const { return isFirstRun; }

private:
    bool isFirstRun;
    int ver;
    #if defined ENABLE_KDE_SUPPORT && defined ENABLE_KWALLET
    KWallet::Wallet *wallet;
    #endif
    Configuration cfg;
};

#endif
