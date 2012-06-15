/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KXmlGuiWindow>
#else
#include <QtCore/qglobal.h>
#include <QtGui/QMainWindow>
#include <QtGui/QSystemTrayIcon>
#endif
#include <QtGui/QMenu>
#include <QtGui/QProxyStyle>
#include <QtGui/QPixmap>
#include <QtGui/QResizeEvent>
#include <QtGui/QMoveEvent>
#include <QtGui/QToolButton>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
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

#ifdef ENABLE_KDE_SUPPORT
class KAction;
class KStatusNotifierItem;
class KMenu;
class KNotification;
#define Action KAction
#else
#define Action QAction
#endif

class MainWindow;
class LibraryPage;
class AlbumsPage;
class FolderPage;
class PlaylistsPage;
#ifndef Q_OS_WIN
class DynamicPage;
#endif
class LyricsPage;
class StreamsPage;
class InfoPage;
class ServerInfoPage;
#ifdef ENABLE_KDE_SUPPORT
class DevicesPage;
#endif
class QThread;
class QAbstractItemView;
#ifndef Q_OS_WIN
class DockManager;
class Mpris;
#endif
class QTimer;
class QPropertyAnimation;
class QActionGroup;

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

class VolumeControl : public QMenu
{
    Q_OBJECT

public:
    VolumeControl(QWidget *parent);
    virtual ~VolumeControl();

    void installSliderEventFilter(QObject *filter);

public Q_SLOTS:
    void increaseVolume();
    void decreaseVolume();
    void setValue(int v);

    QSlider * sliderWidget() { return slider; }

Q_SIGNALS:
    void valueChanged(int v);

private:
    QSlider *slider;
};

class CoverEventHandler : public QObject
{
    Q_OBJECT

public:
    CoverEventHandler(MainWindow *w);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
    MainWindow * const window;
    bool pressed;
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
        PAGE_LIBRARY,
        PAGE_ALBUMS,
        PAGE_FOLDERS,
        PAGE_PLAYLISTS,
        #ifndef Q_OS_WIN
        PAGE_DYNAMIC,
        #endif
        PAGE_STREAMS,
        PAGE_LYRICS,
        PAGE_INFO,
        PAGE_SERVER_INFO
        #ifdef ENABLE_KDE_SUPPORT
        , PAGE_DEVICES
        #endif
    };

    Q_PROPERTY(int volume READ mpdVolume WRITE setMpdVolume)

    static void initButton(QToolButton *btn) {
        btn->setAutoRaise(true);
        btn->setIconSize(QSize(16, 16));
    }

    MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int mpdVolume() const {
        return volume;
    }

    void load(const QList<QUrl> &urls);
    const QDateTime & getDbUpdate() const;

protected:
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    void setupTrayIcon();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void setDetails(const MPDConnectionDetails &det);
    void removeSongs(const QList<qint32> &);
    void pause(bool p);
    void play();
    void stop();
    void getStatus();
    void getStats();
    void clear();
    void playListInfo();
    void currentSong();
    void setSeekId(quint32, quint32);
    void startPlayingSongId(quint32);
    void setVolume(int);
    void outputs();
    void enableOutput(int id, bool);
    void setPriority(const QList<quint32> &ids, quint8 priority);

public Q_SLOTS:
    void showError(const QString &message, bool showActions=false);
    void showInformation(const QString &message);
    void showPage(const QString &page, bool focusSearch);
    #ifndef Q_OS_WIN
    void dynamicStatus(const QString &message);
    #endif

