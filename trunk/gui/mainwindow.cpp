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

#include <QtGui>
#include <QSet>
#include <QString>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QClipboard>
#include <cstdlib>

#ifdef ENABLE_KDE_SUPPORT
#include <KApplication>
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#include <KNotification>
#include <KStandardAction>
#include <KAboutApplicationDialog>
#include <KLineEdit>
#include <KXMLGUIFactory>
#include <KMessageBox>
#include <KIcon>
#include "lyrics.h"
#else
#include "aboutdialog.h"
#endif

#include "covers.h"
#include "mainwindow.h"
#include "musiclibraryitemsong.h"
#include "preferencesdialog.h"
#include "lib/mpdconnection.h"
#include "lib/mpdstats.h"
#include "lib/mpdstatus.h"
#include "lib/mpdparseutils.h"
#include "settings.h"
#include "updatedialog.h"
#include "config.h"
#include "musiclibraryitemalbum.h"
#include "folderpage.h"
#include "librarypage.h"
#include "lyricspage.h"
#include "playlistspage.h"
#include "fancytabwidget.h"

VolumeSliderEventHandler::VolumeSliderEventHandler(MainWindow *w)
    : QObject(w), window(w)
{
}

bool VolumeSliderEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        int numDegrees = static_cast<QWheelEvent *>(event)->delta() / 8;
        int numSteps = numDegrees / 15;
        if (numSteps > 0) {
            for (int i = 0; i < numSteps; ++i)
                window->action_Increase_volume->trigger();
        } else {
            for (int i = 0; i > numSteps; --i)
                window->action_Decrease_volume->trigger();
        }
        return true;
    }

    return QObject::eventFilter(obj, event);
}

VolumeControl::VolumeControl(QWidget *parent)
    : QMenu(parent)
{
    QFrame *w = new QFrame(this);
    slider = new QSlider(w);

    QLabel *increase = new QLabel(QLatin1String("+"), w);
    QLabel *decrease = new QLabel(QLatin1String("-"), w);
    increase->setAlignment(Qt::AlignHCenter);
    decrease->setAlignment(Qt::AlignHCenter);

    QVBoxLayout *l = new QVBoxLayout(w);
    l->setMargin(3);
    l->setSpacing(0);
    l->addWidget(increase);
    l->addWidget(slider);
    l->addWidget(decrease);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setMargin(0);
    layout->addWidget(w);
    connect(slider, SIGNAL(valueChanged(int)), SIGNAL(valueChanged(int)));
    slider->setMinimumHeight(192);
    slider->setMaximumHeight(192);
    slider->setOrientation(Qt::Vertical);
    slider->setMinimum(0);
    slider->setMaximum(100);
    slider->setPageStep(5);
    adjustSize();
}

VolumeControl::~VolumeControl()
{
}

void VolumeControl::installSliderEventFilter(QObject *filter)
{
    slider->installEventFilter(filter);
}

void VolumeControl::increaseVolume()
{
    slider->triggerAction(QAbstractSlider::SliderPageStepAdd);
}

void VolumeControl::decreaseVolume()
{
    slider->triggerAction(QAbstractSlider::SliderPageStepSub);
}

void VolumeControl::setValue(int v)
{
    slider->setValue(v);
}

CoverEventHandler::CoverEventHandler(MainWindow *w)
    : QObject(w), window(w)
{
}

bool CoverEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    switch(event->type()) {
    case QEvent::MouseButtonPress:
        if (Qt::LeftButton==static_cast<QMouseEvent *>(event)->button()) {
            pressed=true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (pressed && Qt::LeftButton==static_cast<QMouseEvent *>(event)->button()) {
            window->action_Show_playlist->trigger();
        }
        pressed=false;
        break;
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

#ifdef ENABLE_KDE_SUPPORT
MainWindow::MainWindow(QWidget *parent) : KXmlGuiWindow(parent),
#else
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
#endif
    lastState(MPDStatus::State_Inactive),
    lastSongId(-1),
    fetchStatsFactor(0),
    nowPlayingFactor(0),
    draggingPositionSlider(false)
{
    loaded=0;
    updateDialog = 0;
    trayIcon = 0;
    lyricsNeedUpdating=false;

#ifdef ENABLE_KDE_SUPPORT
    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
#else
    setupUi(this);
#endif
    libraryPage = new LibraryPage(this);
    folderPage = new FolderPage(this);
    playlistsPage = new PlaylistsPage(this);
    lyricsPage = new LyricsPage(this);

    tabWidget->AddTab(libraryPage, QIcon::fromTheme("media-optical-audio"), i18n("Library"));
    tabWidget->AddTab(folderPage, QIcon::fromTheme("inode-directory"), i18n("Folders"));
    tabWidget->AddTab(playlistsPage, QIcon::fromTheme("view-media-playlist"), i18n("Playlists"));
    tabWidget->AddTab(lyricsPage, QIcon::fromTheme("view-media-lyrics"), i18n("Lyrics"));
    tabWidget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    connect(tabWidget, SIGNAL(CurrentChanged(int)), this, SLOT(currentTabChanged(int)));

#ifdef ENABLE_KDE_SUPPORT
    KStandardAction::quit(kapp, SLOT(quit()), actionCollection());
    KStandardAction::preferences(this, SLOT(showPreferencesDialog()), actionCollection());

    KAction *action_Update_database = actionCollection()->addAction("updatedatabase");
    action_Update_database->setText(i18n("Update Database"));

    action_Prev_track = actionCollection()->addAction("prevtrack");
    action_Prev_track->setText(i18n("Previous Track"));
    action_Prev_track->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Left));

    action_Next_track = actionCollection()->addAction("nexttrack");
    action_Next_track->setText(i18n("Next Track"));
    action_Next_track->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Right));

    action_Play_pause_track = actionCollection()->addAction("playpausetrack");
    action_Play_pause_track->setText(i18n("Play/Pause"));
    action_Play_pause_track->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_C));

    action_Stop_track = actionCollection()->addAction("stoptrack");
    action_Stop_track->setText(i18n("Stop"));
    action_Stop_track->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_X));

    action_Increase_volume = actionCollection()->addAction("increasevolume");
    action_Increase_volume->setText(i18n("Increase Volume"));
    action_Increase_volume->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Up));

    action_Decrease_volume = actionCollection()->addAction("decreasevolume");
    action_Decrease_volume->setText(i18n("Decrease Volume"));
    action_Decrease_volume->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Down));

    action_Add_to_playlist = actionCollection()->addAction("addtoplaylist");
    action_Add_to_playlist->setText(i18n("Add To Playlist"));

    action_Replace_playlist = actionCollection()->addAction("replaceplaylist");
    action_Replace_playlist->setText(i18n("Replace Playlist"));

    action_Load_playlist = actionCollection()->addAction("loadplaylist");
    action_Load_playlist->setText(i18n("Load"));

    action_Remove_playlist = actionCollection()->addAction("removeplaylist");
    action_Remove_playlist->setText(i18n("Remove"));

    action_Remove_from_playlist = actionCollection()->addAction("removefromplaylist");
    action_Remove_from_playlist->setText(i18n("Remove"));
    action_Remove_from_playlist->setShortcut(QKeySequence::Delete);

    action_Copy_song_info = actionCollection()->addAction("copysonginfo");
    action_Copy_song_info->setText(i18n("Copy Song Info"));

    action_Crop_playlist = actionCollection()->addAction("cropplaylist");
    action_Crop_playlist->setText(i18n("Crop"));

    action_Shuffle_playlist = actionCollection()->addAction("shuffleplaylist");
    action_Shuffle_playlist->setText(i18n("Shuffle"));

    action_Rename_playlist = actionCollection()->addAction("renameplaylist");
    action_Rename_playlist->setText(i18n("Rename"));

    action_Save_playlist = actionCollection()->addAction("saveplaylist");
    action_Save_playlist->setText(i18n("Save As"));

    action_Clear_playlist = actionCollection()->addAction("clearplaylist");
    action_Clear_playlist->setText(i18n("Clear"));

    action_Show_playlist = actionCollection()->addAction("showplaylist");
    action_Show_playlist->setText(i18n("Show Playlist"));

    action_Random_playlist = actionCollection()->addAction("randomplaylist");
    action_Random_playlist->setText(i18n("Random"));

    action_Repeat_playlist = actionCollection()->addAction("repeatplaylist");
    action_Repeat_playlist->setText(i18n("Repeat"));

    action_Consume_playlist = actionCollection()->addAction("consumeplaylist");
    action_Consume_playlist->setText(i18n("Consume"));
