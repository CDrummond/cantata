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
#else
#include <QtGui/QMenuBar>
#include "networkproxyfactory.h"
#endif

//#include "covermanager.h"
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
                window->increaseVolumeAction->trigger();
        } else {
            for (int i = 0; i > numSteps; --i)
                window->decreaseVolumeAction->trigger();
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
            window->showPlaylistAction->trigger();
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

    libraryPage = new LibraryPage(this);
    folderPage = new FolderPage(this);
    playlistsPage = new PlaylistsPage(this);
    lyricsPage = new LyricsPage(this);

    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);

#ifndef ENABLE_KDE_SUPPORT
    setWindowIcon(QIcon(":/icons/cantata.svg"));
    setWindowTitle("Cantata");

    QMenu *menu=new QMenu(tr("File"), this);
    QAction *menuAct=menu->addAction(tr("Quit"), qApp, SLOT(quit()));
    menuAct->setIcon(QIcon::fromTheme("application-exit"));
    menuBar()->addMenu(menu);
    menu=new QMenu(tr("Tools"), this);
    //menuAct=menu->addAction(tr("Cover Mananger..."), this, SLOT(showCoverManager()));
    //menuAct->setIcon(QIcon::fromTheme("insert-image"));
    //menuBar()->addMenu(menu);
    menu=new QMenu(tr("Settings"), this);
    menuAct=menu->addAction(tr("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
    menuAct->setIcon(QIcon::fromTheme("configure"));
    menuBar()->addMenu(menu);
    menu=new QMenu(tr("Help"), this);
    menuAct=menu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
    menuAct->setIcon(windowIcon());
    menuBar()->addMenu(menu);

    QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::Instance());
#endif

#ifdef ENABLE_KDE_SUPPORT
    KStandardAction::quit(kapp, SLOT(quit()), actionCollection());
    KStandardAction::preferences(this, SLOT(showPreferencesDialog()), actionCollection());

    //coverManagerAction = actionCollection()->addAction("covermanager");
    //coverManagerAction->setText(i18n("Cover Mananger"));

    updateDbAction = actionCollection()->addAction("updatedatabase");
    updateDbAction->setText(i18n("Update Database"));

    prevTrackAction = actionCollection()->addAction("prevtrack");
    prevTrackAction->setText(i18n("Previous Track"));
    prevTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Left));

    nextTrackAction = actionCollection()->addAction("nexttrack");
    nextTrackAction->setText(i18n("Next Track"));
    nextTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Right));

    playPauseTrackAction = actionCollection()->addAction("playpausetrack");
    playPauseTrackAction->setText(i18n("Play/Pause"));
    playPauseTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_C));

    stopTrackAction = actionCollection()->addAction("stoptrack");
    stopTrackAction->setText(i18n("Stop"));
    stopTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_X));

    increaseVolumeAction = actionCollection()->addAction("increasevolume");
    increaseVolumeAction->setText(i18n("Increase Volume"));
    increaseVolumeAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Up));

    decreaseVolumeAction = actionCollection()->addAction("decreasevolume");
    decreaseVolumeAction->setText(i18n("Decrease Volume"));
    decreaseVolumeAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Down));

    addToPlaylistAction = actionCollection()->addAction("addtoplaylist");
    addToPlaylistAction->setText(i18n("Add To Playlist"));

    replacePlaylistAction = actionCollection()->addAction("replaceplaylist");
    replacePlaylistAction->setText(i18n("Replace Playlist"));

    removePlaylistAction = actionCollection()->addAction("removeplaylist");
    removePlaylistAction->setText(i18n("Remove"));

    removeFromPlaylistAction = actionCollection()->addAction("removefromplaylist");
    removeFromPlaylistAction->setText(i18n("Remove"));
    removeFromPlaylistAction->setShortcut(QKeySequence::Delete);

    copySongInfoAction = actionCollection()->addAction("copysonginfo");
    copySongInfoAction->setText(i18n("Copy Song Info"));

    cropPlaylistAction = actionCollection()->addAction("cropplaylist");
    cropPlaylistAction->setText(i18n("Crop"));

    shufflePlaylistAction = actionCollection()->addAction("shuffleplaylist");
    shufflePlaylistAction->setText(i18n("Shuffle"));

    renamePlaylistAction = actionCollection()->addAction("renameplaylist");
    renamePlaylistAction->setText(i18n("Rename"));

    savePlaylistAction = actionCollection()->addAction("saveplaylist");
    savePlaylistAction->setText(i18n("Save As"));

    clearPlaylistAction = actionCollection()->addAction("clearplaylist");
    clearPlaylistAction->setText(i18n("Clear"));

    showPlaylistAction = actionCollection()->addAction("showplaylist");
    showPlaylistAction->setText(i18n("Show Playlist"));

    randomPlaylistAction = actionCollection()->addAction("randomplaylist");
    randomPlaylistAction->setText(i18n("Random"));

    repeatPlaylistAction = actionCollection()->addAction("repeatplaylist");
    repeatPlaylistAction->setText(i18n("Repeat"));

    consumePlaylistAction = actionCollection()->addAction("consumeplaylist");
    consumePlaylistAction->setText(i18n("Consume"));

    libraryTabAction = actionCollection()->addAction("showlibrarytab");
    libraryTabAction->setText(i18n("Library"));

    foldersTabAction = actionCollection()->addAction("showfolderstab");
    foldersTabAction->setText(i18n("Folders"));

    playlistsTabAction = actionCollection()->addAction("showplayliststab");
    playlistsTabAction->setText(i18n("Playlists"));

    lyricsTabAction = actionCollection()->addAction("showlyricstab");
    lyricsTabAction->setText(i18n("Lyrics"));
