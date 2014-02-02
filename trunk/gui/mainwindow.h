/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KXmlGuiWindow>
#else
#include <QMainWindow>
#endif
#include <QMenu>
#include <QProxyStyle>
#include <QPixmap>
#include <QMoveEvent>
#include <QToolButton>
#include <QStringList>
#include "ui_mainwindow.h"
#include "playqueuemodel.h"
#include "playqueueproxymodel.h"
#include "mpdstatus.h"
#include "mpdconnection.h"
#include "song.h"
#include "config.h"
#ifdef ENABLE_HTTP_STREAM_PLAYBACK
#if QT_VERSION < 0x050000
#include <phonon/mediaobject.h>
#else
#include <QtMultimedia/QMediaPlayer>
#endif
#endif

class Action;
class ActionCollection;
class MainWindow;
class LibraryPage;
class AlbumsPage;
class FolderPage;
class PlaylistsPage;
#ifdef ENABLE_DYNAMIC
class DynamicPage;
#endif
#ifdef ENABLE_STREAMS
class StreamsPage;
#endif
#ifdef ENABLE_ONLINE_SERVICES
class OnlineServicesPage;
#endif
#ifdef ENABLE_DEVICES_SUPPORT
class DevicesPage;
#endif
class SearchPage;
class QAbstractItemView;
#ifndef Q_OS_WIN
class Mpris;
#endif // !defined Q_OS_WIN
class QTimer;
class QPropertyAnimation;
class QActionGroup;
class QDateTime;
class TrayItem;
class GtkProxyStyle;

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

class DeleteKeyEventHandler : public QObject
{
public:
    DeleteKeyEventHandler(QAbstractItemView *v, QAction *a) : QObject(v), view(v), act(a) { }
protected:
    bool eventFilter(QObject *obj, QEvent *event);
private:
    QAbstractItemView *view;
    QAction *act;
};

#ifdef ENABLE_KDE_SUPPORT
#define MAIN_WINDOW_BASE_CLASS KXmlGuiWindow
#else
#define MAIN_WINDOW_BASE_CLASS QMainWindow
#endif

class MainWindow : public MAIN_WINDOW_BASE_CLASS, private Ui::MainWindow
{
    Q_OBJECT

public:
    enum Page
    {
        PAGE_PLAYQUEUE,
        PAGE_LIBRARY,
        PAGE_ALBUMS,
        PAGE_FOLDERS,
        PAGE_PLAYLISTS,
        #ifdef ENABLE_DYNAMIC
        PAGE_DYNAMIC,
        #endif
        #ifdef ENABLE_STREAMS
        PAGE_STREAMS,
        #endif
        #ifdef ENABLE_ONLINE_SERVICES
        PAGE_ONLINE,
        #endif
        #ifdef ENABLE_DEVICES_SUPPORT
        PAGE_DEVICES,
        #endif
        PAGE_SEARCH,
        PAGE_CONTEXT
    };