#else
    action_Prev_track = new QAction(tr("Previous Track"), this);
    action_Next_track = new QAction(tr("Next Track"), this);
    action_Play_pause_track = new QAction(tr("Play/Pause"), this);
    action_Stop_track = new QAction(tr("Stop"), this);
    action_Increase_volume = new QAction(tr("Increase Volume"), this);
    action_Decrease_volume = new QAction(tr("Decrease Volume"), this);
    action_Add_to_playlist = new QAction(tr("Add To Playlist"), this);
    action_Replace_playlist = new QAction(tr("Replace Playlist"), this);
    action_Load_playlist = new QAction(tr("Load"), this);
    action_Remove_playlist = new QAction(tr("Remove"), this);
    action_Remove_from_playlist = new QAction(tr("Remove"), this);
    action_Remove_from_playlist->setShortcut(QKeySequence::Delete);
    action_Copy_song_info = new QAction(tr("Copy Song Info"), this);
    action_Copy_song_info->setShortcuts(QKeySequence::Copy);
    action_Crop_playlist = new QAction(tr("Crop"), this);
    action_Shuffle_playlist = new QAction(tr("Shuffle"), this);
    action_Rename_playlist = new QAction(tr("Rename"), this);
    action_Save_playlist = new QAction(tr("Save As"), this);
    action_Clear_playlist = new QAction(tr("Clear"), this);
    action_Show_playlist = new QAction(tr("Show Playlist"), this);
    action_Random_playlist = new QAction(tr("Random"), this);
    action_Repeat_playlist = new QAction(tr("Repeat"), this);
    action_Consume_playlist = new QAction(tr("Consume"), this);
#endif

    setVisible(true);

    // Setup event handler for volume adjustment
    volumeSliderEventHandler = new VolumeSliderEventHandler(this);
    coverEventHandler = new CoverEventHandler(this);

    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(volumeSliderEventHandler);
// KDE does this for us
//#ifndef ENABLE_KDE_SUPPORT
//    setWindowIcon(QIcon(":/icons/icon.svg"));
//#endif
    noCover = QIcon::fromTheme("media-optical-audio").pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverWidget->setPixmap(noCover);

    playbackPlay = QIcon::fromTheme("media-playback-start");
    playbackPause = QIcon::fromTheme("media-playback-pause");