#else
    //coverManagerAction = new QAction(tr("Cover Mananger"), this);
    updateDbAction = new QAction(tr("Update Database"), this);
    prevTrackAction = new QAction(tr("Previous Track"), this);
    nextTrackAction = new QAction(tr("Next Track"), this);
    playPauseTrackAction = new QAction(tr("Play/Pause"), this);
    stopTrackAction = new QAction(tr("Stop"), this);
    increaseVolumeAction = new QAction(tr("Increase Volume"), this);
    decreaseVolumeAction = new QAction(tr("Decrease Volume"), this);
    addToPlaylistAction = new QAction(tr("Add To Playlist"), this);
    replacePlaylistAction = new QAction(tr("Replace Playlist"), this);
    removePlaylistAction = new QAction(tr("Remove"), this);
    removeFromPlaylistAction = new QAction(tr("Remove"), this);
    removeFromPlaylistAction->setShortcut(QKeySequence::Delete);
    copySongInfoAction = new QAction(tr("Copy Song Info"), this);
    copySongInfoAction->setShortcut(QKeySequence::Copy);
    cropPlaylistAction = new QAction(tr("Crop"), this);
    shufflePlaylistAction = new QAction(tr("Shuffle"), this);
    renamePlaylistAction = new QAction(tr("Rename"), this);
    savePlaylistAction = new QAction(tr("Save As"), this);
    clearPlaylistAction = new QAction(tr("Clear"), this);
    showPlaylistAction = new QAction(tr("Show Playlist"), this);
    randomPlaylistAction = new QAction(tr("Random"), this);
    repeatPlaylistAction = new QAction(tr("Repeat"), this);
    consumePlaylistAction = new QAction(tr("Consume"), this);
    libraryTabAction = new QAction(tr("Library"), this);
    foldersTabAction = new QAction(tr("Folders"), this);
    playlistsTabAction = new QAction(tr("Playlists"), this);
    lyricsTabAction = new QAction(tr("Lyrics"), this);
#endif
    libraryTabAction->setShortcut(Qt::Key_F9);
    foldersTabAction->setShortcut(Qt::Key_F10);
    playlistsTabAction->setShortcut(Qt::Key_F11);
    lyricsTabAction->setShortcut(Qt::Key_F12);

    setVisible(true);

    // Setup event handler for volume adjustment
    volumeSliderEventHandler = new VolumeSliderEventHandler(this);
    coverEventHandler = new CoverEventHandler(this);

    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(volumeSliderEventHandler);

    noCover = QIcon::fromTheme("media-optical-audio").pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverWidget->setPixmap(noCover);

    playbackPlay = QIcon::fromTheme("media-playback-start");
    playbackPause = QIcon::fromTheme("media-playback-pause");
