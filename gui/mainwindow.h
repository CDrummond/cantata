/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef PHONON_FOUND
#include <phonon/mediaobject.h>
#endif

class Action;
class ActionCollection;
class MainWindow;
class LibraryPage;
class AlbumsPage;
class FolderPage;
class PlaylistsPage;
class DynamicPage;
class LyricsPage;
class StreamsPage;
class OnlineServicesPage;
class InfoPage;
#ifdef ENABLE_DEVICES_SUPPORT
class DevicesPage;
#endif
class QAbstractItemView;
#ifndef Q_OS_WIN
class Mpris;
class GnomeMediaKeys;
#endif // !defined Q_OS_WIN
class QTimer;
class QPropertyAnimation;
class QActionGroup;
class QDateTime;
class VolumeControl;
class TrayItem;
class GtkProxyStyle;

// Dummy class so that when class name is saved to the config file, we get a more meaningful name than QWidget!!!
class PlayQueuePage : public QWidget
{
    Q_OBJECT

public:
    PlayQueuePage(QWidget *p) : QWidget(p) { }
};

class DeleteKeyEventHandler : public QObject
{
    Q_OBJECT

public:
    DeleteKeyEventHandler(QAbstractItemView *v, QAction *a);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    QAbstractItemView *view;
    QAction *act;
};

class VolumeSliderEventHandler : public QObject
{
    Q_OBJECT

public:
    VolumeSliderEventHandler(MainWindow *w);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    MainWindow * const window;
};

#ifdef ENABLE_KDE_SUPPORT
class MainWindow : public KXmlGuiWindow, private Ui::MainWindow
#else
class MainWindow : public QMainWindow, private Ui::MainWindow
#endif
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
        PAGE_DYNAMIC,
        PAGE_STREAMS,
        PAGE_ONLINE,
        PAGE_LYRICS,
        PAGE_INFO
        #ifdef ENABLE_DEVICES_SUPPORT
        , PAGE_DEVICES
        #endif
    };

    Q_PROPERTY(int volume READ mpdVolume WRITE setMpdVolume)

    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int mpdVolume() const { return volume; }

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
    void removeSongs(const QList<qint32> &);
    void pause(bool p);
    void play();
    void stop();
    void getStatus();
    void getStats(bool andUpdate);
    void updateMpd();
    void clear();
    void playListInfo();
    void currentSong();
    void setSeekId(quint32, quint32);
    void startPlayingSongId(quint32);
    void setVolume(int);
    void outputs();
    void enableOutput(int id, bool);
    void setPriority(const QList<quint32> &ids, quint8 priority);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

public Q_SLOTS:
    void showError(const QString &message, bool showActions=false);
    void showInformation(const QString &message);
    void showPage(const QString &page, bool focusSearch);
    void dynamicStatus(const QString &message);
    void hideWindow();
    void restoreWindow();
    void load(const QStringList &urls);
    #ifdef ENABLE_KDE_SUPPORT
    void configureShortcuts();
    void saveShortcuts();
    #endif
    void setMpdVolume(int );
    void songLoaded();
    void messageWidgetVisibility(bool v);
    void mpdConnectionStateChanged(bool connected);
    void playQueueItemsSelected(bool s);
    void showVolumeControl();
    void showPreferencesDialog();
    void quit();
    void updateSettings();
    void toggleOutput();
    void changeConnection();
    void connectToMpd();
    void connectToMpd(const MPDConnectionDetails &details);
    void refresh();
    #ifndef ENABLE_KDE_SUPPORT
    void showAboutDialog();
    #endif
    void showServerInfo();
    #ifdef PHONON_FOUND
    void toggleStream(bool s);
    #endif
    void stopTrack();
    void playPauseTrack();
    void nextTrack();
    void prevTrack();
    void increaseVolume();
    void decreaseVolume();
    void setPosition();
    void searchPlayQueue();
    void realSearchPlayQueue();
    void updatePlayQueue(const QList<Song> &songs);
    void updateCurrentSong(const Song &song);
    void scrollPlayQueue();
    void updateStats();
    void updateStatus();
    void playQueueItemActivated(const QModelIndex &);
    void removeFromPlayQueue();
    void replacePlayQueue();
    void addToPlayQueue();
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
    void fullScreen();
    void sidebarModeChanged();
    void currentTabChanged(int index);
    void tabToggled(int index);
    void showPlayQueue() { showTab(PAGE_PLAYQUEUE); }
    void showLibraryTab() { showTab(PAGE_LIBRARY); }
    void showAlbumsTab() { showTab(PAGE_ALBUMS); }
    void showFoldersTab() { showTab(PAGE_FOLDERS); }
    void showPlaylistsTab() { showTab(PAGE_PLAYLISTS); }
    void showDynamicTab() { showTab(PAGE_DYNAMIC); }
    void showStreamsTab() { showTab(PAGE_STREAMS); }
    void showOnlineTab() { showTab(PAGE_ONLINE); }
    void showLyricsTab() { showTab(PAGE_LYRICS); }
    void showInfoTab() { showTab(PAGE_INFO); }
    void showDevicesTab() {
        #ifdef ENABLE_DEVICES_SUPPORT
        showTab(PAGE_DEVICES);
        #endif
    }
    void toggleSplitterAutoHide();
    void locateTrack();
    #ifdef TAGLIB_FOUND
    void editTags();
    void editPlayQueueTags();
    void organiseFiles();
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    void addToDevice(const QString &udi);
    void deleteSongs();
    void copyToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    void replayGain();
    #endif
    void setCover();
    void goBack();
    void focusSearch();
    void expandAll();
    void collapseAll();
    void checkMpdDir();
    void outputsUpdated(const QList<Output> &outputs);
    void updateConnectionsMenu();
    void controlConnectionsMenu(bool enable=true);