private Q_SLOTS:
    void setMpdVolume(int );
    void playbackButtonsMenu();
    void controlButtonsMenu();
    void setPlaybackButtonsSize(bool small);
    void setControlButtonsSize(bool small);
    void songLoaded();
    void messageWidgetVisibility(bool v);
    void mpdConnectionStateChanged(bool connected);
    void playQueueItemsSelected(bool s);
    void showVolumeControl();
    void showPreferencesDialog();
    void updateSettings();
    void toggleOutput();
    void changeConnection();
    void connectToMpd();
    void connectToMpd(const MPDConnectionDetails &details);
    void refresh();
    #ifndef ENABLE_KDE_SUPPORT
    void showAboutDialog();
    #endif
    #ifdef PHONON_FOUND
    void toggleStream(bool s);
    #endif
    void positionSliderPressed();
    void positionSliderReleased();
    void stopTrack();
    void playPauseTrack();
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
    void updatePosition();
    void playQueueItemActivated(const QModelIndex &);
    void removeFromPlayQueue();
    void replacePlayQueue();
    void addToPlayQueue();
    void addWithPriorty();
    void addToNewStoredPlaylist();
    void addToExistingStoredPlaylist(const QString &name);
    void removeItems();
    #ifdef ENABLE_KDE_SUPPORT
    void trayItemScrollRequested(int delta, Qt::Orientation orientation);
    void notificationClosed();
    #else
    void trayItemClicked(QSystemTrayIcon::ActivationReason reason);
    #endif
    void cropPlayQueue();
    void updatePlayQueueStats(int artists, int albums, int songs, quint32 time);
    void copyTrackInfo();
    void togglePlayQueue();
    void sidebarModeChanged();
    void currentTabChanged(int index);
    void tabToggled(int index);
    void showLibraryTab() { showTab(PAGE_LIBRARY); }
    void showAlbumsTab() { showTab(PAGE_ALBUMS); }
    void showFoldersTab() { showTab(PAGE_FOLDERS); }
    void showPlaylistsTab() { showTab(PAGE_PLAYLISTS); }
    #ifndef Q_OS_WIN
    void showDynamicTab() { showTab(PAGE_DYNAMIC); }
    #endif
    void showStreamsTab() { showTab(PAGE_STREAMS); }
    void showLyricsTab() { showTab(PAGE_LYRICS); }
    void showInfoTab() { showTab(PAGE_INFO); }
    void showServerInfoTab() { showTab(PAGE_SERVER_INFO); }
    void toggleSplitterAutoHide(bool ah);
    void locateTrack();
    #ifdef ENABLE_KDE_SUPPORT
    void showDevicesTab() { showTab(PAGE_DEVICES); }
    #endif
    #ifndef Q_OS_WIN
    void toggleMpris();
    void toggleDockManager();
    #endif
//     void createDataCd();
//     void createAudioCd();
    void editTags();
    void editPlayQueueTags();
    void organiseFiles();
    #ifdef ENABLE_DEVICES_SUPPORT
    void addToDevice(const QString &udi);
    void deleteSongs();
    void copyToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    void replayGain();
    #endif
    void focusSearch();
    void expandAll();
    void collapseAll();
    void checkMpdDir();
    void outputsUpdated(const QList<Output> &outputs);

private:
    void updateConnectionsMenu();
    void readSettings();
    int calcMinHeight();
    void addToPlayQueue(bool replace, quint8 priority=0);
    void editTags(const QList<Song> &songs, bool isPlayQueue);
    bool currentIsStream() const;
    void updateWindowTitle();
    void startVolumeFade(/*bool stop*/);
    void stopVolumeFade();
//     void callK3b(const QString &type);
    void showTab(int page);
    void focusTabSearch();
    bool fadeWhenStop() const;