// #ifdef ENABLE_KDE_SUPPORT
//    randomPlaylistAction->setIcon(KIcon("media-random-tracks"));
//    repeatPlaylistAction->setIcon(KIcon("media-repeat-playlist"));
//     consumePlaylistAction->setIcon(KIcon("media-consume-playlist"));
//     addToPlaylistAction->setIcon(KIcon("media-track-add"));
//     replacePlaylistAction->setIcon(KIcon("media-replace-playlist"));
// #else
//    randomPlaylistAction->setIcon(QIcon(":/icons/hi16-action-media-random-tracks.png"));
//    repeatPlaylistAction->setIcon(QIcon(":/icons/hi16-action-media-repeat-playlist.png"));
//     consumePlaylistAction->setIcon(QIcon(":/icons/hi16-action-media-consume-playlist.png"));
//     addToPlaylistAction->setIcon(QIcon(":/icons/hi16-action-media-track-add.png"));
//     replacePlaylistAction->setIcon(QIcon(":/icons/hi16-action-media-replace-playlist.png"));
// #endif
    repeatPlaylistAction->setIcon(QIcon::fromTheme("edit-redo"));
    randomPlaylistAction->setIcon(QIcon::fromTheme("media-playlist-shuffle"));
    consumePlaylistAction->setIcon(QIcon::fromTheme("format-list-unordered"));
    addToPlaylistAction->setIcon(QIcon::fromTheme(Qt::RightToLeft==QApplication::layoutDirection() ? "arrow-left" : "arrow-right"));
    replacePlaylistAction->setIcon(QIcon::fromTheme(Qt::RightToLeft==QApplication::layoutDirection() ? "arrow-left-double" : "arrow-right-double"));

    prevTrackAction->setIcon(QIcon::fromTheme("media-skip-backward"));
    nextTrackAction->setIcon(QIcon::fromTheme("media-skip-forward"));
    playPauseTrackAction->setIcon(playbackPlay);
    stopTrackAction->setIcon(QIcon::fromTheme("media-playback-stop"));
    removePlaylistAction->setIcon(QIcon::fromTheme("edit-delete"));
    removeFromPlaylistAction->setIcon(QIcon::fromTheme("list-remove"));
    renamePlaylistAction->setIcon(QIcon::fromTheme("edit-rename"));
    clearPlaylistAction->setIcon(QIcon::fromTheme("edit-clear-list"));
    savePlaylistAction->setIcon(QIcon::fromTheme("document-save-as"));
    clearPlaylistAction->setIcon(QIcon::fromTheme("edit-clear-list"));
    showPlaylistAction->setIcon(QIcon::fromTheme("view-media-playlist"));
    //coverManagerAction->setIcon(QIcon::fromTheme("insert-image"));
    updateDbAction->setIcon(QIcon::fromTheme("view-refresh"));
    libraryTabAction->setIcon(QIcon::fromTheme("media-optical-audio"));
    foldersTabAction->setIcon(QIcon::fromTheme("inode-directory"));
    playlistsTabAction->setIcon(QIcon::fromTheme("view-media-playlist"));
    lyricsTabAction->setIcon(QIcon::fromTheme("view-media-lyrics"));

    volumeButton->setIcon(QIcon::fromTheme("player-volume"));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            SLOT(cover(const QString &, const QString &, const QImage &)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            &musicLibraryModel, SLOT(setCover(const QString &, const QString &, const QImage &)));

    playPauseTrackButton->setDefaultAction(playPauseTrackAction);
    stopTrackButton->setDefaultAction(stopTrackAction);
    nextTrackButton->setDefaultAction(nextTrackAction);
    prevTrackButton->setDefaultAction(prevTrackAction);
    libraryPage->addToPlaylist->setDefaultAction(addToPlaylistAction);
    libraryPage->replacePlaylist->setDefaultAction(replacePlaylistAction);
    folderPage->addToPlaylist->setDefaultAction(addToPlaylistAction);
    folderPage->replacePlaylist->setDefaultAction(replacePlaylistAction);
    savePlaylistPushButton->setDefaultAction(savePlaylistAction);
    removeAllFromPlaylistPushButton->setDefaultAction(clearPlaylistAction);
    removeFromPlaylistPushButton->setDefaultAction(removeFromPlaylistAction);
    playlistsPage->addToPlaylist->setDefaultAction(addToPlaylistAction);
    playlistsPage->replacePlaylist->setDefaultAction(replacePlaylistAction);
    playlistsPage->removePlaylist->setDefaultAction(removePlaylistAction);
    randomPushButton->setDefaultAction(randomPlaylistAction);
    repeatPushButton->setDefaultAction(repeatPlaylistAction);
    consumePushButton->setDefaultAction(consumePlaylistAction);
    libraryPage->libraryUpdate->setDefaultAction(updateDbAction);
    folderPage->libraryUpdate->setDefaultAction(updateDbAction);
    playlistsPage->libraryUpdate->setDefaultAction(updateDbAction);

    tabWidget->AddTab(libraryPage, libraryTabAction->icon(), libraryTabAction->text());
    tabWidget->AddTab(folderPage, foldersTabAction->icon(), foldersTabAction->text());
    tabWidget->AddTab(playlistsPage, playlistsTabAction->icon(), playlistsTabAction->text());
    tabWidget->AddTab(lyricsPage, lyricsTabAction->icon(), lyricsTabAction->text());

    tabWidget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    connect(tabWidget, SIGNAL(CurrentChanged(int)), this, SLOT(currentTabChanged(int)));

    showPlaylistAction->setCheckable(true);
    randomPlaylistAction->setCheckable(true);
    repeatPlaylistAction->setCheckable(true);
    consumePlaylistAction->setCheckable(true);

    libraryPage->addToPlaylist->setEnabled(false);
    libraryPage->replacePlaylist->setEnabled(false);
    folderPage->addToPlaylist->setEnabled(false);
    folderPage->replacePlaylist->setEnabled(false);
    playlistsPage->addToPlaylist->setEnabled(false);
    playlistsPage->replacePlaylist->setEnabled(false);
    playlistsPage->removePlaylist->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    libraryPage->search->setPlaceholderText(i18n("Search library..."));
    folderPage->search->setPlaceholderText(i18n("Search files..."));
    searchPlaylistLineEdit->setPlaceholderText(i18n("Search playlist..."));
