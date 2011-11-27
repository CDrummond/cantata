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
#include "lib/output.h"
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
    void addSelectionToPlaylist();
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
    void addToPlaylistItemActivated();
    void trayIconClicked(QSystemTrayIcon::ActivationReason reason);
    void crossfadingChanged(const int seconds);
    void playlistTableViewContextMenuClicked();
    void playListTableViewToggleItem(const bool visible);
    void cropPlaylist();
    void updatePlayListStatus();
    void movePlaylistItems(const QList<quint32> items, const int row, const int size);
    void addFilenamesToPlaylist(const QStringList filenames, const int row, const int size);
    void addPlaylistToPlaylistPushButtonActivated();
    void loadPlaylistPushButtonActivated();
    void playlistsViewItemDoubleClicked(const QModelIndex &index);
    void removePlaylistPushButtonActivated();
    void savePlaylistPushButtonActivated();
    void renamePlaylistActivated();
    void updateStoredPlaylists();
    void copySongInfo();
    void updateOutpus(const QList<Output> &outputs);
    void outputChanged(QAction *action);
    void togglePlaylist();
    void currentTabChanged(int index);
    void lyrics(const QString &artist, const QString &title, const QString &text);
    void cover(const QString &artist, const QString &album, const QImage &img);
    void updateGenres(const QStringList &genres);

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
    QIcon playbackStop;
    QIcon playbackNext;
    QIcon playbackPrev;
    QIcon playbackPause;
    QIcon playbackPlay;
    QHeaderView *playlistTableViewHeader;
    VolumeSliderEventHandler *volumeSliderEventHandler;
    CoverEventHandler *coverEventHandler;
    VolumeControl *volumeControl;
    QTimer elapsedTimer;
    Action *action_Prev_track;
    Action *action_Next_track;
    Action *action_Play_pause_track;
    Action *action_Stop_track;
    Action *action_Increase_volume;
    Action *action_Decrease_volume;
    Action *action_Add_to_playlist;
    Action *action_Replace_playlist;
    Action *action_Load_playlist;
    Action *action_Remove_playlist;
    Action *action_Remove_from_playlist;
    Action *action_Clear_playlist;
    Action *action_Copy_song_info;
    Action *action_Crop_playlist;
    Action *action_Shuffle_playlist;
    Action *action_Rename_playlist;
    Action *action_Save_playlist;
    Action *action_Random_playlist;
    Action *action_Repeat_playlist;
    Action *action_Consume_playlist;
    Action *action_Show_playlist;
    Action *quitAction;
    QList<QAction *> viewActions;
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QMenu *playlistTableViewMenu;
    QPixmap noCover;
    QPoint lastPos;
    UpdateDialog *updateDialog;
    QString mpdDir;
    Song current;
#ifdef ENABLE_KDE_SUPPORT
    bool lyricsNeedUpdating;
#endif

    friend class VolumeSliderEventHandler;
    friend class CoverEventHandler;
};

#endif