private:
    int loaded;
    MPDState lastState;
    quint32 songTime;
    qint32 lastSongId;
    QDateTime lastDbUpdate;
    PlayQueueModel playQueueModel;
    PlayQueueProxyModel playQueueProxyModel;
    bool autoScrollPlayQueue;
    bool draggingPositionSlider;
    QIcon playbackPause;
    QIcon playbackPlay;
    VolumeSliderEventHandler *volumeSliderEventHandler;
    VolumeControl *volumeControl;
    Action *prefAction;
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
    Action *addToPlayQueueAction;
    Action *replacePlayQueueAction;
    Action *removeFromPlayQueueAction;
    Action *addToStoredPlaylistAction;
    Action *clearPlayQueueAction;
    Action *copyTrackInfoAction;
    Action *cropPlayQueueAction;
    Action *shufflePlayQueueAction;
    Action *savePlayQueueAction;
    Action *randomPlayQueueAction;
    Action *repeatPlayQueueAction;
    Action *singlePlayQueueAction;
    Action *consumePlayQueueAction;
    Action *addWithPriorityAction;
    Action *addPrioHighestAction;  // 255
    Action *addPrioHighAction;     // 200
    Action *addPrioMediumAction;   // 125
    Action *addPrioLowAction;      // 50
    Action *addPrioCustomAction;
    Action *setPriorityAction;
    #ifdef PHONON_FOUND
    Action *streamPlayAction;
    #endif
    Action *expandInterfaceAction;
    Action *quitAction;
    Action *locateTrackAction;
    Action *libraryTabAction;
    Action *albumsTabAction;
    Action *foldersTabAction;
    Action *playlistsTabAction;
    #ifndef Q_OS_WIN
    Action *dynamicTabAction;
    #endif
    Action *lyricsTabAction;
    Action *streamsTabAction;
    Action *removeAction;
//     Action *burnAction;
//     Action *createDataCdAction;
//     Action *createAudioCdAction;
    Action *editTagsAction;
    Action *editPlayQueueTagsAction;
    #ifdef ENABLE_WEBKIT
    Action *infoTabAction;
    #endif
    Action *serverInfoTabAction;
    #ifdef ENABLE_DEVICES_SUPPORT
    Action *devicesTabAction;
    Action *copyToDeviceAction;
    Action *deleteSongsAction;
    #endif
    Action *organiseFilesAction;
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    Action *replaygainAction;
    #endif
    Action *refreshAction;
    Action *searchAction;
    Action *expandAllAction;
    Action *collapseAllAction;
    Action *smallPlaybackButtonsAction;
    Action *smallControlButtonsAction;
    #ifdef ENABLE_KDE_SUPPORT
    KStatusNotifierItem *trayItem;
    KMenu *trayItemMenu;
    KNotification *notification;
    #else
    QSystemTrayIcon *trayItem;
    QMenu *trayItemMenu;
    #endif
    QMenu *playbackBtnsMenu;
    QMenu *controlBtnsMenu;
    QPoint lastPos;
    QSize expandedSize;
    QSize collapsedSize;
    Song current;
    bool lyricsNeedUpdating;
    #ifdef ENABLE_WEBKIT
    bool infoNeedsUpdating;
    #endif
    LibraryPage *libraryPage;
    AlbumsPage *albumsPage;
    FolderPage *folderPage;
    PlaylistsPage *playlistsPage;
    #ifndef Q_OS_WIN
    DynamicPage *dynamicPage;
    #endif
    LyricsPage *lyricsPage;
    StreamsPage *streamsPage;
    #ifdef ENABLE_WEBKIT
    InfoPage *infoPage;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    DevicesPage *devicesPage;
    #endif
    ServerInfoPage *serverInfoPage;
    QThread *mpdThread;
    #ifndef Q_OS_WIN
    DockManager *dock;
    Mpris *mpris;
    #endif
    QTimer *playQueueSearchTimer;
    bool usingProxy;

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
    friend class LibraryPage;
    friend class AlbumsPage;
    friend class FolderPage;
    friend class PlaylistsPage;
    #ifndef Q_OS_WIN
    friend class DynamicPage;
    #endif
    friend class StreamsPage;
    friend class LyricsPage;
    friend class InfoPage;
    friend class ServerInfoPage;
    #ifdef ENABLE_DEVICES_SUPPORT
    friend class DevicesPage;
    #endif
};

#endif