#else
    libraryPage->search->setPlaceholderText(tr("Search library..."));
    folderPage->search->setPlaceholderText(tr("Search files..."));
    searchPlaylistLineEdit->setPlaceholderText(tr("Search playlist..."));
#endif
    QList<QToolButton *> btns;
    btns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton
         << libraryPage->addToPlaylist << libraryPage->replacePlaylist
         << folderPage->addToPlaylist << folderPage->replacePlaylist << playlistsPage->addToPlaylist
         << playlistsPage->replacePlaylist << playlistsPage->removePlaylist << repeatPushButton << randomPushButton
         << savePlaylistPushButton << removeAllFromPlaylistPushButton << removeFromPlaylistPushButton
         << consumePushButton << libraryPage->libraryUpdate << folderPage->libraryUpdate << playlistsPage->libraryUpdate << volumeButton;

    foreach(QToolButton * b, btns) {
        b->setAutoRaise(true);
    }

#ifdef ENABLE_KDE_SUPPORT
    trackLabel->setText(i18n("Stopped"));
#else
    trackLabel->setText(tr("Stopped"));
#endif
    artistLabel->setText("...");

    playlistsProxyModel.setSourceModel(&playlistsModel);
    playlistsPage->view->setModel(&playlistsProxyModel);
    playlistsPage->view->sortByColumn(0, Qt::AscendingOrder);
    playlistsPage->view->addAction(addToPlaylistAction);
    playlistsPage->view->addAction(replacePlaylistAction);
    playlistsPage->view->addAction(removePlaylistAction);
    playlistsPage->view->addAction(renamePlaylistAction);

    libraryPage->view->setHeaderHidden(true);
    folderPage->view->setHeaderHidden(true);
    playlistsPage->view->setHeaderHidden(true);

    showPlaylistAction->setChecked(Settings::self()->showPlaylist());
    randomPlaylistAction->setChecked(Settings::self()->randomPlaylist());
    repeatPlaylistAction->setChecked(Settings::self()->repeatPlaylist());
    consumePlaylistAction->setChecked(Settings::self()->consumePlaylist());
    mpdDir=Settings::self()->mpdDir();
    lyricsPage->setMpdDir(mpdDir);
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    Covers::self()->setMpdDir(mpdDir);
    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->coverSize());
    tabWidget->SetMode((FancyTabWidget::Mode)Settings::self()->sidebar());

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
    libraryPage->view->setModel(&libraryProxyModel);
    libraryPage->view->addAction(addToPlaylistAction);
    libraryPage->view->addAction(replacePlaylistAction);

    dirProxyModel.setSourceModel(&dirviewModel);
    folderPage->view->setModel(&dirProxyModel);
    folderPage->view->addAction(addToPlaylistAction);
    folderPage->view->addAction(replacePlaylistAction);

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
    playlistTableView->addAction(removeFromPlaylistAction);
    playlistTableView->addAction(clearPlaylistAction);
    playlistTableView->addAction(cropPlaylistAction);
    playlistTableView->addAction(shufflePlaylistAction);
    playlistTableView->addAction(copySongInfoAction);
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
    connect(updateDbAction, SIGNAL(triggered(bool)), this, SLOT(updateDb()));
    //connect(coverManagerAction, SIGNAL(triggered(bool)), this, SLOT(showCoverManager()));
    connect(prevTrackAction, SIGNAL(triggered(bool)), this, SLOT(previousTrack()));
    connect(nextTrackAction, SIGNAL(triggered(bool)), this, SLOT(nextTrack()));
    connect(playPauseTrackAction, SIGNAL(triggered(bool)), this, SLOT(playPauseTrack()));
    connect(stopTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopTrack()));
    connect(volumeControl, SIGNAL(valueChanged(int)), SLOT(setVolume(int)));
    connect(increaseVolumeAction, SIGNAL(triggered(bool)), this, SLOT(increaseVolume()));
    connect(decreaseVolumeAction, SIGNAL(triggered(bool)), this, SLOT(decreaseVolume()));
    connect(increaseVolumeAction, SIGNAL(triggered(bool)), volumeControl, SLOT(increaseVolume()));
    connect(decreaseVolumeAction, SIGNAL(triggered(bool)), volumeControl, SLOT(decreaseVolume()));
    connect(positionSlider, SIGNAL(sliderPressed()), this, SLOT(positionSliderPressed()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(positionSliderReleased()));
    connect(randomPlaylistAction, SIGNAL(activated()), this, SLOT(setRandom()));
    connect(repeatPlaylistAction, SIGNAL(activated()), this, SLOT(setRepeat()));
    connect(consumePlaylistAction, SIGNAL(activated()), this, SLOT(setConsume()));
    connect(libraryPage->search, SIGNAL(returnPressed()), this, SLOT(searchMusicLibrary()));
    connect(libraryPage->search, SIGNAL(textChanged(const QString)), this, SLOT(searchMusicLibrary()));
    connect(libraryPage->genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchMusicLibrary()));
    connect(folderPage->search, SIGNAL(returnPressed()), this, SLOT(searchDirView()));
    connect(folderPage->search, SIGNAL(textChanged(const QString)), this, SLOT(searchDirView()));
    connect(searchPlaylistLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPlaylist()));
    connect(searchPlaylistLineEdit, SIGNAL(textChanged(const QString)), this, SLOT(searchPlaylist()));
    connect(playlistTableView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playlistItemActivated(const QModelIndex &)));
    connect(libraryPage->view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(libraryItemActivated(const QModelIndex &)));
    connect(folderPage->view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(dirViewItemActivated(const QModelIndex &)));
    connect(addToPlaylistAction, SIGNAL(activated()), this, SLOT(addToPlaylist()));
    connect(replacePlaylistAction, SIGNAL(activated()), this, SLOT(replacePlaylist()));
    connect(removePlaylistAction, SIGNAL(activated()), this, SLOT(removePlaylist()));
    connect(savePlaylistAction, SIGNAL(activated()), this, SLOT(savePlaylist()));
    connect(removeFromPlaylistAction, SIGNAL(activated()), this, SLOT(removeFromPlaylist()));
    connect(renamePlaylistAction, SIGNAL(triggered()), this, SLOT(renamePlaylist()));
    connect(clearPlaylistAction, SIGNAL(activated()), this, SLOT(clearPlaylist()));
    connect(copySongInfoAction, SIGNAL(activated()), this, SLOT(copySongInfo()));
    connect(cropPlaylistAction, SIGNAL(activated()), this, SLOT(cropPlaylist()));
    connect(shufflePlaylistAction, SIGNAL(activated()), MPDConnection::self(), SLOT(shuffle()));
    connect(showPlaylistAction, SIGNAL(activated()), this, SLOT(togglePlaylist()));
    connect(&elapsedTimer, SIGNAL(timeout()), this, SLOT(updatePositionSilder()));
    connect(&musicLibraryModel, SIGNAL(updateGenres(const QStringList &)), this, SLOT(updateGenres(const QStringList &)));
    connect(libraryPage->view, SIGNAL(itemsSelected(bool)), libraryPage->addToPlaylist, SLOT(setEnabled(bool)));
    connect(libraryPage->view, SIGNAL(itemsSelected(bool)), libraryPage->replacePlaylist, SLOT(setEnabled(bool)));
    connect(folderPage->view, SIGNAL(itemsSelected(bool)), folderPage->addToPlaylist, SLOT(setEnabled(bool)));
    connect(folderPage->view, SIGNAL(itemsSelected(bool)), folderPage->replacePlaylist, SLOT(setEnabled(bool)));
    connect(playlistsPage->view, SIGNAL(itemsSelected(bool)), playlistsPage->addToPlaylist, SLOT(setEnabled(bool)));
    connect(playlistsPage->view, SIGNAL(itemsSelected(bool)), playlistsPage->replacePlaylist, SLOT(setEnabled(bool)));
    connect(playlistsPage->view, SIGNAL(itemsSelected(bool)), playlistsPage->removePlaylist, SLOT(setEnabled(bool)));
    connect(volumeButton, SIGNAL(clicked()), SLOT(showVolumeControl()));
    connect(libraryTabAction, SIGNAL(activated()), this, SLOT(showLibraryTab()));
    connect(foldersTabAction, SIGNAL(activated()), this, SLOT(showFoldersTab()));
    connect(playlistsTabAction, SIGNAL(activated()), this, SLOT(showPlaylistsTab()));
    connect(lyricsTabAction, SIGNAL(activated()), this, SLOT(showLyricsTab()));

    connect(playlistsPage->view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playlistsViewItemDoubleClicked(const QModelIndex &)));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), this, SLOT(updateStoredPlaylists()));

    elapsedTimer.setInterval(1000);

    QByteArray state=Settings::self()->splitterState();

    if (state.isEmpty()) {
        QList<int> sizes;
        sizes << 250 << 500;
        splitter->setSizes(sizes);
        resize(800, 600);
    } else {
        splitter->restoreState(Settings::self()->splitterState());
    }

    MPDConnection::self()->getStatus();
    MPDConnection::self()->getStats();
    MPDConnection::self()->playListInfo();

    currentTabChanged(0);
    playlistItemsSelected(false);
    playlistTableView->setFocus();
}

