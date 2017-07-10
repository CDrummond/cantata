/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <qglobal.h>
#include <QMainWindow>
#include <QToolButton>
#include <QStringList>
#include "ui_mainwindow.h"
#include "models/playqueueproxymodel.h"
#include "models/playqueuemodel.h"
#include "mpd-interface/song.h"
#include "mpd-interface/mpdstatus.h"
#include "mpd-interface/mpdconnection.h"
#include "config.h"

class Action;
class ActionCollection;
class MainWindow;
class Page;
class LibraryPage;
class FolderPage;
class PlaylistsPage;
class OnlineServicesPage;
#ifdef ENABLE_DEVICES_SUPPORT
class DevicesPage;
#endif
class SearchPage;
class QAbstractItemView;
#ifdef QT_QTDBUS_FOUND
class Mpris;
#endif
class QTimer;
class QPropertyAnimation;
class QActionGroup;
class QDateTime;
class QMenu;
class TrayItem;
class HttpStream;
class MPDStatus;
struct MPDConnectionDetails;
struct Output;
#if defined Q_OS_WIN
class ThumbnailToolBar;
#endif
#ifdef Q_OS_MAC
class DockMenu;
#endif

// Dummy classes so that when class name is saved to the config file, we get a more meaningful name than QWidget!!!
class PlayQueuePage : public QWidget
{
    Q_OBJECT
public:
    PlayQueuePage(QWidget *p) : QWidget(p) { }
};
class ContextPage : public QWidget
{
    Q_OBJECT
public:
    ContextPage(QWidget *p) : QWidget(p) { }
};

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

    Q_PROPERTY( QStringList listActions READ listActions )

public:
    enum Pages
    {
        PAGE_PLAYQUEUE,
        PAGE_LIBRARY,
        PAGE_FOLDERS,
        PAGE_PLAYLISTS,
        PAGE_ONLINE,
        #ifdef ENABLE_DEVICES_SUPPORT
        PAGE_DEVICES,
        #endif
        PAGE_SEARCH,
        PAGE_CONTEXT
    };

    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int mpdVolume() const { return volumeSlider->value(); }
    QList<Song> selectedSongs() const;
    QStringList listActions() const;

protected:
    void keyPressEvent(QKeyEvent *event);
    #if defined Q_OS_WIN
    void showEvent(QShowEvent *event);
    #endif
    void closeEvent(QCloseEvent *event);

private:
    void addMenuAction(QMenu *menu, QAction *action);
    void setupTrayIcon();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void setDetails(const MPDConnectionDetails &det);
    void pause(bool p);
    void play();
    void stop(bool afterCurrent=false);
    void terminating();
    void getStatus();
    void playListInfo();
    void currentSong();
    void setSeekId(qint32, quint32);
    void startPlayingSongId(qint32);
    void setVolume(int);
    void outputs();
    void enableOutput(int id, bool);
    void setPriority(const QList<qint32> &ids, quint8 priority);
    void addSongsToPlaylist(const QString &name, const QStringList &files);
    void showPreferencesPage(const QString &page);
    void playNext(const QList<quint32> &items, quint32 pos, quint32 size);

public Q_SLOTS:
    void showError(const QString &message, bool showActions=false);
    void showInformation(const QString &message);
    void dynamicStatus(const QString &message);
    void setCollection(const QString &collection);
    void mpdConnectionName(const QString &name);
    void hideWindow();
    void restoreWindow();
    void load(const QStringList &urls) { PlayQueueModel::self()->load(urls); }
    void showAboutDialog();
    void mpdConnectionStateChanged(bool connected);
    void playQueueItemsSelected(bool s);
    void showPreferencesDialog(const QString &page=QString());
    void quit();
    void updateSettings();
    void toggleOutput();
    void changeConnection();
    void connectToMpd();
    void connectToMpd(const MPDConnectionDetails &details);
    void streamUrl(const QString &u);
    void refreshDbPromp();
    void showServerInfo();
    void stopPlayback();
    void stopAfterCurrentTrack();
    void stopAfterTrack();
    void playPauseTrack();
    void setPosition();
    void searchPlayQueue();
    void realSearchPlayQueue();
    void playQueueSearchActivated(bool a);
    void updatePlayQueue(const QList<Song> &songs, bool isComplete);
    void updateCurrentSong(Song song, bool wasEmpty=false);
    void scrollPlayQueue(bool wasEmpty=false);
    void updateStatus();
    void playQueueItemActivated(const QModelIndex &);
    void clearPlayQueue();
    void centerPlayQueue();
    void removeFromPlayQueue() { PlayQueueModel::self()->remove(playQueueProxyModel.mapToSourceRows(playQueue->selectedIndexes())); }
    void replacePlayQueue() { appendToPlayQueue(MPDConnection::ReplaceAndplay); }
    void appendToPlayQueue() { appendToPlayQueue(MPDConnection::Append); }
    void appendToPlayQueueAndPlay() { appendToPlayQueue(MPDConnection::AppendAndPlay); }
    void addToPlayQueueAndPlay() { appendToPlayQueue(MPDConnection::AddAndPlay); }
    void insertIntoPlayQueue() { appendToPlayQueue(MPDConnection::AddAfterCurrent); }
    void addWithPriority();
    void addToNewStoredPlaylist();
    void addToExistingStoredPlaylist(const QString &name) { addToExistingStoredPlaylist(name, playQueue->hasFocus()); }
    void addToExistingStoredPlaylist(const QString &name, bool pq);
    void addStreamToPlayQueue();
    void removeItems();
    void checkMpdAccessibility();
    void cropPlayQueue() { PlayQueueModel::self()->crop(playQueueProxyModel.mapToSourceRows(playQueue->selectedIndexes())); }
    void updatePlayQueueStats(int songs, quint32 time);
    void expandOrCollapse(bool saveCurrentSize=true);
    void showSongInfo();
    void fullScreen();
    void sidebarModeChanged();
    void currentTabChanged(int index);
    void tabToggled(int index);
    void showPlayQueue() { showTab(PAGE_PLAYQUEUE); }
    void showLibraryTab() { showTab(PAGE_LIBRARY); }
    void showFoldersTab() { showTab(PAGE_FOLDERS); }
    void showPlaylistsTab() { showTab(PAGE_PLAYLISTS); }
    void showOnlineTab() { showTab(PAGE_ONLINE); }
    void showContextTab() { showTab(PAGE_CONTEXT); }
    void showDevicesTab() {
        #ifdef ENABLE_DEVICES_SUPPORT
        showTab(PAGE_DEVICES);
        #endif
    }
    void showSearchTab() { showTab(PAGE_SEARCH); }
    void toggleSplitterAutoHide();
    void locateTracks(const QList<Song> &songs);
    void locateTrack() { locateTracks(playQueue->selectedSongs()); }
    void moveSelectionAfterCurrentSong();
    void locateArtist(const QString &artist);
    void locateAlbum(const QString &artist, const QString &album);
    void editTags();
    void organiseFiles();
    void addToDevice(const QString &udi);
    void deleteSongs();
    void copyToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);
    void replayGain();
    void setCover();
    void showPlayQueueSearch();
    void showSearch();
    void expandAll();
    void collapseAll();
    void checkMpdDir();
    void outputsUpdated(const QList<Output> &outputs);
    void updateConnectionsMenu();
    void controlConnectionsMenu(bool enable=true);
    void controlDynamicButton();
    void setRating();
    void triggerAction(const QString &name);

