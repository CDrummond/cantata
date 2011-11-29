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
#include "gui/musiclibrarymodel.h"
#include "gui/musiclibraryproxymodel.h"
#include "gui/playlistsmodel.h"
#include "gui/playlistsproxymodel.h"
#include "gui/playlisttablemodel.h"
#include "gui/playlisttableproxymodel.h"
#include "gui/dirviewmodel.h"
#include "gui/dirviewproxymodel.h"
#include "lib/mpdstatus.h"
#include "lib/song.h"

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
    void setupPlaylistViewMenu();
    void setupPlaylistViewHeader();

private Q_SLOTS:
    void playlistItemsSelected(bool s);
    void showVolumeControl();
    int showPreferencesDialog();
    void toggleTrayIcon(bool enable);
    void mpdConnectionDied();
    void updateDb();
#ifndef ENABLE_KDE_SUPPORT
    void showAboutDialog();
#endif
    void positionSliderPressed();
    void positionSliderReleased();
    void startPlayingSong();
    void nextTrack();
    void stopTrack();
    void playPauseTrack();
    void previousTrack();
    void setVolume(int vol);
    void increaseVolume();
    void decreaseVolume();
    void setPosition();
    void setRandom();
    void setRepeat();
    void setConsume();
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
    void clearPlaylist();
    void replacePlaylist();
    void libraryItemActivated(const QModelIndex &);
    void dirViewItemActivated(const QModelIndex &);
    void addToPlaylist();
    void trayIconClicked(QSystemTrayIcon::ActivationReason reason);
    void playlistTableViewContextMenuClicked();
    void playListTableViewToggleItem(const bool visible);
    void cropPlaylist();
    void updatePlayListStatus();
    void movePlaylistItems(const QList<quint32> items, const int row, const int size);
    void addFilenamesToPlaylist(const QStringList filenames, const int row, const int size);
    void addPlaylistsSelectionToPlaylist();
    void playlistsViewItemDoubleClicked(const QModelIndex &index);
    void removePlaylist();
    void savePlaylist();
    void renamePlaylist();
    void updateStoredPlaylists();
    void copySongInfo();
    void togglePlaylist();
    void currentTabChanged(int index);
    void lyrics(const QString &artist, const QString &title, const QString &text);
    void cover(const QString &artist, const QString &album, const QImage &img);
    void updateGenres(const QStringList &genres);
    void showLibraryTab();
    void showFoldersTab();
    void showPlaylistsTab();
    void showLyricsTab();

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
    friend class VolumeSliderEventHandler;
    friend class CoverEventHandler;
};

#endif