MainWindow::~MainWindow()
{
#ifndef ENABLE_KDE_SUPPORT
    Settings::self()->saveMainWindowSize(size());
#endif
    Settings::self()->saveConsumePlaylist(consumePlaylistAction->isChecked());
    Settings::self()->saveRandomPlaylist(randomPlaylistAction->isChecked());
    Settings::self()->saveRepeatPlaylist(repeatPlaylistAction->isChecked());
    Settings::self()->saveShowPlaylist(showPlaylistAction->isChecked());
    Settings::self()->saveSplitterState(splitter->saveState());
    Settings::self()->savePlaylistHeaderState(playlistTableViewHeader->saveState());
    Settings::self()->saveSidebar((int)(tabWidget->mode()));
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
        removeFromPlaylistAction->setEnabled(s);
        copySongInfoAction->setEnabled(s);
        clearPlaylistAction->setEnabled(true);
        cropPlaylistAction->setEnabled(playlistTableView->haveUnSelectedItems());
        shufflePlaylistAction->setEnabled(true);
    }
}

void MainWindow::mpdConnectionDied()
{
    // Stop timer(s)
    elapsedTimer.stop();

    // Reset GUI
    positionSlider->setValue(0);
    songTimeElapsedLabel->setText("00:00 / 00:00");

    playPauseTrackAction->setIcon(playbackPlay);
    playPauseTrackAction->setEnabled(true);
    stopTrackAction->setEnabled(false);

    // Show warning message
#ifdef ENABLE_KDE_SUPPORT
    KMessageBox::error(this, i18n("The MPD connection died unexpectedly.\nThis error is unrecoverable, please restart %1.").arg(PACKAGE_NAME),
                       i18n("Lost MPD Connection"));
#else
    QMessageBox::critical(this, tr("MPD connection gone"), tr("The MPD connection died unexpectedly. This error is unrecoverable, please restart "PACKAGE_NAME"."));
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

//void MainWindow::showCoverManager()
//{
//    CoverManager *mgr = new CoverManager(this, &musicLibraryModel);
//    mgr->exec();
//}

int MainWindow::showPreferencesDialog()
{
    PreferencesDialog pref(this, lyricsPage);
    connect(&pref, SIGNAL(settingsSaved()), this, SLOT(updateSettings()));

    return pref.exec();
}

void MainWindow::updateSettings()
{
    mpdDir=Settings::self()->mpdDir();
    lyricsPage->setMpdDir(mpdDir);
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    Covers::self()->setMpdDir(mpdDir);
    Settings::self()->save();

    if (!MPDConnection::self()->isConnected()) {
        return;
    }

    if (((int)MusicLibraryItemAlbum::currentCoverSize())!=Settings::self()->coverSize()) {
        MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->coverSize());
        musicLibraryModel.clearUpdateTime();
        MPDConnection::self()->listAllInfo(MPDStats::self()->dbUpdate());
    }

    if (Settings::self()->useSystemTray()) {
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
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("About Cantata"));
    msgBox.setText(tr("Simple GUI front-end for MPD.<br/>(c) Craig Drummond 2001.<br/>Released under the GPLv2<br/><br/><i>Based upon QtMPC - (C) 2007-2010 The QtMPC Authors</i>"));
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
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

    stopTrackAction->setEnabled(false);
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
    MPDConnection::self()->setRandom(randomPlaylistAction->isChecked());
}