// #ifdef ENABLE_KDE_SUPPORT
//    action_Random_playlist->setIcon(KIcon("media-random-tracks"));
//    action_Repeat_playlist->setIcon(KIcon("media-repeat-playlist"));
//     action_Consume_playlist->setIcon(KIcon("media-consume-playlist"));
//     action_Add_to_playlist->setIcon(KIcon("media-track-add"));
//     action_Replace_playlist->setIcon(KIcon("media-replace-playlist"));
// #else
//    action_Random_playlist->setIcon(QIcon(":/icons/hi16-action-media-random-tracks.png"));
//    action_Repeat_playlist->setIcon(QIcon(":/icons/hi16-action-media-repeat-playlist.png"));
//     action_Consume_playlist->setIcon(QIcon(":/icons/hi16-action-media-consume-playlist.png"));
//     action_Add_to_playlist->setIcon(QIcon(":/icons/hi16-action-media-track-add.png"));
//     action_Replace_playlist->setIcon(QIcon(":/icons/hi16-action-media-replace-playlist.png"));
// #endif
    action_Repeat_playlist->setIcon(QIcon::fromTheme("edit-redo"));
    action_Random_playlist->setIcon(QIcon::fromTheme("roll"));
    action_Consume_playlist->setIcon(QIcon::fromTheme("format-list-unordered"));
    action_Add_to_playlist->setIcon(QIcon::fromTheme(Qt::RightToLeft==QApplication::layoutDirection() ? "arrow-left" : "arrow-right"));
    action_Replace_playlist->setIcon(QIcon::fromTheme(Qt::RightToLeft==QApplication::layoutDirection() ? "arrow-left-double" : "arrow-right-double"));
    action_Prev_track->setIcon(QIcon::fromTheme("media-skip-backward"));
    action_Next_track->setIcon(QIcon::fromTheme("media-skip-forward"));
    action_Play_pause_track->setIcon(playbackPlay);
    action_Stop_track->setIcon(QIcon::fromTheme("media-playback-stop"));
    action_Load_playlist->setIcon(QIcon::fromTheme("document-open"));
    action_Remove_playlist->setIcon(QIcon::fromTheme("edit-delete"));
    action_Remove_from_playlist->setIcon(QIcon::fromTheme("list-remove"));
    action_Clear_playlist->setIcon(QIcon::fromTheme("edit-clear-list"));
    action_Save_playlist->setIcon(QIcon::fromTheme("document-save-as"));
    action_Clear_playlist->setIcon(QIcon::fromTheme("edit-clear-list"));
    action_Show_playlist->setIcon(QIcon::fromTheme("view-media-playlist"));
    action_Update_database->setIcon(QIcon::fromTheme("view-refresh"));

    volumeButton->setIcon(QIcon::fromTheme("player-volume"));
    connect(Lyrics::self(), SIGNAL(lyrics(const QString &, const QString &, const QString &)),
            SLOT(lyrics(const QString &, const QString &, const QString &)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            SLOT(cover(const QString &, const QString &, const QImage &)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            &musicLibraryModel, SLOT(setCover(const QString &, const QString &, const QImage &)));

    playPauseTrackButton->setDefaultAction(action_Play_pause_track);
    stopTrackButton->setDefaultAction(action_Stop_track);
    nextTrackButton->setDefaultAction(action_Next_track);
    prevTrackButton->setDefaultAction(action_Prev_track);
    libraryPage->addToPlaylistPushButton->setDefaultAction(action_Add_to_playlist);
    libraryPage->replacePlaylistPushButton->setDefaultAction(action_Replace_playlist);
    folderPage->dirViewAddToPlaylistPushButton->setDefaultAction(action_Add_to_playlist);
    folderPage->dirViewReplacePlaylistPushButton->setDefaultAction(action_Replace_playlist);
    savePlaylistPushButton->setDefaultAction(action_Save_playlist);
    removeAllFromPlaylistPushButton->setDefaultAction(action_Clear_playlist);
    removeFromPlaylistPushButton->setDefaultAction(action_Remove_from_playlist);
    playlistsPage->addPlaylistToPlaylistPushButton->setDefaultAction(action_Add_to_playlist);
    playlistsPage->loadPlaylistPushButton->setDefaultAction(action_Load_playlist);
    playlistsPage->removePlaylistPushButton->setDefaultAction(action_Remove_playlist);
    randomPushButton->setDefaultAction(action_Random_playlist);
    repeatPushButton->setDefaultAction(action_Repeat_playlist);
    consumePushButton->setDefaultAction(action_Consume_playlist);
    libraryPage->libraryUpdateButton->setDefaultAction(action_Update_database);
    folderPage->libraryUpdateButton2->setDefaultAction(action_Update_database);
    playlistsPage->libraryUpdateButton3->setDefaultAction(action_Update_database);

    action_Show_playlist->setCheckable(true);
    action_Random_playlist->setCheckable(true);
    action_Repeat_playlist->setCheckable(true);
    action_Consume_playlist->setCheckable(true);

    libraryPage->addToPlaylistPushButton->setEnabled(false);
    libraryPage->replacePlaylistPushButton->setEnabled(false);
    folderPage->dirViewAddToPlaylistPushButton->setEnabled(false);
    folderPage->dirViewReplacePlaylistPushButton->setEnabled(false);
    playlistsPage->addPlaylistToPlaylistPushButton->setEnabled(false);
    playlistsPage->loadPlaylistPushButton->setEnabled(false);
    playlistsPage->removePlaylistPushButton->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    libraryPage->searchLibraryLineEdit->setPlaceholderText(i18n("Search library..."));
    folderPage->searchDirViewLineEdit->setPlaceholderText(i18n("Search files..."));
    searchPlaylistLineEdit->setPlaceholderText(i18n("Search playlist..."));
#endif
    QList<QToolButton *> btns;
    btns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton
         << libraryPage->addToPlaylistPushButton << libraryPage->replacePlaylistPushButton
         << folderPage->dirViewAddToPlaylistPushButton << folderPage->dirViewReplacePlaylistPushButton << playlistsPage->addPlaylistToPlaylistPushButton
         << playlistsPage->loadPlaylistPushButton << playlistsPage->removePlaylistPushButton << repeatPushButton << randomPushButton
         << savePlaylistPushButton << removeAllFromPlaylistPushButton << removeFromPlaylistPushButton
         << consumePushButton << libraryPage->libraryUpdateButton << folderPage->libraryUpdateButton2 << playlistsPage->libraryUpdateButton3 << volumeButton;

    foreach(QToolButton * b, btns) {
        b->setAutoRaise(true);
    }

    trackLabel->setText(i18n("Stopped"));
    artistLabel->setText("...");

    playlistsProxyModel.setSourceModel(&playlistsModel);
    playlistsPage->playlistsView->setModel(&playlistsProxyModel);
    playlistsPage->playlistsView->sortByColumn(0, Qt::AscendingOrder);
    playlistsPage->playlistsView->addAction(action_Add_to_playlist);
    playlistsPage->playlistsView->addAction(action_Load_playlist);
    playlistsPage->playlistsView->addAction(action_Remove_playlist);
    playlistsPage->playlistsView->addAction(action_Rename_playlist);

    libraryPage->libraryTreeView->setHeaderHidden(true);
    folderPage->dirTreeView->setHeaderHidden(true);
    playlistsPage->playlistsView->setHeaderHidden(true);

    action_Show_playlist->setChecked(Settings::self()->showPlaylist());
    action_Random_playlist->setChecked(Settings::self()->randomPlaylist());
    action_Repeat_playlist->setChecked(Settings::self()->repeatPlaylist());
    action_Consume_playlist->setChecked(Settings::self()->consumePlaylist());
    mpdDir=Settings::self()->mpdDir();
    Lyrics::self()->setMpdDir(mpdDir);
    Covers::self()->setMpdDir(mpdDir);
    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->coverSize());

    if (setupTrayIcon() && Settings::self()->useSystemTray())
        trayIcon->show();

    // Start connection threads
    MPDConnection::self()->start();

    // Set connection data
    MPDConnection::self()->setDetails(Settings::self()->connectionHost(), Settings::self()->connectionPort(), Settings::self()->connectionPasswd());

    while (!MPDConnection::self()->connectToMPD()) {
        if (!showPreferencesDialog())
            exit(EXIT_FAILURE);

        MPDConnection::self()->setDetails(Settings::self()->connectionHost(), Settings::self()->connectionPort(), Settings::self()->connectionPasswd());
        qWarning("Retrying");
    }

    togglePlaylist();
    setRandom();
    setRepeat();
    setConsume();
#ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys | KXmlGuiWindow::Save | KXmlGuiWindow::Create);
#else
    QSize sz=Settings::self()->mainWindowSize();
    if (!sz.isEmpty()) {
        resize(sz);
    }
#endif

    coverWidget->installEventFilter(coverEventHandler);
    libraryProxyModel.setSourceModel(&musicLibraryModel);
    libraryPage->libraryTreeView->setModel(&libraryProxyModel);
    libraryPage->libraryTreeView->addAction(action_Add_to_playlist);
    libraryPage->libraryTreeView->addAction(action_Replace_playlist);

    dirProxyModel.setSourceModel(&dirviewModel);
    folderPage->dirTreeView->setModel(&dirProxyModel);
    folderPage->dirTreeView->addAction(action_Add_to_playlist);
    folderPage->dirTreeView->addAction(action_Replace_playlist);

    connect(&playlistsModel, SIGNAL(playlistLoaded()), this, SLOT(startPlayingSong()));
    connect(&playlistModel, SIGNAL(filesAddedInPlaylist(const QStringList, const int, const int)), this,
            SLOT(addFilenamesToPlaylist(const QStringList, const int, const int)));
    connect(&playlistModel, SIGNAL(moveInPlaylist(const QList<quint32>, const int, const int)), this,
            SLOT(movePlaylistItems(const QList<quint32>, const int, const int)));
    connect(&playlistModel, SIGNAL(playListStatsUpdated()), this,
            SLOT(updatePlayListStatus()));

    playlistProxyModel.setSourceModel(&playlistModel);
    playlistTableView->setModel(&playlistProxyModel);
    playlistTableView->setAcceptDrops(true);
    playlistTableView->setDropIndicatorShown(true);
    playlistTableView->addAction(action_Remove_from_playlist);
    playlistTableView->addAction(action_Clear_playlist);
    playlistTableView->addAction(action_Crop_playlist);
    playlistTableView->addAction(action_Shuffle_playlist);
    playlistTableView->addAction(action_Copy_song_info);
    connect(playlistTableView, SIGNAL(itemsSelected(bool)), SLOT(playlistItemsSelected(bool)));
    setupPlaylistViewHeader();
    setupPlaylistViewMenu();

    connect(MPDConnection::self(), SIGNAL(statsUpdated()), this, SLOT(updateStats()));
    connect(MPDConnection::self(), SIGNAL(statusUpdated()), this, SLOT(updateStatus()), Qt::DirectConnection);
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlaylist(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(mpdConnectionDied()), this, SLOT(mpdConnectionDied()));
    connect(MPDConnection::self(), SIGNAL(musicLibraryUpdated(MusicLibraryItemRoot *, QDateTime)), &musicLibraryModel, SLOT(updateMusicLibrary(MusicLibraryItemRoot *, QDateTime)));
    connect(MPDConnection::self(), SIGNAL(dirViewUpdated(DirViewItemRoot *)), &dirviewModel, SLOT(updateDirView(DirViewItemRoot *)));
#ifndef ENABLE_KDE_SUPPORT
    connect(action_Quit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(action_Preferences, SIGNAL(triggered(bool)), this, SLOT(showPreferencesDialog()));
    connect(action_About, SIGNAL(triggered(bool)), this, SLOT(showAboutDialog()));
#endif
    connect(action_Update_database, SIGNAL(triggered(bool)), this, SLOT(updateDb()));
    connect(action_Prev_track, SIGNAL(triggered(bool)), this, SLOT(previousTrack()));
    connect(action_Next_track, SIGNAL(triggered(bool)), this, SLOT(nextTrack()));
    connect(action_Play_pause_track, SIGNAL(triggered(bool)), this, SLOT(playPauseTrack()));
    connect(action_Stop_track, SIGNAL(triggered(bool)), this, SLOT(stopTrack()));
    connect(volumeControl, SIGNAL(valueChanged(int)), SLOT(setVolume(int)));
    connect(action_Increase_volume, SIGNAL(triggered(bool)), this, SLOT(increaseVolume()));
    connect(action_Decrease_volume, SIGNAL(triggered(bool)), this, SLOT(decreaseVolume()));
    connect(action_Increase_volume, SIGNAL(triggered(bool)), volumeControl, SLOT(increaseVolume()));
    connect(action_Decrease_volume, SIGNAL(triggered(bool)), volumeControl, SLOT(decreaseVolume()));
    connect(positionSlider, SIGNAL(sliderPressed()), this, SLOT(positionSliderPressed()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(positionSliderReleased()));
    connect(action_Random_playlist, SIGNAL(activated()), this, SLOT(setRandom()));
    connect(action_Repeat_playlist, SIGNAL(activated()), this, SLOT(setRepeat()));
    connect(action_Consume_playlist, SIGNAL(activated()), this, SLOT(setConsume()));
    connect(libraryPage->searchLibraryLineEdit, SIGNAL(returnPressed()), this, SLOT(searchMusicLibrary()));
    connect(libraryPage->searchLibraryLineEdit, SIGNAL(textChanged(const QString)), this, SLOT(searchMusicLibrary()));
    connect(libraryPage->genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchMusicLibrary()));
    connect(folderPage->searchDirViewLineEdit, SIGNAL(returnPressed()), this, SLOT(searchDirView()));
    connect(folderPage->searchDirViewLineEdit, SIGNAL(textChanged(const QString)), this, SLOT(searchDirView()));
    connect(searchPlaylistLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPlaylist()));
    connect(searchPlaylistLineEdit, SIGNAL(textChanged(const QString)), this, SLOT(searchPlaylist()));
    connect(playlistTableView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playlistItemActivated(const QModelIndex &)));
    connect(libraryPage->libraryTreeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(libraryItemActivated(const QModelIndex &)));
    connect(folderPage->dirTreeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(dirViewItemActivated(const QModelIndex &)));
    connect(action_Add_to_playlist, SIGNAL(activated()), this, SLOT(addToPlaylistItemActivated()));
    connect(action_Replace_playlist, SIGNAL(activated()), this, SLOT(replacePlaylist()));
    connect(action_Load_playlist, SIGNAL(activated()), this, SLOT(loadPlaylistPushButtonActivated()));
    connect(action_Remove_playlist, SIGNAL(activated()), this, SLOT(removePlaylistPushButtonActivated()));
    connect(action_Save_playlist, SIGNAL(activated()), this, SLOT(savePlaylistPushButtonActivated()));
    connect(action_Remove_from_playlist, SIGNAL(activated()), this, SLOT(removeFromPlaylist()));
    connect(action_Rename_playlist, SIGNAL(triggered()), this, SLOT(renamePlaylistActivated()));
    connect(action_Clear_playlist, SIGNAL(activated()), this, SLOT(clearPlaylist()));
    connect(action_Copy_song_info, SIGNAL(activated()), this, SLOT(copySongInfo()));
    connect(action_Crop_playlist, SIGNAL(activated()), this, SLOT(cropPlaylist()));
    connect(action_Shuffle_playlist, SIGNAL(activated()), MPDConnection::self(), SLOT(shuffle()));
    connect(action_Show_playlist, SIGNAL(activated()), this, SLOT(togglePlaylist()));
    connect(&elapsedTimer, SIGNAL(timeout()), this, SLOT(updatePositionSilder()));
    connect(&musicLibraryModel, SIGNAL(updateGenres(const QStringList &)), this, SLOT(updateGenres(const QStringList &)));
    connect(libraryPage->libraryTreeView, SIGNAL(itemsSelected(bool)), libraryPage->addToPlaylistPushButton, SLOT(setEnabled(bool)));
    connect(libraryPage->libraryTreeView, SIGNAL(itemsSelected(bool)), libraryPage->replacePlaylistPushButton, SLOT(setEnabled(bool)));
    connect(folderPage->dirTreeView, SIGNAL(itemsSelected(bool)), folderPage->dirViewAddToPlaylistPushButton, SLOT(setEnabled(bool)));
    connect(folderPage->dirTreeView, SIGNAL(itemsSelected(bool)), folderPage->dirViewReplacePlaylistPushButton, SLOT(setEnabled(bool)));
    connect(playlistsPage->playlistsView, SIGNAL(itemsSelected(bool)), playlistsPage->addPlaylistToPlaylistPushButton, SLOT(setEnabled(bool)));
    connect(playlistsPage->playlistsView, SIGNAL(itemsSelected(bool)), playlistsPage->loadPlaylistPushButton, SLOT(setEnabled(bool)));
    connect(playlistsPage->playlistsView, SIGNAL(itemsSelected(bool)), playlistsPage->removePlaylistPushButton, SLOT(setEnabled(bool)));
    connect(volumeButton, SIGNAL(clicked()), SLOT(showVolumeControl()));

    connect(playlistsPage->addPlaylistToPlaylistPushButton, SIGNAL(clicked(bool)), this, SLOT(addPlaylistToPlaylistPushButtonActivated()));
    connect(playlistsPage->playlistsView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playlistsViewItemDoubleClicked(const QModelIndex &)));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), this, SLOT(updateStoredPlaylists()));

    elapsedTimer.setInterval(1000);

    splitter->restoreState(Settings::self()->splitterState());

    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutpus(const QList<Output> &)));