    Q_PROPERTY(int volume READ mpdVolume WRITE setMpdVolume)

    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int mpdVolume() const;
    int currentTrackPosition() const;
    QString coverFile() const;

protected:
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    void initSizes();
    void setupTrayIcon();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void setDetails(const MPDConnectionDetails &det);
    void pause(bool p);
    void play();
    void stop(bool afterCurrent=false);
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

public Q_SLOTS:
    void showError(const QString &message, bool showActions=false);
    void showInformation(const QString &message);
    void showPage(const QString &page, bool showSearch);
    void dynamicStatus(const QString &message);
    void hideWindow();
    void restoreWindow();
    void load(const QStringList &urls);
    #ifdef ENABLE_KDE_SUPPORT
    void configureShortcuts();
    void saveShortcuts();
    #endif
    void setMpdVolume(int v);
    void songLoaded();
    void messageWidgetVisibility(bool v);
    void mpdConnectionStateChanged(bool connected);
    void playQueueItemsSelected(bool s);
    void showSidebarPreferencesPage();
    void showPreferencesDialog(const QString &page=QString());
    void quit();
    void updateSettings();
    void toggleOutput();
    void changeConnection();
    void connectToMpd();
    void connectToMpd(const MPDConnectionDetails &details);
    void streamUrl(const QString &u);
    void refreshDbPromp();
    #ifndef ENABLE_KDE_SUPPORT
    void showAboutDialog();
    #endif
    void showServerInfo();
    void toggleStream(bool s, const QString &url=QString());
    void stopPlayback();
    void stopAfterCurrentTrack();
    void stopAfterTrack();
    void playPauseTrack();
    void setPosition();
    void searchPlayQueue();
    void realSearchPlayQueue();
    void playQueueSearchActivated(bool a);
    void updatePlayQueue(const QList<Song> &songs);
    void updateCurrentSong(const Song &song);
    void scrollPlayQueue();
    void updateStats();
    void updateStatus();
    void playQueueItemActivated(const QModelIndex &);
    void promptClearPlayQueue();
    void clearPlayQueue();
    void removeFromPlayQueue();
    void replacePlayQueue();
    void addToPlayQueue();
    void addRandomToPlayQueue();
    void addWithPriority();
    void addToNewStoredPlaylist();
    void addToExistingStoredPlaylist(const QString &name);
    void addToExistingStoredPlaylist(const QString &name, bool pq);
    void addStreamToPlayQueue();
    void removeItems();
    void checkMpdAccessibility();
    void cropPlayQueue();
    void updatePlayQueueStats(int songs, quint32 time);
    void copyTrackInfo();
    void expandOrCollapse(bool saveCurrentSize=true);
    void showSongInfo();
    void fullScreen();
    void sidebarModeChanged();
    void currentTabChanged(int index);
    void tabToggled(int index);
    void showPlayQueue() { showTab(PAGE_PLAYQUEUE); }
    void showLibraryTab() { showTab(PAGE_LIBRARY); }
    void showAlbumsTab() { showTab(PAGE_ALBUMS); }
    void showFoldersTab() { showTab(PAGE_FOLDERS); }
    void showPlaylistsTab() { showTab(PAGE_PLAYLISTS); }
    void showDynamicTab() {
        #ifdef ENABLE_DYNAMIC
        showTab(PAGE_DYNAMIC);
        #endif
    }
    void showStreamsTab() {
        #ifdef ENABLE_STREAMS
        showTab(PAGE_STREAMS);
        #endif
    }
    void showOnlineTab() {
        #ifdef ENABLE_ONLINE_SERVICES
        showTab(PAGE_ONLINE); 
        #endif
    }
    void showContextTab() { showTab(PAGE_CONTEXT); }
    void showDevicesTab() {
        #ifdef ENABLE_DEVICES_SUPPORT
        showTab(PAGE_DEVICES);
        #endif
    }
    void showSearchTab() { showTab(PAGE_SEARCH); }
    void toggleSplitterAutoHide();
    void toggleMonoIcons();
    void locateTrack();
    void locateArtist(const QString &artist);
    void locateAlbum(const QString &artist, const QString &album);
    void editTags();
    void editPlayQueueTags();
    void organiseFiles();
    void addToDevice(const QString &udi);
    void deleteSongs();
    void copyToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);
    void replayGain();
    void setCover();
    void goBack();
    void showSearch();
    void expandAll();
    void collapseAll();
    void checkMpdDir();
    void outputsUpdated(const QList<Output> &outputs);
    void updateConnectionsMenu();
    void controlConnectionsMenu(bool enable=true);
    void controlDynamicButton();

private:
    void expand();
    bool canShowDialog();
    void enableStopActions(bool enable);
    void updateStatus(MPDStatus * const status);
    void readSettings();
    int calcMinHeight();
    int calcCompactHeight();
    void addToPlayQueue(bool replace, quint8 priority=0, bool randomAlbums=false);
    #ifdef TAGLIB_FOUND
    void editTags(const QList<Song> &songs, bool isPlayQueue);
    #endif
    bool currentIsStream() const;
    void updateWindowTitle();
    void startVolumeFade(/*bool stop*/);
    void stopVolumeFade();
    void showTab(int page);
    bool fadeWhenStop() const;
    void updateNextTrack(int nextTrackId);
    void updateActionToolTips();
    void setPlaylistsEnabled(bool e);
    void controlPlaylistActions();