void MainWindow::setRepeat()
{
    MPDConnection::self()->setRepeat(repeatPlaylistAction->isChecked());
}

void MainWindow::setConsume()
{
    MPDConnection::self()->setConsume(consumePlaylistAction->isChecked());
}

void MainWindow::searchMusicLibrary()
{
    libraryProxyModel.setFilterGenre(0==libraryPage->genreCombo->currentIndex() ? QString() : libraryPage->genreCombo->currentText());

    if (libraryPage->search->text().isEmpty()) {
        libraryProxyModel.setFilterRegExp(QString());
        return;
    }

    libraryProxyModel.setFilterRegExp(libraryPage->search->text());
}

void MainWindow::searchDirView()
{
    if (folderPage->search->text().isEmpty()) {
        dirProxyModel.setFilterRegExp("");
        return;
    }

    dirProxyModel.setFilterRegExp(folderPage->search->text());
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
#ifdef ENABLE_KDE_SUPPORT
        trackLabel->setText(i18n("Stopped"));
#else
        trackLabel->setText(tr("Stopped"));
#endif
        artistLabel->setText("...");
    } else {
        trackLabel->setText(song.title);
        artistLabel->setText(song.artist);
    }

    playlistModel.updateCurrentSong(song.id);

    if (3==tabWidget->current_index()) {
        lyricsPage->update(song);
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

#ifdef ENABLE_KDE_SUPPORT
    volumeButton->setToolTip(i18n("Volume %1%").arg(status->volume()));
    volumeControl->setToolTip(i18n("Volume %1%").arg(status->volume()));
#else
    volumeButton->setToolTip(tr("Volume %1%").arg(status->volume()));
    volumeControl->setToolTip(tr("Volume %1%").arg(status->volume()));
#endif
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
        playPauseTrackAction->setIcon(playbackPause);
        playPauseTrackAction->setEnabled(true);
        //playPauseTrackButton->setChecked(false);
        stopTrackAction->setEnabled(true);
        elapsedTimer.start();

        if (trayIcon != NULL)
            trayIcon->setIcon(playbackPlay);

        break;
    case MPDStatus::State_Inactive:
    case MPDStatus::State_Stopped:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(true);
        stopTrackAction->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
        trackLabel->setText(i18n("Stopped"));
#else
        trackLabel->setText(tr("Stopped"));
#endif
        artistLabel->setText("...");

        if (trayIcon != NULL)
            trayIcon->setIcon(windowIcon());

        elapsedTimer.stop();
        break;
    case MPDStatus::State_Paused:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(true);
        stopTrackAction->setEnabled(true);

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
    if (libraryPage->view->isVisible()) {
        addLibrarySelectionToPlaylist();
    } else if (folderPage->view->isVisible()) {
        addDirViewSelectionToPlaylist();
    } else if (playlistsPage->view->isVisible()) {
        addPlaylistsSelectionToPlaylist();
    }
}