#ifdef ENABLE_KDE_SUPPORT
    QMenu *m = (QMenu *)factory()->container("output_menu", this);
    connect(m, SIGNAL(triggered(QAction *)), this, SLOT(outputChanged(QAction *)));
#else
    connect(menu_Outputs, SIGNAL(triggered(QAction *)), this, SLOT(outputChanged(QAction *)));
#endif

    MPDConnection::self()->outputs();
    MPDConnection::self()->getStatus();
    MPDConnection::self()->getStats();
    MPDConnection::self()->playListInfo();

    currentTabChanged(0);
    playlistItemsSelected(false);
}

MainWindow::~MainWindow()
{
#ifndef ENABLE_KDE_SUPPORT
    Settings::self()->saveMainWindowSize(size());
#endif
    Settings::self()->saveConsumePlaylist(action_Consume_playlist->isChecked());
    Settings::self()->saveRandomPlaylist(action_Random_playlist->isChecked());
    Settings::self()->saveRepeatPlaylist(action_Repeat_playlist->isChecked());
    Settings::self()->saveShowPlaylist(action_Show_playlist->isChecked());
    Settings::self()->saveSplitterState(splitter->saveState());
    Settings::self()->savePlaylistHeaderState(playlistTableViewHeader->saveState());
    Settings::self()->save();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayIcon != NULL && trayIcon->isVisible())
    {
        lastPos=pos();
        hide();
    }
    else
        qApp->quit();

    event->ignore();
}

void MainWindow::showVolumeControl()
{
//     qWarning() << volumeControl->pos().x() << volumeControl->pos().y()
//     << volumeButton->pos().x() << volumeButton->pos().y()
//     << volumeButton->mapToGlobal(volumeButton->pos()).x() << volumeButton->mapToGlobal(volumeButton->pos()).y();
//     volumeControl->move();
    volumeControl->popup(volumeButton->mapToGlobal(QPoint((volumeButton->width()-volumeControl->width())/2, volumeButton->height())));
}

void MainWindow::playlistItemsSelected(bool s)
{
    if (playlistTableView->model()->rowCount()) {
        playlistTableView->setContextMenuPolicy(Qt::ActionsContextMenu);
        action_Remove_from_playlist->setEnabled(s);
        action_Copy_song_info->setEnabled(s);
        action_Clear_playlist->setEnabled(true);
        action_Crop_playlist->setEnabled(playlistTableView->haveUnSelectedItems());
        action_Shuffle_playlist->setEnabled(true);
    }
}

void MainWindow::mpdConnectionDied()
{
    // Stop timer(s)
    elapsedTimer.stop();

    // Reset GUI
    positionSlider->setValue(0);
    songTimeElapsedLabel->setText("00:00 / 00:00");

#ifndef ENABLE_KDE_SUPPORT
    action_Play_pause_track->setText(i18n("&Play"));
#endif
    action_Play_pause_track->setIcon(playbackPlay);
    action_Play_pause_track->setEnabled(true);
    action_Stop_track->setEnabled(false);

    // Show warning message
#ifdef ENABLE_KDE_SUPPORT
    KMessageBox::error(this, i18n("The MPD connection died unexpectedly.\nThis error is unrecoverable, please restart %1.").arg(PACKAGE_NAME),
                       i18n("Lost MPD Connection"));
#else
    QMessageBox::critical(this, "MPD connection gone", "The MPD connection died unexpectedly. This error is unrecoverable, please restart "PACKAGE_NAME".");
#endif
}

void MainWindow::updateDb()
{
    MPDConnection::self()->update();
    if (!updateDialog) {
        updateDialog = new UpdateDialog(this);
        connect(MPDConnection::self(), SIGNAL(databaseUpdated()), updateDialog, SLOT(complete()));
    }
    updateDialog->show();
}

int MainWindow::showPreferencesDialog()
{
    PreferencesDialog pref(this);
    if (MPDConnection::self()->isConnected()) {
        if (trayIcon != NULL)
            connect(&pref, SIGNAL(systemTraySet(bool)), this, SLOT(toggleTrayIcon(bool)));
        connect(&pref, SIGNAL(crossfadingChanged(const int)), this, SLOT(crossfadingChanged(const int)));
    }

    int rv=pref.exec();
    if (rv) {
        mpdDir=Settings::self()->mpdDir();
        if (!mpdDir.endsWith("/")) {
            mpdDir=mpdDir+"/";
        }
        Lyrics::self()->setMpdDir(mpdDir);
        Covers::self()->setMpdDir(mpdDir);
    }
    return rv;
}