private:
    void updateStatus(MPDStatus * const status);
    void readSettings();
    int calcMinHeight();
    int calcCompactHeight();
    void addToPlayQueue(bool replace, quint8 priority=0);
    #ifdef TAGLIB_FOUND
    void editTags(const QList<Song> &songs, bool isPlayQueue);
    #endif
    bool currentIsStream() const;
    void updateWindowTitle();
    void startVolumeFade(/*bool stop*/);
    void stopVolumeFade();
    void showTab(int page);
    bool fadeWhenStop() const;

private:
    int loaded;
    MPDState lastState;
    quint32 songTime;
    qint32 lastSongId;
    PlayQueueModel playQueueModel;
    PlayQueueProxyModel playQueueProxyModel;
    bool autoScrollPlayQueue;
    VolumeSliderEventHandler *volumeSliderEventHandler;
    VolumeControl *volumeControl;
    Action *prefAction;
    #ifdef ENABLE_KDE_SUPPORT
    Action *shortcutsAction;
    #endif
    Action *connectAction;
    Action *connectionsAction;
    Action *outputsAction;
    QActionGroup *connectionsGroup;
    Action *prevTrackAction;
    Action *nextTrackAction;
    Action *playPauseTrackAction;
    Action *stopTrackAction;
    Action *increaseVolumeAction;
    Action *decreaseVolumeAction;
    Action *removeFromPlayQueueAction;
    Action *addPlayQueueToStoredPlaylistAction;
    Action *clearPlayQueueAction;
    Action *copyTrackInfoAction;
    Action *cropPlayQueueAction;
    Action *shufflePlayQueueAction;
    Action *savePlayQueueAction;
    Action *addStreamToPlayQueueAction;
    Action *randomPlayQueueAction;
    Action *repeatPlayQueueAction;
    Action *singlePlayQueueAction;
    Action *consumePlayQueueAction;
    Action *setPriorityAction;
    #ifdef PHONON_FOUND
    Action *streamPlayAction;
    #endif
    Action *expandInterfaceAction;
    Action *fullScreenAction;
    Action *quitAction;
    Action *restoreAction;
    Action *locateTrackAction;
    Action *showPlayQueueAction;
    Action *libraryTabAction;
    Action *albumsTabAction;
    Action *foldersTabAction;
    Action *playlistsTabAction;
    Action *dynamicTabAction;
    Action *lyricsTabAction;
    Action *streamsTabAction;
    Action *onlineTabAction;
    #ifdef TAGLIB_FOUND
    Action *editPlayQueueTagsAction;
    #endif
    Action *infoTabAction;
    #ifdef ENABLE_DEVICES_SUPPORT
    Action *devicesTabAction;
    #endif
    Action *searchAction;
    Action *expandAllAction;
    Action *collapseAllAction;
    Action *serverInfoAction;
    QAction *autoHideSplitterAction;
    TrayItem *trayItem;
    QPoint lastPos;
    QSize expandedSize;
    QSize collapsedSize;
    Song current;
    bool lyricsNeedUpdating;
    bool infoNeedsUpdating;
    QWidget *playQueuePage;
    LibraryPage *libraryPage;
    AlbumsPage *albumsPage;
    FolderPage *folderPage;
    PlaylistsPage *playlistsPage;
    DynamicPage *dynamicPage;
    LyricsPage *lyricsPage;
    StreamsPage *streamsPage;
    OnlineServicesPage *onlinePage;
    InfoPage *infoPage;
    #ifdef ENABLE_DEVICES_SUPPORT
    DevicesPage *devicesPage;
    #endif
    #ifndef Q_OS_WIN
    Mpris *mpris;
    GnomeMediaKeys *gnomeMediaKeys;
    GtkProxyStyle *gtkStyle;
    #endif // Q_OS_WIN
    QTimer *playQueueSearchTimer;
    bool usingProxy;
    #ifdef Q_OS_LINUX
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
    QPropertyAnimation *volumeFade;
    int volume;
    int origVolume;
    int lastVolume;
    StopState stopState;
    #ifdef PHONON_FOUND
    bool phononStreamEnabled;
    Phonon::MediaObject *phononStream;
    #endif

    friend class VolumeSliderEventHandler;
    friend class CoverEventHandler;
    friend class TrayItem;
};

#endif