void MainWindow::addLibrarySelectionToPlaylist()
{
    QStringList files;
    MusicLibraryItem *item;
    MusicLibraryItemSong *songItem;

    // Get selected view indexes
    const QModelIndexList selected = libraryPage->view->selectionModel()->selectedIndexes();
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

        libraryPage->view->selectionModel()->clearSelection();
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
    const QModelIndexList selected = folderPage->view->selectionModel()->selectedIndexes();
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

        folderPage->view->selectionModel()->clearSelection();
    }
}

void MainWindow::libraryItemActivated(const QModelIndex & /*index*/)
{
    MusicLibraryItem *item;
    const QModelIndexList selected = libraryPage->view->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();
    if (selectionSize != 1) return; //doubleclick should only have one selected item

    const QModelIndex current = selected.at(0);
    item = static_cast<MusicLibraryItem *>(libraryProxyModel.mapToSource(current).internalPointer());
    if (item->type() == MusicLibraryItem::Type_Song) {
        addLibrarySelectionToPlaylist();
    }
}

void MainWindow::dirViewItemActivated(const QModelIndex & /*index*/)
{
    DirViewItem *item;
    const QModelIndexList selected = folderPage->view->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();
    if (selectionSize != 1) return; //doubleclick should only have one selected item

    const QModelIndex current = selected.at(0);
    item = static_cast<DirViewItem *>(dirProxyModel.mapToSource(current).internalPointer());
    if (item->type() == DirViewItem::Type_File) {
        addDirViewSelectionToPlaylist();
    }
}