void MainWindow::toggleTrayIcon(bool enable)
{
    if (enable) {
        if (!trayIcon) {
            setupTrayIcon();
        }

        if (trayIcon) {
            trayIcon->show();
        }
    } else if (trayIcon) {
        trayIcon->hide();
    }
}

#ifndef ENABLE_KDE_SUPPORT
void MainWindow::showAboutDialog()
{
    AboutDialog about(this);
    about.exec();
}
#endif

void MainWindow::positionSliderPressed()
{
    draggingPositionSlider = true;
}

void MainWindow::positionSliderReleased()
{
    draggingPositionSlider = false;
}

void MainWindow::startPlayingSong()
{
    MPDConnection::self()->startPlayingSong();
}

void MainWindow::nextTrack()
{
    MPDConnection::self()->goToNext();
}

void MainWindow::stopTrack()
{
    MPDConnection::self()->stopPlaying();

    action_Stop_track->setEnabled(false);
#ifndef ENABLE_KDE_SUPPORT
    action_Play_pause_track->setText(i18n("&Play"));
#endif
}

void MainWindow::playPauseTrack()
{
    MPDStatus * const status = MPDStatus::self();

    if (status->state() == MPDStatus::State_Playing)
        MPDConnection::self()->setPause(true);
    else if (status->state() == MPDStatus::State_Paused)
        MPDConnection::self()->setPause(false);
    else
        MPDConnection::self()->startPlayingSong();

}

void MainWindow::previousTrack()
{
    MPDConnection::self()->goToPrevious();
}

void MainWindow::setPosition()
{
    MPDConnection::self()->setSeekId(MPDStatus::self()->songId(), positionSlider->value());
}

void MainWindow::setVolume(int vol)
{
    MPDConnection::self()->setVolume(vol);
}

void MainWindow::increaseVolume()
{
    volumeControl->sliderWidget()->triggerAction(QAbstractSlider::SliderPageStepAdd);
}

void MainWindow::decreaseVolume()
{
    volumeControl->sliderWidget()->triggerAction(QAbstractSlider::SliderPageStepSub);
}

void MainWindow::setRandom()
{
    MPDConnection::self()->setRandom(action_Random_playlist->isChecked());
}

void MainWindow::setRepeat()
{
    MPDConnection::self()->setRepeat(action_Repeat_playlist->isChecked());
}

void MainWindow::setConsume()
{
    MPDConnection::self()->setConsume(action_Consume_playlist->isChecked());
}

void MainWindow::searchMusicLibrary()
{
    libraryProxyModel.setFilterGenre(0==libraryPage->genreCombo->currentIndex() ? QString() : libraryPage->genreCombo->currentText());

    if (libraryPage->searchLibraryLineEdit->text().isEmpty()) {
        libraryProxyModel.setFilterRegExp(QString());
        return;
    }

    libraryProxyModel.setFilterRegExp(libraryPage->searchLibraryLineEdit->text());
}

void MainWindow::searchDirView()
{
    if (folderPage->searchDirViewLineEdit->text().isEmpty()) {
        dirProxyModel.setFilterRegExp("");
        return;
    }

    dirProxyModel.setFilterRegExp(folderPage->searchDirViewLineEdit->text());
}

void MainWindow::searchPlaylist()
{
    if (searchPlaylistLineEdit->text().isEmpty()) {
        playlistProxyModel.setFilterRegExp("");
        return;
    }

    playlistProxyModel.setFilterRegExp(searchPlaylistLineEdit->text());
}

