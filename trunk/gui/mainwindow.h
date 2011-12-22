/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <KXmlGuiWindow>
#include <KStatusBar>
#else
#include <QMainWindow>
#include <QSystemTrayIcon>
#endif

// #include <QTimer>
#include <QMenu>
#include <QHeaderView>
#include <QStringList>
#include <QDateTime>
#include <QProxyStyle>
#include <QPixmap>
#include <QResizeEvent>
#include <QMoveEvent>

#include "ui_mainwindow.h"
#include "playqueuemodel.h"
#include "playqueueproxymodel.h"
#include "mpdstatus.h"
#include "song.h"

#ifdef ENABLE_KDE_SUPPORT
class KAction;
class KStatusNotifierItem;
class KMenu;
#define Action KAction
#else
#define Action QAction
#endif

class MainWindow;
class UpdateDialog;
class LibraryPage;
class AlbumsPage;
class FolderPage;
class PlaylistsPage;
class LyricsPage;
class StreamsPage;
class InfoPage;
class ServerInfoPage;
class QThread;
class QAbstractItemView;

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

// class CoverEventHandler : public QObject
// {
//     Q_OBJECT
//
// public:
//     CoverEventHandler(MainWindow *w);
//
// protected:
//     bool eventFilter(QObject *obj, QEvent *event);
//
// private:
//     MainWindow * const window;
//     bool pressed;
// };

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
        PAGE_STREAMS,
        PAGE_LYRICS,
        PAGE_INFO,
        PAGE_SERVER_INFO
    };

    MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private:
    void setupTrayIcon();
    void setupPlaylistView();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void setDetails(const QString &host, quint16 port, const QString &pass);
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

public Q_SLOTS:
    void showError(const QString &message);

private Q_SLOTS:
    void playbackButtonsMenu();
    void controlButtonsMenu();
    void setPlaybackButtonsSize(bool small);
    void setControlButtonsSize(bool small);
    void songLoaded();
    void mpdConnectionStateChanged(bool connected);
    void playlistItemsSelected(bool s);
    void showVolumeControl();
    void showPreferencesDialog();
    void updateSettings();
    void mpdConnectionDied();
    void updateDb();
#ifndef ENABLE_KDE_SUPPORT
    void showAboutDialog();
#endif
    void positionSliderPressed();
    void positionSliderReleased();
    void stopTrack();
    void playPauseTrack();
    void increaseVolume();
    void decreaseVolume();
    void setPosition();
    void searchPlaylist();
    void updatePlaylist(const QList<Song> &songs);
    void updateCurrentSong(const Song &song);
    void updateStats();
    void updateStatus();
    void updatePositionSilder();
    void playlistItemActivated(const QModelIndex &);
    void removeFromPlaylist();
    void replacePlaylist();
    void addToPlaylist();
    void addToNewStoredPlaylist();
    void addToExistingStoredPlaylist(const QString &name);
#ifdef ENABLE_KDE_SUPPORT
    void trayItemScrollRequested(int delta, Qt::Orientation orientation);
#else
    void trayIconClicked(QSystemTrayIcon::ActivationReason reason);
#endif
    void playQueueContextMenuClicked();
    void togglePlayQueueHeaderItem(const bool visible);
    void cropPlaylist();
    void updatePlayListStatus();
    void copyTrackInfo();
    void togglePlaylist();
    void currentTabChanged(int index);
    void cover(const QString &artist, const QString &album, const QImage &img);
    void showLibraryTab() { showTab(PAGE_LIBRARY); }
    void showAlbumsTab() { showTab(PAGE_ALBUMS); }
    void showFoldersTab() { showTab(PAGE_FOLDERS); }
    void showPlaylistsTab() { showTab(PAGE_PLAYLISTS); }
    void showStreamsTab() { showTab(PAGE_STREAMS); }
    void showLyricsTab() { showTab(PAGE_LYRICS); }
    void showInfoTab() { showTab(PAGE_INFO); }
    void showServerInfoTab() { showTab(PAGE_SERVER_INFO); }

private:
    bool currentIsStream();
    void showTab(int page);

private:
    int loaded;
    MPDStatus::State lastState;
    quint32 songTime;
    qint32 lastSongId;
    quint32 lastPlaylist;
    QDateTime lastDbUpdate;
    int fetchStatsFactor;
    int nowPlayingFactor;
    PlayQueueModel playQueueModel;
    PlayQueueProxyModel playQueueProxyModel;
    bool draggingPositionSlider;
    QIcon playbackPause;
    QIcon playbackPlay;
    QHeaderView *playQueueHeader;
    VolumeSliderEventHandler *volumeSliderEventHandler;
//     CoverEventHandler *coverEventHandler;
    VolumeControl *volumeControl;
    QTimer elapsedTimer;
    Action *prevTrackAction;
    Action *nextTrackAction;
    Action *playPauseTrackAction;
    Action *stopTrackAction;
    Action *increaseVolumeAction;
    Action *decreaseVolumeAction;
    Action *addToPlaylistAction;
    Action *replacePlaylistAction;
    Action *removeFromPlaylistAction;
    Action *addToStoredPlaylistAction;
    Action *clearPlaylistAction;
    Action *copyTrackInfoAction;
    Action *cropPlaylistAction;
    Action *shufflePlaylistAction;
    Action *savePlaylistAction;
    Action *randomPlaylistAction;
    Action *repeatPlaylistAction;
    Action *consumePlaylistAction;
    Action *expandInterfaceAction;
    Action *quitAction;
    Action *libraryTabAction;
    Action *albumsTabAction;
    Action *foldersTabAction;
    Action *playlistsTabAction;
    Action *lyricsTabAction;
    Action *streamsTabAction;
    #ifdef ENABLE_WEBKIT
    Action *infoTabAction;
    #endif
    Action *serverInfoTabAction;
    Action *updateDbAction;
    Action *smallPlaybackButtonsAction;
    Action *smallControlButtonsAction;
    QList<QAction *> viewActions;
    #ifdef ENABLE_KDE_SUPPORT
    KStatusNotifierItem *trayItem;
    KMenu *trayItemMenu;
    #else
    QSystemTrayIcon *trayItem;
    QMenu *trayItemMenu;
    #endif
    QMenu *playbackBtnsMenu;
    QMenu *controlBtnsMenu;
    QMenu *playQueueMenu;
    QPixmap noCover;
    QPixmap noStreamCover;
    QPoint lastPos;
    UpdateDialog *updateDialog;
    QString mpdDir;
    Song current;
    bool lyricsNeedUpdating;
    #ifdef ENABLE_WEBKIT
    bool infoNeedsUpdating;
    #endif
    LibraryPage *libraryPage;
    AlbumsPage *albumsPage;
    FolderPage *folderPage;
    PlaylistsPage *playlistsPage;
    LyricsPage *lyricsPage;
    StreamsPage *streamsPage;
    #ifdef ENABLE_WEBKIT
    InfoPage *infoPage;
    #endif
    ServerInfoPage *serverInfoPage;
    QThread *mpdThread;
    friend class VolumeSliderEventHandler;
//     friend class CoverEventHandler;
    friend class LibraryPage;
    friend class AlbumsPage;
    friend class FolderPage;
    friend class PlaylistsPage;
    friend class StreamsPage;
    friend class LyricsPage;
    friend class InfoPage;
    friend class ServerInfoPage;
};

#endif