void MainWindow::addToPlaylist()
{
    if (libraryPage->view->isVisible()) {
        addLibrarySelectionToPlaylist();
    } else if (folderPage->view->isVisible()) {
        addDirViewSelectionToPlaylist();
    } else if (playlistTableView->isVisible()) {
        addPlaylistsSelectionToPlaylist();
    }
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

void MainWindow::togglePlaylist()
{
    if (splitter->isVisible()==showPlaylistAction->isChecked()) {
        return;
    }
    static int lastHeight=0;

    int widthB4=size().width();
    bool showing=showPlaylistAction->isChecked();

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
#ifdef ENABLE_KDE_SUPPORT
    names << i18n("Length") << i18n("Track") << i18n("Disc") << i18n("Year");
#else
    names << tr("Length") << tr("Track") << tr("Disc") << tr("Year");
#endif
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

    trayIconMenu->addAction(prevTrackAction);
    trayIconMenu->addAction(nextTrackAction);
    trayIconMenu->addAction(stopTrackAction);
    trayIconMenu->addAction(playPauseTrackAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setIcon(windowIcon());
#ifdef ENABLE_KDE_SUPPORT
    trayIcon->setToolTip(i18n("Cantata"));
#else
    trayIcon->setToolTip(tr("Cantata"));
#endif

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

void MainWindow::addPlaylistsSelectionToPlaylist()
{
    const QModelIndexList items = playlistsPage->view->selectionModel()->selectedRows();

    if (items.size() == 1) {
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

void MainWindow::removePlaylist()
{
    const QModelIndexList items = playlistsPage->view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = playlistsProxyModel.mapToSource(items.first());
        QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();

#ifdef ENABLE_KDE_SUPPORT
        if (KMessageBox::Yes == KMessageBox::warningYesNo(this, i18n("Are you sure you want to delete playlist: %1?", playlist_name),
                i18n("Delete Playlist?"))) {
            playlistsModel.removePlaylist(playlist_name);
        }
#else
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Delete Playlist?"));
        msgBox.setText(tr("Are you sure you want to delete playlist: %1?").arg(playlist_name));
        msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        msgBox.setDefaultButton(QMessageBox::No);
        int ret = msgBox.exec();

        if (ret == QMessageBox::Yes) {
            playlistsModel.removePlaylist(playlist_name);
        }
#endif
    }
}

void MainWindow::savePlaylist()
{
#ifdef ENABLE_KDE_SUPPORT
    QString name = QInputDialog::getText(this, i18n("Playlist Name"), i18n("Enter a name for the playlist:"));
#else
    QString name = QInputDialog::getText(this, tr("Playlist Name"), tr("Enter a name for the playlist:"));
#endif

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

void MainWindow::renamePlaylist()
{
    const QModelIndexList items = playlistsPage->view->selectionModel()->selectedRows();

    if (items.size() == 1) {
        QModelIndex sourceIndex = playlistsProxyModel.mapToSource(items.first());
        QString playlist_name = playlistsModel.data(sourceIndex, Qt::DisplayRole).toString();
#ifdef ENABLE_KDE_SUPPORT
        QString new_name = QInputDialog::getText(this, i18n("Rename Playlist"), i18n("Enter new name for playlist: %1").arg(playlist_name));
#else
        QString new_name = QInputDialog::getText(this, tr("Rename Playlist"), tr("Enter new name for playlist: %1").arg(playlist_name));
#endif

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
            lyricsPage->update(current);
            lyricsNeedUpdating=false;
        }
        break;
    default:
        break;
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
#ifdef ENABLE_KDE_SUPPORT
    entries << i18n("All Genres");
#else
    entries << tr("All Genres");
#endif
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

void MainWindow::showLibraryTab()
{
    tabWidget->SetCurrentIndex(0);
}

void MainWindow::showFoldersTab()
{
    tabWidget->SetCurrentIndex(1);
}

void MainWindow::showPlaylistsTab()
{
    tabWidget->SetCurrentIndex(2);
}

void MainWindow::showLyricsTab()
{
    tabWidget->SetCurrentIndex(3);
}