void MainWindow::updatePlaylist(const QList<Song> &songs)
{
    QList<qint32> selectedSongIds;
    qint32 firstSelectedSongId = -1;
    qint32 firstSelectedRow = -1;

    // remember selected song ids and rownum of smallest selected row in proxymodel (because that represents the visible rows)
    if (playlistTableView->selectionModel()->hasSelection()) {
        QModelIndexList items = playlistTableView->selectionModel()->selectedRows();
        QModelIndex index;
        QModelIndex sourceIndex;
        // find smallest selected rownum
        for (int i = 0; i < items.size(); i++) {
            index = items.at(i);
            sourceIndex = playlistProxyModel.mapToSource(index);
            selectedSongIds.append(playlistModel.getIdByRow(sourceIndex.row()));
            if (firstSelectedRow == -1 || index.row() < firstSelectedRow) {
                firstSelectedRow = index.row();
                firstSelectedSongId = playlistModel.getIdByRow(sourceIndex.row());
            }
        }
    }

    // refresh playlist
    playlistModel.updatePlaylist(songs);

    // reselect song ids or minrow if songids were not found (songs removed)
    bool found =  false;
    if (selectedSongIds.size() > 0) {
        qint32 newCurrentRow = playlistModel.getRowById(firstSelectedSongId);
        QModelIndex newCurrentSourceIndex = playlistModel.index(newCurrentRow, 0);
        QModelIndex newCurrentIndex = playlistProxyModel.mapFromSource(newCurrentSourceIndex);
        playlistTableView->setCurrentIndex(newCurrentIndex);

        qint32 row;
        QModelIndex sourceIndex;
        QModelIndex index;
        for (int i = 0; i < selectedSongIds.size(); i++) {
            row = playlistModel.getRowById(selectedSongIds.at(i));
            if (row >= 0) {
                found = true;
            }
            sourceIndex = playlistModel.index(row, 0);
            index = playlistProxyModel.mapFromSource(sourceIndex);
            playlistTableView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        if (!found) {
            // songids which were selected before the playlistupdate were not found anymore (they were removed) -> select firstSelectedRow again (should be the next song after the removed one)
            // firstSelectedRow contains the selected row of the proxymodel before we did the playlist refresh
            // check if rowcount of current proxymodel is smaller than that (last row as removed) and adjust firstSelectedRow when needed
            index = playlistProxyModel.index(firstSelectedRow, 0);
            if (firstSelectedRow > playlistProxyModel.rowCount(index) - 1) {
                firstSelectedRow = playlistProxyModel.rowCount(index) - 1;
            }
            index = playlistProxyModel.index(firstSelectedRow, 0);
            playlistTableView->setCurrentIndex(index);
            playlistTableView->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
}

void MainWindow::updateCurrentSong(const Song &song)
{
    current=song;

    // Determine if album cover should be updated
    const QString &albumArtist=song.albumArtist();
    if (coverWidget->property("artist").toString() != albumArtist || coverWidget->property("album").toString() != song.album) {
        Covers::self()->get(song);
        coverWidget->setProperty("artist", albumArtist);
        coverWidget->setProperty("album", song.album);
    }

    if (song.title.isEmpty() && albumArtist.isEmpty()) {
        trackLabel->setText(i18n("Stopped"));
        artistLabel->setText("...");
    } else {
        trackLabel->setText(song.title);
        artistLabel->setText(song.artist);
    }

    playlistModel.updateCurrentSong(song.id);

    if (3==tabWidget->current_index()) {
        Lyrics::self()->get(song);
    } else {
        lyricsNeedUpdating=true;
    }

    if (Settings::self()->showPopups()) { //(trayIcon && trayIcon->isVisible() && isHidden()) {
        if (!song.title.isEmpty() && !song.artist.isEmpty() && !song.album.isEmpty()) {
#ifdef ENABLE_KDE_SUPPORT
            const QString text(i18n("<table>"
                                    "<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                                    "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                                    "<tr><td align=\"right\"><b>Song:</b></td><td>%3</td></tr>"
                                    "<tr><td align=\"right\"><b>Track:</b></td><td>%4</td></tr>"
                                    "<tr><td align=\"right\"><b>Length:</b></td><td>%5</td></tr>"
                                    "</table>").arg(song.artist).arg(song.album).arg(song.title)
                                               .arg(song.track).arg(Song::formattedTime(song.time)));

            KNotification *notification = new KNotification("CurrentTrackChanged", this);
            notification->setText(text);
            notification->sendEvent();
#else
            const QString text("album:  " + song.album + "\n"
                            + "track:  " + QString::number(song.track) + "\n"
                            + "length: " + Song::formattedTime(song.time));

            trayIcon->showMessage(song.artist + " - " + song.title, text, QSystemTrayIcon::Information, 5000);
#endif
        }
    }
}

void MainWindow::updateStats()
{
    MPDStats * const stats = MPDStats::self();

    /*
     * Check if remote db is more recent than local one
     * Also update the dirview
     */
    if (lastDbUpdate.isValid() && stats->dbUpdate() > lastDbUpdate) {
        MPDConnection::self()->listAllInfo(stats->dbUpdate());
        MPDConnection::self()->listAll();
    }

    lastDbUpdate = stats->dbUpdate();
}

void MainWindow::updateStatus()
{
    MPDStatus * const status = MPDStatus::self();
    QString timeElapsedFormattedString;

    if (!draggingPositionSlider) {
        if (status->state() == MPDStatus::State_Stopped
                || status->state() == MPDStatus::State_Inactive) {
            positionSlider->setValue(0);
        } else {
            positionSlider->setRange(0, status->timeTotal());
            positionSlider->setValue(status->timeElapsed());
        }
    }

    volumeButton->setToolTip(i18n("Volume %1%").arg(status->volume()));
    volumeControl->setToolTip(i18n("Volume %1%").arg(status->volume()));
    volumeControl->setValue(status->volume());

    randomPushButton->setChecked(status->random());
    repeatPushButton->setChecked(status->repeat());
    consumePushButton->setChecked(status->consume());

    if (status->state() == MPDStatus::State_Stopped || status->state() == MPDStatus::State_Inactive) {
        timeElapsedFormattedString = "00:00 / 00:00";
    } else {
        timeElapsedFormattedString += Song::formattedTime(status->timeElapsed());
        timeElapsedFormattedString += " / ";
        timeElapsedFormattedString += Song::formattedTime(status->timeTotal());
        songTime = status->timeTotal();
    }

    songTimeElapsedLabel->setText(timeElapsedFormattedString);

    switch (status->state()) {
    case MPDStatus::State_Playing:
#ifndef ENABLE_KDE_SUPPORT
        action_Play_pause_track->setText(i18n("&Pause"));
#endif
        action_Play_pause_track->setIcon(playbackPause);
        action_Play_pause_track->setEnabled(true);
        //playPauseTrackButton->setChecked(false);
        action_Stop_track->setEnabled(true);
        elapsedTimer.start();

        if (trayIcon != NULL)
            trayIcon->setIcon(playbackPlay);

        break;
    case MPDStatus::State_Inactive:
    case MPDStatus::State_Stopped:
#ifndef ENABLE_KDE_SUPPORT
        action_Play_pause_track->setText(i18n("&Play"));
#endif
        action_Play_pause_track->setIcon(playbackPlay);
        action_Play_pause_track->setEnabled(true);
        action_Stop_track->setEnabled(false);

        trackLabel->setText(i18n("Stopped"));
        artistLabel->setText("...");

        if (trayIcon != NULL)
            trayIcon->setIcon(windowIcon());

        elapsedTimer.stop();
        break;
    case MPDStatus::State_Paused:
#ifndef ENABLE_KDE_SUPPORT
        action_Play_pause_track->setText("&Play");
#endif
        action_Play_pause_track->setIcon(playbackPlay);
        action_Play_pause_track->setEnabled(true);
        action_Stop_track->setEnabled(true);

        if (trayIcon != NULL)
            trayIcon->setIcon(playbackPause);

        elapsedTimer.stop();

        break;
    default:
        qDebug("Invalid state");
        break;
    }

    // Check if song has changed or we're playing again after being stopped
    // and update song info if needed
    if (lastState == MPDStatus::State_Inactive
            || (lastState == MPDStatus::State_Stopped && status->state() == MPDStatus::State_Playing)
            || lastSongId != status->songId())
        MPDConnection::self()->currentSong();
    // Update status info
    lastState = status->state();
    lastSongId = status->songId();
    lastPlaylist = status->playlist();
}

void MainWindow::playlistItemActivated(const QModelIndex &index)
{
    QModelIndex sourceIndex = playlistProxyModel.mapToSource(index);
    MPDConnection::self()->startPlayingSongId(playlistModel.getIdByRow(sourceIndex.row()));
}

void MainWindow::clearPlaylist()
{
    MPDConnection::self()->clear();
    searchPlaylistLineEdit->clear();
}

void MainWindow::removeFromPlaylist()
{
    const QModelIndexList items = playlistTableView->selectionModel()->selectedRows();
    QModelIndex sourceIndex;
    QList<qint32> toBeRemoved;

    if (items.isEmpty())
        return;

    for (int i = 0; i < items.size(); i++) {
        sourceIndex = playlistProxyModel.mapToSource(items.at(i));
        toBeRemoved.append(playlistModel.getIdByRow(sourceIndex.row()));
    }

    MPDConnection::self()->removeSongs(toBeRemoved);
}
void MainWindow::replacePlaylist()
{
    MPDConnection::self()->clear();
    MPDConnection::self()->getStatus();
    searchPlaylistLineEdit->clear();
    if (libraryPage->libraryTreeView->isVisible()) {
        addSelectionToPlaylist();
    } else if (folderPage->dirTreeView->isVisible()) {
        addDirViewSelectionToPlaylist();
    }
}

void MainWindow::addSelectionToPlaylist()
{
    QStringList files;
    MusicLibraryItem *item;
    MusicLibraryItemSong *songItem;

    // Get selected view indexes
    const QModelIndexList selected = libraryPage->libraryTreeView->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();

    if (selectionSize == 0) {
        return;
    }

    // Loop over the selection. Only add files.
    for (int selectionPos = 0; selectionPos < selectionSize; selectionPos++) {
        const QModelIndex current = selected.at(selectionPos);
        item = static_cast<MusicLibraryItem *>(libraryProxyModel.mapToSource(current).internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Artist: {
            for (quint32 i = 0; ; i++) {
                const QModelIndex album = current.child(i , 0);
                if (!album.isValid())
                    break;

                for (quint32 j = 0; ; j++) {
                    const QModelIndex track = album.child(j, 0);
                    if (!track.isValid())
                        break;
                    const QModelIndex mappedSongIndex = libraryProxyModel.mapToSource(track);
                    songItem = static_cast<MusicLibraryItemSong *>(mappedSongIndex.internalPointer());
                    const QString fileName = songItem->file();
                    if (!fileName.isEmpty() && !files.contains(fileName))
                        files.append(fileName);
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album: {
            for (quint32 i = 0; ; i++) {
                QModelIndex track = current.child(i, 0);
                if (!track.isValid())
                    break;
                const QModelIndex mappedSongIndex = libraryProxyModel.mapToSource(track);
                songItem = static_cast<MusicLibraryItemSong *>(mappedSongIndex.internalPointer());
                const QString fileName = songItem->file();
                if (!fileName.isEmpty() && !files.contains(fileName))
                    files.append(fileName);
            }
            break;
        }
        case MusicLibraryItem::Type_Song: {
            const QString fileName = static_cast<MusicLibraryItemSong *>(item)->file();
            if (!fileName.isEmpty() && !files.contains(fileName))
                files.append(fileName);
            break;
        }
        default:
            break;
        }
    }

    if (!files.isEmpty()) {
        MPDConnection::self()->add(files);
        if (MPDStatus::self()->state() != MPDStatus::State_Playing)
            MPDConnection::self()->startPlayingSong();

        libraryPage->libraryTreeView->selectionModel()->clearSelection();
    }
}

QStringList MainWindow::walkDirView(QModelIndex rootItem)
{
    QStringList files;

    DirViewItem *item = static_cast<DirViewItem *>(dirProxyModel.mapToSource(rootItem).internalPointer());

    if (item->type() == DirViewItem::Type_File) {
        return QStringList(item->fileName());
    }

    for (int i = 0; ; i++) {
        QModelIndex current = rootItem.child(i, 0);
        if (!current.isValid())
            return files;

        QStringList tmp = walkDirView(current);
        for (int j = 0; j < tmp.size(); j++) {
            if (!files.contains(tmp.at(j)))
                files << tmp.at(j);
        }
    }
    return files;
}

void MainWindow::addDirViewSelectionToPlaylist()
{
    QModelIndex current;
    QStringList files;
    DirViewItem *item;

    // Get selected view indexes
    const QModelIndexList selected = folderPage->dirTreeView->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();

    if (selectionSize == 0) {
        return;
    }

    for (int selectionPos = 0; selectionPos < selectionSize; selectionPos++) {
        current = selected.at(selectionPos);
        item = static_cast<DirViewItem *>(dirProxyModel.mapToSource(current).internalPointer());
        QStringList tmp;

        switch (item->type()) {
        case DirViewItem::Type_Dir:
            tmp = walkDirView(current);
            for (int i = 0; i < tmp.size(); i++) {
                if (!files.contains(tmp.at(i)))
                    files << tmp.at(i);
            }
            break;
        case DirViewItem::Type_File:
            if (!files.contains(item->fileName()))
                files << item->fileName();
            break;
        default:
            break;
        }
    }

    if (!files.isEmpty()) {
        MPDConnection::self()->add(files);
        if (MPDStatus::self()->state() != MPDStatus::State_Playing)
            MPDConnection::self()->startPlayingSong();

        folderPage->dirTreeView->selectionModel()->clearSelection();
    }
}

void MainWindow::libraryItemActivated(const QModelIndex & /*index*/)
{
    MusicLibraryItem *item;
    const QModelIndexList selected = libraryPage->libraryTreeView->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();
    if (selectionSize != 1) return; //doubleclick should only have one selected item

    const QModelIndex current = selected.at(0);
    item = static_cast<MusicLibraryItem *>(libraryProxyModel.mapToSource(current).internalPointer());
    if (item->type() == MusicLibraryItem::Type_Song) {
        addSelectionToPlaylist();
    }
}

void MainWindow::dirViewItemActivated(const QModelIndex & /*index*/)
{
    DirViewItem *item;
    const QModelIndexList selected = folderPage->dirTreeView->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();
    if (selectionSize != 1) return; //doubleclick should only have one selected item

    const QModelIndex current = selected.at(0);
    item = static_cast<DirViewItem *>(dirProxyModel.mapToSource(current).internalPointer());
    if (item->type() == DirViewItem::Type_File) {
        addDirViewSelectionToPlaylist();
    }
}

void MainWindow::addToPlaylistItemActivated()
{
    if (libraryPage->libraryTreeView->isVisible()) {
        addSelectionToPlaylist();
    } else if (folderPage->dirTreeView->isVisible()) {
        addDirViewSelectionToPlaylist();
    } else if (playlistTableView->isVisible()) {
        addPlaylistToPlaylistPushButtonActivated();
    }
}

void MainWindow::crossfadingChanged(int seconds)
{
    MPDConnection::self()->setCrossfade(seconds);
}

void MainWindow::updatePlayListStatus()
{
    MPDStats * const stats = MPDStats::self();
    QString status = "";

    if (stats->playlistSongs() == 0) {
        playListStatsLabel->setText(status);
        return;
    }

#ifdef ENABLE_KDE_SUPPORT
    status+=i18np("1 Artist, ", "%1 Artists, ", stats->playlistArtists());
    status+=i18np("1 Album, ", "%1 Albums, ", stats->playlistAlbums());
    status+=i18np("1 Song", "%1 Songs", stats->playlistSongs());
#else
    status += QString::number(stats->playlistArtists())+QString(1==stats->playlistArtists() ? " Artist, " : "Artists, ");
    status += QString::number(stats->playlistAlbums())+QString(1==stats->playlistAlbums() ? " Album, " : "Albums, ");
    status += QString::number(stats->playlistSongs())+QString(1==stats->playlistSongs() ? " Song" : "Songs");
#endif
    status += " (";
    status += MPDParseUtils::seconds2formattedString(stats->playlistTime());
    status += ")";

    playListStatsLabel->setText(status);
}

void MainWindow::movePlaylistItems(const QList<quint32> items, const int row, const int size)
{
    MPDConnection::self()->move(items, row, size);
}

void MainWindow::addFilenamesToPlaylist(const QStringList filenames, const int row, const int size)
{
    MPDConnection::self()->addid(filenames, row, size);
    if (MPDStatus::self()->state() != MPDStatus::State_Playing)
        MPDConnection::self()->startPlayingSong();
}

void MainWindow::updatePositionSilder()
{
    QString timeElapsedFormattedString;

    if (positionSlider->value() != positionSlider->maximum()) {
        positionSlider->setValue(positionSlider->value() + 1);

        timeElapsedFormattedString += Song::formattedTime(positionSlider->value());
        timeElapsedFormattedString += " / ";
        timeElapsedFormattedString += Song::formattedTime(songTime);
        songTimeElapsedLabel->setText(timeElapsedFormattedString);
    }
}

void MainWindow::copySongInfo()
{
    const QModelIndexList items = playlistTableView->selectionModel()->selectedRows();
    QModelIndex sourceIndex;
    QString txt = "";
    QClipboard *clipboard = QApplication::clipboard();

    if (items.isEmpty())
        return;

    for (int i = 0; i < items.size(); i++) {
        sourceIndex = playlistProxyModel.mapToSource(items.at(i));
        Song s = playlistModel.getSongByRow(sourceIndex.row());
        if (s.isEmpty()) {
            if (txt != "") {
                txt += "\n";
            }
            txt += s.format();
        }
    }

    clipboard->setText(txt);
}

void MainWindow::updateOutpus(const QList<Output> &outputs)
{
#ifndef ENABLE_KDE_SUPPORT
    menu_Outputs->clear();

    foreach(const Output &o, outputs) {
        QAction *a = new QAction(o.name, menu_Outputs);
        a->setCheckable(true);
        a->setChecked(o.enabled);
        a->setData(o.id);

        menu_Outputs->addAction(a);
    }
#else
    QList<QAction *> actions;

    foreach(const Output &o, outputs) {
        QAction *a = new QAction(o.name, this);
        a->setCheckable(true);
        a->setChecked(o.enabled);
        a->setData(o.id);
        actions << a;
    }

    unplugActionList("outputs_list");
    plugActionList("outputs_list", actions);
#endif
}

void MainWindow::outputChanged(QAction *action)
{
    if (action->isChecked()) {
        MPDConnection::self()->enableOutput(action->data().toInt());
    } else {
        MPDConnection::self()->disableOutput(action->data().toInt());
    }
}

void MainWindow::togglePlaylist()
{
    if (splitter->isVisible()==action_Show_playlist->isChecked()) {
        return;
    }
    static int lastHeight=0;

    int widthB4=size().width();
    bool showing=action_Show_playlist->isChecked();

    if (!showing) {
        lastHeight=size().height();
    } else {
        setMinimumHeight(0);
        setMaximumHeight(65535);
    }
    splitter->setVisible(showing);
    QApplication::processEvents();
    adjustSize();

    bool adjustWidth=size().width()<widthB4;
    bool adjustHeight=showing && size().height()<lastHeight;
    if (adjustWidth || adjustHeight) {
        resize(adjustWidth ? widthB4 : size().width(), adjustHeight ? lastHeight : size().height());
    }

    if (!showing) {
        setMinimumHeight(size().height());
        setMaximumHeight(size().height());
    }
}

// PlayList view //
void MainWindow::setupPlaylistViewMenu()
{
    playlistTableViewMenu = new QMenu(this);

    QStringList names;
    names << i18n("Length") << i18n("Track") << i18n("Disc") << i18n("Year");
    foreach(const QString &n, names) {
        QAction *act=new QAction(n, playlistTableViewMenu);
        act->setCheckable(true);
        act->setChecked(true);
        playlistTableViewMenu->addAction(act);
        viewActions.append(act);
        connect(act, SIGNAL(toggled(bool)), this, SLOT(playListTableViewToggleItem(bool)));
    }

    //Restore state
    QByteArray state = Settings::self()->playlistHeaderState();

    //Restore
    if (!state.isEmpty()) {
        playlistTableViewHeader->restoreState(state);

        for(int i=3; i<7; ++i) {
            if (playlistTableViewHeader->isSectionHidden(i) || playlistTableViewHeader->sectionSize(i) == 0) {
                viewActions.at(i-3)->setChecked(false);
                playlistTableViewHeader->resizeSection(i, 100);
                playlistTableViewHeader->setSectionHidden(i, true);
            }
        }
    }
}

void MainWindow::setupPlaylistViewHeader()
{
    playlistTableViewHeader = playlistTableView->header();

    playlistTableViewHeader->setMovable(true);
    playlistTableViewHeader->setResizeMode(QHeaderView::Interactive);
    playlistTableViewHeader->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(playlistTableViewHeader, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(playlistTableViewContextMenuClicked()));
}

void MainWindow::playlistTableViewContextMenuClicked()
{
    playlistTableViewMenu->exec(QCursor::pos());
}

void MainWindow::playListTableViewToggleItem(const bool visible)
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        int index=viewActions.indexOf(act);
        if (-1!=index) {
            playlistTableViewHeader->setSectionHidden(index+3, !visible);
        }
    }
}

/*
 * Crop playlist
 * Do this by taking the set off all song id's and subtracting from that
 * the set of selected song id's. Feed that list to MPDConnection::self()->removeSongs
 */
void MainWindow::cropPlaylist()
{
        QSet<qint32> songs = playlistModel.getSongIdSet();
        QSet<qint32> selected;

        const QModelIndexList items = playlistTableView->selectionModel()->selectedRows();
        QModelIndex sourceIndex;

        if (items.isEmpty())
                return;

        for(int i = 0; i < items.size(); i++) {
                sourceIndex = playlistProxyModel.mapToSource(items.at(i));
                selected << playlistModel.getIdByRow(sourceIndex.row());
        }

        QList<qint32> toBeRemoved = (songs - selected).toList();

        MPDConnection::self()->removeSongs(toBeRemoved);
}

// Tray Icon //
bool MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        trayIcon = NULL;
        return false;
    }

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->installEventFilter(volumeSliderEventHandler);
    trayIconMenu = new QMenu(this);

#ifdef ENABLE_KDE_SUPPORT
    quitAction = KStandardAction::quit(kapp, SLOT(quit()), trayIconMenu);
#else
    quitAction = new QAction(tr("&Quit"), trayIconMenu);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
#endif
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayIconClicked(QSystemTrayIcon::ActivationReason)));

    trayIconMenu->addAction(action_Prev_track);
    trayIconMenu->addAction(action_Next_track);
    trayIconMenu->addAction(action_Stop_track);
    trayIconMenu->addAction(action_Play_pause_track);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(windowIcon());
    trayIcon->setToolTip(i18n("Cantata"));

    return true;
}