private:
    int loaded;
    MPDState lastState;
    qint32 lastSongId;
    PlayQueueModel playQueueModel;
    PlayQueueProxyModel playQueueProxyModel;
    bool autoScrollPlayQueue;
    Action *prefAction;
    Action *refreshDbAction;
    Action *doDbRefreshAction;
    #ifdef ENABLE_KDE_SUPPORT
    Action *shortcutsAction;
    #endif
    Action *connectAction;
    Action *connectionsAction;
    Action *outputsAction;
    QActionGroup *connectionsGroup;
    Action *stopAfterTrackAction;
    Action *removeFromPlayQueueAction;
    Action *addPlayQueueToStoredPlaylistAction;
    Action *promptClearPlayQueueAction;
    Action *copyTrackInfoAction;
    Action *cropPlayQueueAction;
    Action *shufflePlayQueueAction;
    Action *shufflePlayQueueAlbumsAction;
    Action *addStreamToPlayQueueAction;
    Action *randomPlayQueueAction;
    Action *repeatPlayQueueAction;
    Action *singlePlayQueueAction;
    Action *consumePlayQueueAction;
    Action *searchPlayQueueAction;
    Action *setPriorityAction;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    Action *streamPlayAction;
    #endif
    Action *expandInterfaceAction;
    Action *songInfoAction;
    Action *fullScreenAction;
    Action *quitAction;
    Action *restoreAction;
    Action *locateTrackAction;
    Action *showPlayQueueAction;
    Action *libraryTabAction;
    Action *albumsTabAction;
    Action *foldersTabAction;
    Action *playlistsTabAction;
    #ifdef ENABLE_DYNAMIC
    Action *dynamicTabAction;
    #endif
    #ifdef ENABLE_STREAMS
    Action *streamsTabAction;
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    Action *onlineTabAction;
    #endif
    #ifdef TAGLIB_FOUND
    Action *editPlayQueueTagsAction;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    Action *devicesTabAction;
    #endif
    Action *searchTabAction;
    Action *expandAllAction;
    Action *collapseAllAction;
    Action *serverInfoAction;
    Action *clearPlayQueueAction;
    Action *cancelAction;
    TrayItem *trayItem;
    QPoint lastPos;
    QSize expandedSize;
    QSize collapsedSize;
    Song current;
    QWidget *playQueuePage;
    LibraryPage *libraryPage;
    AlbumsPage *albumsPage;
    FolderPage *folderPage;
    PlaylistsPage *playlistsPage;
    #ifdef ENABLE_DYNAMIC
    DynamicPage *dynamicPage;
    #endif
    #ifdef ENABLE_STREAMS
    StreamsPage *streamsPage;
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    OnlineServicesPage *onlinePage;
    #endif
    QWidget *contextPage;
    #ifdef ENABLE_DEVICES_SUPPORT
    DevicesPage *devicesPage;
    #endif
    SearchPage *searchPage;
    #ifndef Q_OS_WIN
    Mpris *mpris;
    GtkProxyStyle *gtkStyle;
    #endif // Q_OS_WIN
    QTimer *statusTimer;
    QTimer *playQueueSearchTimer;
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    QTimer *mpdAccessibilityTimer;
    #endif

    enum ConnState {
        CS_Init,
        CS_Connected,
        CS_Disconnected
    };

    ConnState connectedState;

    enum StopState {
        StopState_None     = 0,
        StopState_Stopping = 1
//         StopState_Pausing  = 2
    };

    bool fadeStop;
    bool stopAfterCurrent;
    QPropertyAnimation *volumeFade;
    int volume;
    int origVolume;
    int lastVolume;
    StopState stopState;
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    bool httpStreamEnabled;
    #if QT_VERSION < 0x050000
    Phonon::MediaObject *httpStream;
    #else
    QMediaPlayer *httpStream;
    #endif
    #endif
    friend class CoverEventHandler;
    friend class TrayItem;
};

#endif