private:
    bool canClose();
    void expand();
    bool canShowDialog();
    void enableStopActions(bool enable);
    void updateStatus(MPDStatus * const status);
    void readSettings();
    void appendToPlayQueue(int action, quint8 priority=0);
    bool currentIsStream() const { return PlayQueueModel::self()->rowCount() && -1!=current.id && current.isStream(); }
    void updateWindowTitle();
    void showTab(int page) { tabWidget->setCurrentIndex(page); }
    void updateNextTrack(int nextTrackId);
    void updateActionToolTips();
    void startContextTimer();
    int calcMinHeight();
    int calcCollapsedSize();
    void setCollapsedSize();

private Q_SLOTS:
    void init();
    void toggleContext();
    void toggleMenubar();
    void initMpris();

private:
    int prevPage;
    MPDState lastState;
    qint32 lastSongId;
    PlayQueueProxyModel playQueueProxyModel;
    bool autoScrollPlayQueue;
    Action *showMenuAction;
    Action *prefAction;
    Action *refreshDbAction;
    Action *doDbRefreshAction;
    Action *connectAction;
    Action *connectionsAction;
    Action *outputsAction;
    QActionGroup *connectionsGroup;
    Action *stopAfterTrackAction;
    Action *addPlayQueueToStoredPlaylistAction;
    Action *clearPlayQueueAction;
    Action *centerPlayQueueAction;
    Action *expandInterfaceAction;
    Action *cropPlayQueueAction;
    Action *addStreamToPlayQueueAction;
    Action *randomPlayQueueAction;
    Action *repeatPlayQueueAction;
    Action *singlePlayQueueAction;
    Action *consumePlayQueueAction;
    Action *searchPlayQueueAction;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    HttpStream *httpStream;
    Action *streamPlayAction;
    #endif
    Action *songInfoAction;
    Action *fullScreenAction;
    Action *quitAction;
    Action *restoreAction;
    Action *locateTrackAction;
    Action *playNextAction;
    #ifdef TAGLIB_FOUND
    Action *editPlayQueueTagsAction;
    #endif
    Action *searchTabAction;
    Action *expandAllAction;
    Action *collapseAllAction;
    Action *serverInfoAction;
    Action *cancelAction;
    Action *ratingAction;
    TrayItem *trayItem;
    QPoint lastPos;
    QSize expandedSize;
    QSize collapsedSize;
    Song current;
    Page *currentPage;
    Action *showPlayQueueAction;
    QWidget *playQueuePage;
    Action *libraryTabAction;
    LibraryPage *libraryPage;
    Action *foldersTabAction;
    FolderPage *folderPage;
    Action *playlistsTabAction;
    PlaylistsPage *playlistsPage;
    Action *onlineTabAction;
    OnlineServicesPage *onlinePage;
    QWidget *contextPage;
    #ifdef ENABLE_DEVICES_SUPPORT
    Action *devicesTabAction;
    Action *copyToDeviceAction;
    DevicesPage *devicesPage;
    #endif
    SearchPage *searchPage;
    #ifdef QT_QTDBUS_FOUND
    Mpris *mpris;
    #endif
    QTimer *statusTimer;
    QTimer *playQueueSearchTimer;
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    QTimer *mpdAccessibilityTimer;
    #endif
    QTimer *contextTimer;
    int contextSwitchTime;
    enum { CS_Init, CS_Connected, CS_Disconnected } connectedState;
    bool stopAfterCurrent;
    #if defined Q_OS_WIN
    ThumbnailToolBar *thumbnailTooolbar;
    #endif
    #ifdef Q_OS_MAC
    DockMenu *dockMenu;
    #endif
    friend class TrayItem;
};

#endif