void MainWindow::trayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        if (isHidden()) {
            showNormal();
            if (!lastPos.isNull()) {
                move(lastPos);
            }
        } else {
            hide();
        }
    default:
        break;
    }
}

void MainWindow::addPlaylistToPlaylistPushButtonActivated()
{
    const QModelIndexList items = playlistsPage->playlistsView->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = playlistsProxyModel.mapToSource(items.first());
        QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();
        playlistsModel.loadPlaylist(playlist_name);
    }
}

void MainWindow::loadPlaylistPushButtonActivated()
{
    const QModelIndexList items = playlistsPage->playlistsView->selectionModel()->selectedRows();

    if (items.size() == 1) {
        MPDConnection::self()->clear();
        MPDConnection::self()->getStatus();
        searchPlaylistLineEdit->clear();
        QModelIndex sourceIndex = playlistsProxyModel.mapToSource(items.first());
        QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();
        playlistsModel.loadPlaylist(playlist_name);
    }
}

void MainWindow::playlistsViewItemDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = playlistsProxyModel.mapToSource(index);
    QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();
    playlistsModel.loadPlaylist(playlist_name);
}

void MainWindow::removePlaylistPushButtonActivated()
{
    const QModelIndexList items = playlistsPage->playlistsView->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = playlistsProxyModel.mapToSource(items.first());
        QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();

#ifdef ENABLE_KDE_SUPPORT
        if (KMessageBox::Yes == KMessageBox::warningYesNo(this, i18n("Are you sure you want to delete playlist: %1", playlist_name),
                i18n("Delete Playlist?"))) {
            playlistsModel.removePlaylist(playlist_name);
        }
#else
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle("Delete playlist?");
        msgBox.setText("Are you sure you want to delete playlist: " + playlist_name + "?");
        msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::No);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Yes) {
            playlistsModel.removePlaylist(playlist_name);
        }
