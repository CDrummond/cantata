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
#endif

// #include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QHeaderView>
#include <QStringList>
#include <QDateTime>
#include <QProxyStyle>
#include <QPixmap>
#include <QResizeEvent>
#include <QMoveEvent>

#include "ui_mainwindow.h"
#include "musiclibrarymodel.h"
#include "musiclibraryproxymodel.h"
#include "playlistsmodel.h"
#include "playlistsproxymodel.h"
#include "playlisttablemodel.h"
#include "playlisttableproxymodel.h"
#include "dirviewmodel.h"
#include "dirviewproxymodel.h"
#include "mpdstatus.h"
#include "song.h"

#ifdef ENABLE_KDE_SUPPORT
class KAction;
#define Action KAction
#else
#define Action QAction
#endif

class MainWindow;
class UpdateDialog;
class LibraryPage;
class FolderPage;
class PlaylistsPage;
class LyricsPage;
class StreamsPage;
class QThread;

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
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private:
    void addLibrarySelectionToPlaylist();
    void addDirViewSelectionToPlaylist();
    QStringList walkDirView(QModelIndex rootItem);
    bool setupTrayIcon();
    void setupPlaylistView();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via singal/slots)
    void setDetails(const QString &host, quint16 port, const QString &pass);
    void loadPlaylist(const QString &name);
    void removePlaylist(const QString &name);
    void savePlaylist(const QString &name);
    void renamePlaylist(const QString &oldname, const QString &newname);
    void removeSongs(const QList<qint32> &);
    void add(const QStringList &files);
    void pause(bool p);
    void play();
    void stop();
    void getStatus();
    void getStats();
    void clear();
    void playListInfo();
    void listAllInfo(const QDateTime &);
    void listAll();
    void currentSong();
    void setSeekId(quint32, quint32);
    void startPlayingSongId(quint32);

private Q_SLOTS:
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
    void searchMusicLibrary();
    void searchDirView();
    void searchPlaylist();
    void updatePlaylist(const QList<Song> &songs);
    void updateCurrentSong(const Song &song);
    void updateStats();
    void updateStatus();
    void updatePositionSilder();
    void playlistItemActivated(const QModelIndex &);
    void removeFromPlaylist();
    void replacePlaylist();
    void libraryItemActivated(const QModelIndex &);
    void dirViewItemActivated(const QModelIndex &);
    void addToPlaylist();
    void trayIconClicked(QSystemTrayIcon::ActivationReason reason);
    void playlistTableViewContextMenuClicked();
    void playListTableViewToggleItem(const bool visible);
    void cropPlaylist();
    void updatePlayListStatus();
    void addPlaylistsSelectionToPlaylist();
    void playlistsViewItemDoubleClicked(const QModelIndex &index);
    void removePlaylist();
    void savePlaylist();
    void renamePlaylist();
    void copySongInfo();
    void togglePlaylist();
    void currentTabChanged(int index);
    void cover(const QString &artist, const QString &album, const QImage &img);
    void updateGenres(const QStringList &genres);
    void showLibraryTab();
    void showFoldersTab();
    void showPlaylistsTab();
    void showLyricsTab();
    void showStreamsTab();

private:
    int loaded;
    MPDStatus::State lastState;
    quint32 songTime;
    qint32 lastSongId;
    quint32 lastPlaylist;
    QDateTime lastDbUpdate;
    int fetchStatsFactor;
    int nowPlayingFactor;
    PlaylistsModel playlistsModel;
    PlaylistsProxyModel playlistsProxyModel;
    PlaylistTableModel playlistModel;
    PlaylistTableProxyModel playlistProxyModel;
    dirViewModel dirviewModel;
    DirViewProxyModel dirProxyModel;
    MusicLibraryModel musicLibraryModel;
    MusicLibraryProxyModel libraryProxyModel;
    bool draggingPositionSlider;
    QIcon playbackPause;
    QIcon playbackPlay;
    QHeaderView *playlistTableViewHeader;
    VolumeSliderEventHandler *volumeSliderEventHandler;
    CoverEventHandler *coverEventHandler;
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
    Action *removePlaylistAction;
    Action *removeFromPlaylistAction;
    Action *clearPlaylistAction;
    Action *copySongInfoAction;
    Action *cropPlaylistAction;
    Action *shufflePlaylistAction;
    Action *renamePlaylistAction;
    Action *savePlaylistAction;
    Action *randomPlaylistAction;
    Action *repeatPlaylistAction;
    Action *consumePlaylistAction;
    Action *showPlaylistAction;
    Action *quitAction;
    Action *libraryTabAction;
    Action *foldersTabAction;
    Action *playlistsTabAction;
    Action *lyricsTabAction;
    Action *streamsTabAction;
    Action *updateDbAction;
    QList<QAction *> viewActions;
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QMenu *playlistTableViewMenu;
    QPixmap noCover;
    QPoint lastPos;
    UpdateDialog *updateDialog;
    QString mpdDir;
    Song current;
    bool lyricsNeedUpdating;
    LibraryPage *libraryPage;
    FolderPage *folderPage;
    PlaylistsPage *playlistsPage;
    LyricsPage *lyricsPage;
    StreamsPage *streamsPage;
    QThread *mpdThread;
    friend class VolumeSliderEventHandler;
    friend class CoverEventHandler;
    friend class StreamsPage;
};

#endif