#endif
    }
}

void MainWindow::savePlaylistPushButtonActivated()
{
    QString name = QInputDialog::getText(this, i18n("Enter name"), i18n("Name"));

    if (!name.isEmpty())
        playlistsModel.savePlaylist(name);
}

/*
 * Idle command has returnd that stored_playlist:
 * "a stored playlist has been modified, renamed, created or deleted"
 * Update accordingly
 *
 * TODO: Implement
 */
void MainWindow::updateStoredPlaylists()
{
    qDebug() << "Need to update playlist";
    playlistsModel.getPlaylists();
}

void MainWindow::renamePlaylistActivated()
{
    const QModelIndexList items = playlistsPage->playlistsView->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = playlistsProxyModel.mapToSource(items.first());
        QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();
        QString new_name = QInputDialog::getText(this, i18n("Rename playlist"), i18n("Enter new name for playlist: %1").arg(playlist_name));


        if (!new_name.isEmpty()) {
            playlistsModel.renamePlaylist(playlist_name, new_name);
        }
    }
}

enum Tabs
{
    TAB_LIBRARY   = 0x01,
    TAB_DIRS      = 0x02,
    TAB_PLAYLISTS = 0x04
};

void MainWindow::currentTabChanged(int index)
{
    switch(index) {
    case 0:
        if (!(loaded&TAB_LIBRARY)) {
            loaded|=TAB_LIBRARY;
            if (!musicLibraryModel.fromXML(MPDStats::self()->dbUpdate())) {
                MPDConnection::self()->listAllInfo(MPDStats::self()->dbUpdate());
            }
        }
        break;
    case 1:
        if (!(loaded&TAB_DIRS)) {
            loaded|=TAB_DIRS;
            MPDConnection::self()->listAll();
        }
        break;
    case 2:
        if (!(loaded&TAB_PLAYLISTS)) {
            loaded|=TAB_PLAYLISTS;
            playlistsModel.getPlaylists();
        }
        break;
    case 3:
        if (lyricsNeedUpdating) {
            Lyrics::self()->get(current);
            lyricsNeedUpdating=false;
        }
        break;
    default:
        break;
    }
}

void MainWindow::lyrics(const QString &artist, const QString &title, const QString &text)
{
    if (artist==current.artist && title==current.title) {
        lyricsPage->lyricsText->setText(text);
    }
}

void MainWindow::cover(const QString &artist, const QString &album, const QImage &img)
{
    if (artist==current.albumArtist() && album==current.album) {
        if (img.isNull()) {
            coverWidget->setPixmap(noCover);
        } else {
            coverWidget->setPixmap(QPixmap::fromImage(img).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void MainWindow::updateGenres(const QStringList &genres)
{
    QStringList entries;
    entries << i18n("All Genres");
    entries+=genres;

    bool diff=libraryPage->genreCombo->count() != entries.count();
    if (!diff) {
        // Check items...
        for (int i=1; i<libraryPage->genreCombo->count() && !diff; ++i) {
            if (libraryPage->genreCombo->itemText(i) != entries.at(i)) {
                diff=true;
            }
        }
    }

    if (!diff) {
        return;
    }

    QString currentFilter = libraryPage->genreCombo->currentIndex() ? libraryPage->genreCombo->currentText() : QString();

    libraryPage->genreCombo->clear();
    if (genres.count()<2) {
        libraryPage->genreCombo->setCurrentIndex(0);
    } else {
        libraryPage->genreCombo->addItems(entries);
        if (!currentFilter.isEmpty()) {
            bool found=false;
            for (int i=1; i<libraryPage->genreCombo->count() && !found; ++i) {
                if (libraryPage->genreCombo->itemText(i) == currentFilter) {
                    libraryPage->genreCombo->setCurrentIndex(i);
                    found=true;
                }
            }
            if (!found) {
                libraryPage->genreCombo->setCurrentIndex(0);
            }
        }
    }
}
