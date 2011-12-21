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

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtGui/QResizeEvent>
#include <QtGui/QMoveEvent>
#include <QtGui/QClipboard>
#include <QtGui/QInputDialog>
#include <cstdlib>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KApplication>
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KNotification>
#include <KDE/KStandardAction>
#include <KDE/KAboutApplicationDialog>
#include <KDE/KLineEdit>
#include <KDE/KXMLGUIFactory>
#include <KDE/KMessageBox>
#include <KDE/KIcon>
#include <KDE/KMenuBar>
#include <KDE/KMenu>
#include <KDE/KStatusNotifierItem>
#else
#include <QtGui/QMenuBar>
#include <QtGui/QMessageBox>
#include "networkproxyfactory.h"
#endif
#include "playlistsmodel.h"
#include "covers.h"
#include "mainwindow.h"
#include "preferencesdialog.h"
#include "mpdconnection.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "mpdparseutils.h"
#include "settings.h"
#include "updatedialog.h"
#include "config.h"
#include "musiclibraryitemalbum.h"
#include "librarypage.h"
#include "albumspage.h"
#include "folderpage.h"
#include "streamspage.h"
#include "lyricspage.h"
#include "infopage.h"
#include "serverinfopage.h"
#include "streamsmodel.h"
#include "playlistspage.h"
#include "fancytabwidget.h"

DeleteKeyEventHandler::DeleteKeyEventHandler(QAbstractItemView *v, QAction *a)
    : QObject(v)
    , view(v)
    , act(a)
{
}

bool DeleteKeyEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (view->hasFocus() && QEvent::KeyRelease==event->type() && static_cast<QKeyEvent *>(event)->matches(QKeySequence::Delete)) {
        act->trigger();
        return true;
    }
    return QObject::eventFilter(obj, event);
}

VolumeSliderEventHandler::VolumeSliderEventHandler(MainWindow *w)
    : QObject(w)
    , window(w)
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

// CoverEventHandler::CoverEventHandler(MainWindow *w)
//     : QObject(w), window(w)
// {
// }
//
// bool CoverEventHandler::eventFilter(QObject *obj, QEvent *event)
// {
//     switch(event->type()) {
//     case QEvent::MouseButtonPress:
//         if (Qt::LeftButton==static_cast<QMouseEvent *>(event)->button()) {
//             pressed=true;
//         }
//         break;
//     case QEvent::MouseButtonRelease:
//         if (pressed && Qt::LeftButton==static_cast<QMouseEvent *>(event)->button()) {
//             window->expandInterfaceAction->trigger();
//         }
//         pressed=false;
//         break;
//     default:
//         break;
//     }
//     return QObject::eventFilter(obj, event);
// }

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
    trayItem = 0;
    lyricsNeedUpdating=false;
    infoNeedsUpdating=false;

    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
    QMenu *mainMenu=new QMenu(this);

#ifndef ENABLE_KDE_SUPPORT
    setWindowIcon(QIcon(":/icons/cantata.svg"));
    setWindowTitle("Cantata");

//     quitAction=menu->addAction(tr("Quit"), qApp, SLOT(quit()));
//     menuAct->setIcon(QIcon::fromTheme("application-exit"));
//     menuBar()->addMenu(menu);
//     menu=new QMenu(tr("Tools"), this);
//     menu=new QMenu(tr("Settings"), this);
//     menuAct=menu->addAction(tr("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
//     menuAct->setIcon(QIcon::fromTheme("configure"));
//     menuBar()->addMenu(menu);
//     menu=new QMenu(tr("Help"), this);
//     menuAct=menu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
//     menuAct->setIcon(windowIcon());
//     menuBar()->addMenu(menu);

    QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::Instance());
#endif

#ifdef ENABLE_KDE_SUPPORT
    KStandardAction::preferences(this, SLOT(showPreferencesDialog()), actionCollection());
    quitAction=KStandardAction::quit(kapp, SLOT(quit()), actionCollection());

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
    addToPlaylistAction->setText(i18n("Add To Play Queue"));

    addToStoredPlaylistAction = actionCollection()->addAction("addtostoredplaylist");
    addToStoredPlaylistAction->setText(i18n("Add To"));

    replacePlaylistAction = actionCollection()->addAction("replaceplaylist");
    replacePlaylistAction->setText(i18n("Replace Play Queue"));

    removeFromPlaylistAction = actionCollection()->addAction("removefromplaylist");
    removeFromPlaylistAction->setText(i18n("Remove"));

    copyTrackInfoAction = actionCollection()->addAction("copytrackinfo");
    copyTrackInfoAction->setText(i18n("Copy Track Info"));

    cropPlaylistAction = actionCollection()->addAction("cropplaylist");
    cropPlaylistAction->setText(i18n("Crop"));

    shufflePlaylistAction = actionCollection()->addAction("shuffleplaylist");
    shufflePlaylistAction->setText(i18n("Shuffle"));

    savePlaylistAction = actionCollection()->addAction("saveplaylist");
    savePlaylistAction->setText(i18n("Save As"));

    clearPlaylistAction = actionCollection()->addAction("clearplaylist");
    clearPlaylistAction->setText(i18n("Clear"));

    expandInterfaceAction = actionCollection()->addAction("expandinterface");
    expandInterfaceAction->setText(i18n("Expanded Interface"));

    randomPlaylistAction = actionCollection()->addAction("randomplaylist");
    randomPlaylistAction->setText(i18n("Random"));

    repeatPlaylistAction = actionCollection()->addAction("repeatplaylist");
    repeatPlaylistAction->setText(i18n("Repeat"));

    consumePlaylistAction = actionCollection()->addAction("consumeplaylist");
    consumePlaylistAction->setText(i18n("Consume"));

    libraryTabAction = actionCollection()->addAction("showlibrarytab");
    libraryTabAction->setText(i18n("Library"));

    albumsTabAction = actionCollection()->addAction("showalbumstab");
    albumsTabAction->setText(i18n("Albums"));

    foldersTabAction = actionCollection()->addAction("showfolderstab");
    foldersTabAction->setText(i18n("Folders"));

    playlistsTabAction = actionCollection()->addAction("showplayliststab");
    playlistsTabAction->setText(i18n("Playlists"));

    lyricsTabAction = actionCollection()->addAction("showlyricstab");
    lyricsTabAction->setText(i18n("Lyrics"));

    streamsTabAction = actionCollection()->addAction("showstreamstab");
    streamsTabAction->setText(i18n("Streams"));

    infoTabAction = actionCollection()->addAction("showinfotab");
    infoTabAction->setText(i18n("Info"));

    serverInfoTabAction = actionCollection()->addAction("showserverinfotab");
    serverInfoTabAction->setText(i18n("Server Info"));
#else
    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    quitAction->setIcon(QIcon::fromTheme("application-exit"));
    quitAction->setShortcut(QKeySequence::Quit);
    updateDbAction = new QAction(tr("Update Database"), this);
    prevTrackAction = new QAction(tr("Previous Track"), this);
    nextTrackAction = new QAction(tr("Next Track"), this);
    playPauseTrackAction = new QAction(tr("Play/Pause"), this);
    stopTrackAction = new QAction(tr("Stop"), this);
    increaseVolumeAction = new QAction(tr("Increase Volume"), this);
    decreaseVolumeAction = new QAction(tr("Decrease Volume"), this);
    addToPlaylistAction = new QAction(tr("Add To Play Queue"), this);
    addToStoredPlaylistAction = new QAction(tr("Add To"), this);
    replacePlaylistAction = new QAction(tr("Replace Play Queue"), this);
    removeFromPlaylistAction = new QAction(tr("Remove"), this);
    copyTrackInfoAction = new QAction(tr("Copy Track Info"), this);
    copyTrackInfoAction->setShortcut(QKeySequence::Copy);
    cropPlaylistAction = new QAction(tr("Crop"), this);
    shufflePlaylistAction = new QAction(tr("Shuffle"), this);
    savePlaylistAction = new QAction(tr("Save As"), this);
    clearPlaylistAction = new QAction(tr("Clear"), this);
    expandInterfaceAction = new QAction(tr("Expanded Interface"), this);
    randomPlaylistAction = new QAction(tr("Random"), this);
    repeatPlaylistAction = new QAction(tr("Repeat"), this);
    consumePlaylistAction = new QAction(tr("Consume"), this);
    libraryTabAction = new QAction(tr("Library"), this);
    albumsTabAction = new QAction(tr("Albums"), this);
    foldersTabAction = new QAction(tr("Folders"), this);
    playlistsTabAction = new QAction(tr("Playlists"), this);
    lyricsTabAction = new QAction(tr("Lyrics"), this);
    streamsTabAction = new QAction(tr("Streams"), this);
    infoTabAction = new QAction(tr("Info"), this);
    serverInfoTabAction = new QAction(tr("Server Info"), this);
#endif
    libraryTabAction->setShortcut(Qt::Key_F5);
    albumsTabAction->setShortcut(Qt::Key_F6);
    foldersTabAction->setShortcut(Qt::Key_F7);
    playlistsTabAction->setShortcut(Qt::Key_F8);
    streamsTabAction->setShortcut(Qt::Key_F9);
    lyricsTabAction->setShortcut(Qt::Key_F10);
    infoTabAction->setShortcut(Qt::Key_F11);
    serverInfoTabAction->setShortcut(Qt::Key_F12);

    // Setup event handler for volume adjustment
    volumeSliderEventHandler = new VolumeSliderEventHandler(this);
//     coverEventHandler = new CoverEventHandler(this);

    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(volumeSliderEventHandler);

    noCover = QIcon::fromTheme("media-optical-audio").pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    noStreamCover = QIcon::fromTheme("applications-internet").pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverWidget->setPixmap(noCover);

    playbackPlay = QIcon::fromTheme("media-playback-start");
    playbackPause = QIcon::fromTheme("media-playback-pause");
    repeatPlaylistAction->setIcon(QIcon::fromTheme("edit-redo"));
    randomPlaylistAction->setIcon(QIcon::fromTheme("media-playlist-shuffle"));
    consumePlaylistAction->setIcon(QIcon::fromTheme("format-list-unordered"));
    addToPlaylistAction->setIcon(QIcon::fromTheme(Qt::RightToLeft==QApplication::layoutDirection() ? "arrow-left" : "arrow-right"));
    replacePlaylistAction->setIcon(QIcon::fromTheme(Qt::RightToLeft==QApplication::layoutDirection() ? "arrow-left-double" : "arrow-right-double"));

    prevTrackAction->setIcon(QIcon::fromTheme("media-skip-backward"));
    nextTrackAction->setIcon(QIcon::fromTheme("media-skip-forward"));
    playPauseTrackAction->setIcon(playbackPlay);
    stopTrackAction->setIcon(QIcon::fromTheme("media-playback-stop"));
    removeFromPlaylistAction->setIcon(QIcon::fromTheme("list-remove"));
    clearPlaylistAction->setIcon(QIcon::fromTheme("edit-clear-list"));
    savePlaylistAction->setIcon(QIcon::fromTheme("document-save-as"));
    clearPlaylistAction->setIcon(QIcon::fromTheme("edit-clear-list"));
    expandInterfaceAction->setIcon(QIcon::fromTheme("view-media-playlist"));
    updateDbAction->setIcon(QIcon::fromTheme("view-refresh"));
    libraryTabAction->setIcon(QIcon::fromTheme("audio-ac3"));
    albumsTabAction->setIcon(QIcon::fromTheme("media-optical-audio"));
    foldersTabAction->setIcon(QIcon::fromTheme("inode-directory"));
    playlistsTabAction->setIcon(QIcon::fromTheme("view-media-playlist"));
    lyricsTabAction->setIcon(QIcon::fromTheme("view-media-lyrics"));
    streamsTabAction->setIcon(QIcon::fromTheme("applications-internet"));
    infoTabAction->setIcon(QIcon::fromTheme("dialog-information"));
    serverInfoTabAction->setIcon(QIcon::fromTheme("server-database"));

    addToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());

    menuButton->setIcon(QIcon::fromTheme("configure"));
    volumeButton->setIcon(QIcon::fromTheme("player-volume"));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            SLOT(cover(const QString &, const QString &, const QImage &)));

    menuButton->setMenu(mainMenu);
    menuButton->setPopupMode(QToolButton::InstantPopup);

    playPauseTrackButton->setDefaultAction(playPauseTrackAction);
    stopTrackButton->setDefaultAction(stopTrackAction);
    nextTrackButton->setDefaultAction(nextTrackAction);
    prevTrackButton->setDefaultAction(prevTrackAction);

    libraryPage = new LibraryPage(this);
    albumsPage = new AlbumsPage(this);
    folderPage = new FolderPage(this);
    playlistsPage = new PlaylistsPage(this);
    streamsPage = new StreamsPage(this);
    lyricsPage = new LyricsPage(this);
    infoPage = new InfoPage(this);
    serverInfoPage = new ServerInfoPage(this);

    connect(&libraryPage->getModel(), SIGNAL(updated(const MusicLibraryItemRoot *)), albumsPage, SLOT(update(const MusicLibraryItemRoot *)));
    connect(&libraryPage->getModel(), SIGNAL(updateGenres(const QStringList &)), albumsPage, SLOT(updateGenres(const QStringList &)));

    setVisible(true);

    savePlaylistPushButton->setDefaultAction(savePlaylistAction);
    removeAllFromPlaylistPushButton->setDefaultAction(clearPlaylistAction);
    removeFromPlaylistPushButton->setDefaultAction(removeFromPlaylistAction);
    randomPushButton->setDefaultAction(randomPlaylistAction);
    repeatPushButton->setDefaultAction(repeatPlaylistAction);
    consumePushButton->setDefaultAction(consumePlaylistAction);

    QStringList hiddenPages=Settings::self()->hiddenPages();
    tabWidget->AddTab(libraryPage, libraryTabAction->icon(), libraryTabAction->text(), !hiddenPages.contains(libraryPage->metaObject()->className()));
    tabWidget->AddTab(albumsPage, albumsTabAction->icon(), albumsTabAction->text(), !hiddenPages.contains(albumsPage->metaObject()->className()));
    tabWidget->AddTab(folderPage, foldersTabAction->icon(), foldersTabAction->text(), !hiddenPages.contains(folderPage->metaObject()->className()));
    tabWidget->AddTab(playlistsPage, playlistsTabAction->icon(), playlistsTabAction->text(), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    tabWidget->AddTab(streamsPage, streamsTabAction->icon(), streamsTabAction->text(), !hiddenPages.contains(streamsPage->metaObject()->className()));
    tabWidget->AddTab(lyricsPage, lyricsTabAction->icon(), lyricsTabAction->text(), !hiddenPages.contains(lyricsPage->metaObject()->className()));
    tabWidget->AddTab(infoPage, infoTabAction->icon(), infoTabAction->text(), !hiddenPages.contains(infoPage->metaObject()->className()));
    tabWidget->AddTab(serverInfoPage, serverInfoTabAction->icon(), serverInfoTabAction->text(), !hiddenPages.contains(serverInfoPage->metaObject()->className()));

    tabWidget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    connect(tabWidget, SIGNAL(CurrentChanged(int)), this, SLOT(currentTabChanged(int)));

    expandInterfaceAction->setCheckable(true);
    randomPlaylistAction->setCheckable(true);
    repeatPlaylistAction->setCheckable(true);
    consumePlaylistAction->setCheckable(true);

#ifdef ENABLE_KDE_SUPPORT
    searchPlaylistLineEdit->setPlaceholderText(i18n("Search Play Queue..."));
#else
    searchPlaylistLineEdit->setPlaceholderText(tr("Search Play Queue..."));
#endif
    QList<QToolButton *> btns;
    btns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton
         << repeatPushButton << randomPushButton << savePlaylistPushButton << removeAllFromPlaylistPushButton
         << removeFromPlaylistPushButton << consumePushButton << volumeButton << menuButton;

    foreach(QToolButton * b, btns) {
        b->setAutoRaise(true);
    }

    trackLabel->setText(QString());
    artistLabel->setText(QString());

    expandInterfaceAction->setChecked(Settings::self()->showPlaylist());
    randomPlaylistAction->setChecked(false);
    repeatPlaylistAction->setChecked(false);
    consumePlaylistAction->setChecked(false);
    mpdDir=Settings::self()->mpdDir();
    lyricsPage->setMpdDir(mpdDir);
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    Covers::self()->setMpdDir(mpdDir);
    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());
    tabWidget->SetMode((FancyTabWidget::Mode)Settings::self()->sidebar());

    setupTrayIcon();
    togglePlaylist();
#ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys | KXmlGuiWindow::Save | KXmlGuiWindow::Create);
    menuBar()->setVisible(false);
#else
    QSize sz=Settings::self()->mainWindowSize();
    if (!sz.isEmpty()) {
        resize(sz);
    }
#endif

    mainMenu->addAction(expandInterfaceAction);
#ifdef ENABLE_KDE_SUPPORT
    QAction *menuAct= mainMenu->addAction(i18n("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
    menuAct->setIcon(QIcon::fromTheme("configure"));
    mainMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)));
    mainMenu->addSeparator();
   mainMenu->addMenu(helpMenu());
#else
    QAction *menuAct=mainMenu->addAction(tr("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
    menuAct->setIcon(QIcon::fromTheme("configure"));
    mainMenu->addSeparator();
//     QMenu *menu=new QMenu(tr("Help"), this);
//     QAction *menuAct=menu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
    menuAct=mainMenu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
    menuAct->setIcon(windowIcon());
//     mainMenu->addMenu(menu);
#endif
    mainMenu->addSeparator();
    mainMenu->addAction(quitAction);

//     coverWidget->installEventFilter(coverEventHandler);

    connect(MPDConnection::self(), SIGNAL(playlistLoaded(const QString &)), SLOT(songLoaded()));
    connect(MPDConnection::self(), SIGNAL(added(const QStringList &)), SLOT(songLoaded()));
    connect(this, SIGNAL(removeSongs(const QList<qint32> &)), MPDConnection::self(), SLOT(removeSongs(const QList<qint32> &)));
    connect(this, SIGNAL(pause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(play()), MPDConnection::self(), SLOT(startPlayingSong()));
    connect(this, SIGNAL(stop()), MPDConnection::self(), SLOT(stopPlaying()));
    connect(this, SIGNAL(getStatus()), MPDConnection::self(), SLOT(getStatus()));
    connect(this, SIGNAL(getStats()), MPDConnection::self(), SLOT(getStats()));
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(this, SIGNAL(playListInfo()), MPDConnection::self(), SLOT(playListInfo()));
    connect(this, SIGNAL(currentSong()), MPDConnection::self(), SLOT(currentSong()));
    connect(this, SIGNAL(setSeekId(quint32, quint32)), MPDConnection::self(), SLOT(setSeekId(quint32, quint32)));
    connect(this, SIGNAL(startPlayingSongId(quint32)), MPDConnection::self(), SLOT(startPlayingSongId(quint32)));
    connect(this, SIGNAL(setDetails(const QString &, quint16, const QString &)), MPDConnection::self(), SLOT(setDetails(const QString &, quint16, const QString &)));

    connect(&playQueueModel, SIGNAL(filesAddedInPlaylist(const QStringList, quint32, quint32)),
            MPDConnection::self(), SLOT(addid(const QStringList, quint32, quint32)));
    connect(&playQueueModel, SIGNAL(moveInPlaylist(const QList<quint32> &, quint32, quint32)),
            MPDConnection::self(), SLOT(move(const QList<quint32> &, quint32, quint32)));
    connect(&playQueueModel, SIGNAL(playListStatsUpdated()), this,
            SLOT(updatePlayListStatus()));

    playQueueProxyModel.setSourceModel(&playQueueModel);
    playQueue->setModel(&playQueueProxyModel);
    playQueue->setAcceptDrops(true);
    playQueue->setDropIndicatorShown(true);
    playQueue->addAction(removeFromPlaylistAction);
    playQueue->addAction(clearPlaylistAction);
    playQueue->addAction(cropPlaylistAction);
    playQueue->addAction(shufflePlaylistAction);
    playQueue->addAction(copyTrackInfoAction);
    playQueue->installEventFilter(new DeleteKeyEventHandler(playQueue, removeFromPlaylistAction));
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playlistItemsSelected(bool)));
    setupPlaylistView();

    connect(MPDConnection::self(), SIGNAL(statsUpdated()), this, SLOT(updateStats()));
    connect(MPDConnection::self(), SIGNAL(statusUpdated()), this, SLOT(updateStatus())/*, Qt::DirectConnection*/);
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlaylist(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(mpdConnectionDied()), this, SLOT(mpdConnectionDied()));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(updateDbAction, SIGNAL(triggered(bool)), this, SLOT(updateDb()));
    connect(updateDbAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(update()));
    connect(prevTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToPrevious()));
    connect(nextTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToNext()));
    connect(playPauseTrackAction, SIGNAL(triggered(bool)), this, SLOT(playPauseTrack()));
    connect(stopTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopTrack()));
    connect(volumeControl, SIGNAL(valueChanged(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(increaseVolumeAction, SIGNAL(triggered(bool)), this, SLOT(increaseVolume()));
    connect(decreaseVolumeAction, SIGNAL(triggered(bool)), this, SLOT(decreaseVolume()));
    connect(increaseVolumeAction, SIGNAL(triggered(bool)), volumeControl, SLOT(increaseVolume()));
    connect(decreaseVolumeAction, SIGNAL(triggered(bool)), volumeControl, SLOT(decreaseVolume()));
    connect(positionSlider, SIGNAL(sliderPressed()), this, SLOT(positionSliderPressed()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(positionSliderReleased()));
    connect(randomPlaylistAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(repeatPlaylistAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(consumePlaylistAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setConsume(bool)));
    connect(searchPlaylistLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPlaylist()));
    connect(searchPlaylistLineEdit, SIGNAL(textChanged(const QString)), this, SLOT(searchPlaylist()));
    connect(playQueue, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playlistItemActivated(const QModelIndex &)));
    connect(addToPlaylistAction, SIGNAL(activated()), this, SLOT(addToPlaylist()));
    connect(replacePlaylistAction, SIGNAL(activated()), this, SLOT(replacePlaylist()));
    connect(removeFromPlaylistAction, SIGNAL(activated()), this, SLOT(removeFromPlaylist()));
    connect(clearPlaylistAction, SIGNAL(activated()), searchPlaylistLineEdit, SLOT(clear()));
    connect(clearPlaylistAction, SIGNAL(activated()), MPDConnection::self(), SLOT(clear()));
    connect(copyTrackInfoAction, SIGNAL(activated()), this, SLOT(copyTrackInfo()));
    connect(cropPlaylistAction, SIGNAL(activated()), this, SLOT(cropPlaylist()));
    connect(shufflePlaylistAction, SIGNAL(activated()), MPDConnection::self(), SLOT(shuffle()));
    connect(expandInterfaceAction, SIGNAL(activated()), this, SLOT(togglePlaylist()));
    connect(&elapsedTimer, SIGNAL(timeout()), this, SLOT(updatePositionSilder()));
    connect(volumeButton, SIGNAL(clicked()), SLOT(showVolumeControl()));
    connect(libraryTabAction, SIGNAL(activated()), this, SLOT(showLibraryTab()));
    connect(albumsTabAction, SIGNAL(activated()), this, SLOT(showAlbumsTab()));
    connect(foldersTabAction, SIGNAL(activated()), this, SLOT(showFoldersTab()));
    connect(playlistsTabAction, SIGNAL(activated()), this, SLOT(showPlaylistsTab()));
    connect(lyricsTabAction, SIGNAL(activated()), this, SLOT(showLyricsTab()));
    connect(streamsTabAction, SIGNAL(activated()), this, SLOT(showStreamsTab()));
    connect(infoTabAction, SIGNAL(activated()), this, SLOT(showInfoTab()));
    connect(serverInfoTabAction, SIGNAL(activated()), this, SLOT(showServerInfoTab()));
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
    connect(playlistsPage, SIGNAL(add(const QStringList &)), &playQueueModel, SLOT(addItems(const QStringList &)));
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

    emit setDetails(Settings::self()->connectionHost(), Settings::self()->connectionPort(), Settings::self()->connectionPasswd());
    playlistItemsSelected(false);
    playQueue->setFocus();

    mpdThread=new QThread(this);
    MPDConnection::self()->moveToThread(mpdThread);
    mpdThread->start();
    MPDConnection::self()->setUi(this); // For questions, errors, etc.

    QString page=Settings::self()->page();

    for (int i=0; i<tabWidget->count(); ++i) {
        if (tabWidget->widget(i)->metaObject()->className()==page) {
            tabWidget->SetCurrentIndex(i);
            break;
        }
    }

    libraryPage->setView(0==Settings::self()->libraryView());
    albumsPage->setView(Settings::self()->albumsView());
    AlbumsModel::setUseLibrarySizes(Settings::self()->albumsView()!=ItemView::Mode_IconTop);
    playlistsPage->setView(0==Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());
    currentTabChanged(tabWidget->current_index());
    playlistsPage->refresh();
}

struct Thread : public QThread
{
    static void sleep() { QThread::msleep(100); }
};

MainWindow::~MainWindow()
{
#ifndef ENABLE_KDE_SUPPORT
    Settings::self()->saveMainWindowSize(size());
#endif
    Settings::self()->saveShowPlaylist(expandInterfaceAction->isChecked());
    Settings::self()->saveSplitterState(splitter->saveState());
    Settings::self()->savePlayQueueHeaderState(playQueueHeader->saveState());
    Settings::self()->saveSidebar((int)(tabWidget->mode()));
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    QStringList hiddenPages;
    for (int i=0; i<tabWidget->count(); ++i) {
        if (!tabWidget->isEnabled(i)) {
            QWidget *w=tabWidget->widget(i);
            if (w) {
                hiddenPages << w->metaObject()->className();
            }
        }
    }
    Settings::self()->saveHiddenPages(hiddenPages);
    streamsPage->save();
    Settings::self()->save(true);
    disconnect(MPDConnection::self(), 0, 0, 0);
    if (Settings::self()->stopOnExit()) {
        emit stop();
        Thread::sleep();
    }
    mpdThread->quit();
    for(int i=0; i<10 && mpdThread->isRunning(); ++i)
        Thread::sleep();
}

void MainWindow::songLoaded()
{
    if (MPDStatus::State_Stopped==MPDStatus::self()->state()) {
        emit play();
    }
}

void MainWindow::showError(const QString &message)
{
    #ifdef ENABLE_KDE_SUPPORT
    KMessageBox::error(this, message);
    #else
    QMessageBox::critical(this, tr("Error"), message);
    #endif
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    if (connected) {
        emit getStatus();
        emit getStats();
        emit playListInfo();
        loaded=0;
        currentTabChanged(tabWidget->current_index());
    } else {
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        lyricsPage->text->clear();
        serverInfoPage->clear();
        showPreferencesDialog();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayItem)
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
    if (playQueue->model()->rowCount()) {
        playQueue->setContextMenuPolicy(Qt::ActionsContextMenu);
        removeFromPlaylistAction->setEnabled(s);
        copyTrackInfoAction->setEnabled(s);
        clearPlaylistAction->setEnabled(true);
        cropPlaylistAction->setEnabled(playQueue->haveUnSelectedItems());
        shufflePlaylistAction->setEnabled(true);
    }
}

void MainWindow::mpdConnectionDied()
{
    // Stop timer(s)
    elapsedTimer.stop();

    // Reset GUI
    positionSlider->setValue(0);
    songTimeElapsedLabel->setText("0:00 / 0:00");

    playPauseTrackAction->setIcon(playbackPlay);
    playPauseTrackAction->setEnabled(true);
    stopTrackAction->setEnabled(false);

    // Show warning message
#ifdef ENABLE_KDE_SUPPORT
    KMessageBox::error(this, i18n("The MPD connection died unexpectedly.<br/>TThis error is unrecoverable, please restart %1.").arg(PACKAGE_NAME),
                       i18n("Lost MPD Connection"));
#else
    QMessageBox::critical(this, tr("MPD connection gone"), tr("The MPD connection died unexpectedly.<br/>This error is unrecoverable, please restart %1.").arg(PACKAGE_NAME));
#endif
}

void MainWindow::updateDb()
{
    if (!updateDialog) {
        updateDialog = new UpdateDialog(this);
        connect(MPDConnection::self(), SIGNAL(databaseUpdated()), updateDialog, SLOT(complete()));
    }
    updateDialog->show();
    emit getStats();
}

void MainWindow::showPreferencesDialog()
{
    static bool showing=false;

    if (!showing) {
        showing=true;
        PreferencesDialog pref(this, lyricsPage);
        connect(&pref, SIGNAL(settingsSaved()), this, SLOT(updateSettings()));

        pref.exec();
        showing=false;
    }
}

void MainWindow::updateSettings()
{
    mpdDir=Settings::self()->mpdDir();
    lyricsPage->setMpdDir(mpdDir);
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    Covers::self()->setMpdDir(mpdDir);
    Settings::self()->save();
    bool useLibSizeForAl=Settings::self()->albumsView()!=ItemView::Mode_IconTop;
    bool diffLibCovers=((int)MusicLibraryItemAlbum::currentCoverSize())!=Settings::self()->libraryCoverSize();
    bool diffAlCovers=((int)AlbumsModel::currentCoverSize())!=Settings::self()->albumsCoverSize() ||
                      albumsPage->viewMode()!=Settings::self()->albumsView() ||
                      useLibSizeForAl!=AlbumsModel::useLibrarySizes();

    if (diffLibCovers) {
        MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    }
    if (diffAlCovers) {
        AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());
    }

    AlbumsModel::setUseLibrarySizes(useLibSizeForAl);
    albumsPage->setView(Settings::self()->albumsView());
    if (diffAlCovers) {
        albumsPage->clear();
    }
    if (diffLibCovers || diffAlCovers) {
        libraryPage->refresh(LibraryPage::RefreshForce);
    }

    streamsPage->refresh();
    libraryPage->setView(0==Settings::self()->libraryView());
    playlistsPage->setView(0==Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());

    setupTrayIcon();
}

#ifndef ENABLE_KDE_SUPPORT
void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("About Cantata"),
                       tr("Simple GUI front-end for MPD.<br/><br/>(c) Craig Drummond 2001.<br/>Released under the GPLv2<br/><br/><i><small>Based upon QtMPC - (C) 2007-2010 The QtMPC Authors</small></i>"));
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

void MainWindow::stopTrack()
{
    emit stop();
    stopTrackAction->setEnabled(false);
}

void MainWindow::playPauseTrack()
{
    MPDStatus * const status = MPDStatus::self();

    if (status->state() == MPDStatus::State_Playing) {
        emit pause(true);
    } else if (status->state() == MPDStatus::State_Paused) {
        emit pause(false);
    } else {
        emit play();
    }
}

void MainWindow::setPosition()
{
    emit setSeekId(MPDStatus::self()->songId(), positionSlider->value());
}

void MainWindow::increaseVolume()
{
    volumeControl->sliderWidget()->triggerAction(QAbstractSlider::SliderPageStepAdd);
}

void MainWindow::decreaseVolume()
{
    volumeControl->sliderWidget()->triggerAction(QAbstractSlider::SliderPageStepSub);
}

void MainWindow::searchPlaylist()
{
    if (searchPlaylistLineEdit->text().isEmpty()) {
        playQueueProxyModel.setFilterRegExp("");
        return;
    }

    playQueueProxyModel.setFilterRegExp(searchPlaylistLineEdit->text());
}

void MainWindow::updatePlaylist(const QList<Song> &songs)
{
    QList<qint32> selectedSongIds;
    qint32 firstSelectedSongId = -1;
    qint32 firstSelectedRow = -1;

    // remember selected song ids and rownum of smallest selected row in proxymodel (because that represents the visible rows)
    if (playQueue->selectionModel()->hasSelection()) {
        QModelIndexList items = playQueue->selectionModel()->selectedRows();
        QModelIndex index;
        QModelIndex sourceIndex;
        // find smallest selected rownum
        for (int i = 0; i < items.size(); i++) {
            index = items.at(i);
            sourceIndex = playQueueProxyModel.mapToSource(index);
            selectedSongIds.append(playQueueModel.getIdByRow(sourceIndex.row()));
            if (firstSelectedRow == -1 || index.row() < firstSelectedRow) {
                firstSelectedRow = index.row();
                firstSelectedSongId = playQueueModel.getIdByRow(sourceIndex.row());
            }
        }
    }

    // refresh playlist
    playQueueModel.updatePlaylist(songs);

    // reselect song ids or minrow if songids were not found (songs removed)
    bool found =  false;
    if (selectedSongIds.size() > 0) {
        qint32 newCurrentRow = playQueueModel.getRowById(firstSelectedSongId);
        QModelIndex newCurrentSourceIndex = playQueueModel.index(newCurrentRow, 0);
        QModelIndex newCurrentIndex = playQueueProxyModel.mapFromSource(newCurrentSourceIndex);
        playQueue->setCurrentIndex(newCurrentIndex);

        qint32 row;
        QModelIndex sourceIndex;
        QModelIndex index;
        for (int i = 0; i < selectedSongIds.size(); i++) {
            row = playQueueModel.getRowById(selectedSongIds.at(i));
            if (row >= 0) {
                found = true;
            }
            sourceIndex = playQueueModel.index(row, 0);
            index = playQueueProxyModel.mapFromSource(sourceIndex);
            playQueue->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        if (!found) {
            // songids which were selected before the playlistupdate were not found anymore (they were removed) -> select firstSelectedRow again (should be the next song after the removed one)
            // firstSelectedRow contains the selected row of the proxymodel before we did the playlist refresh
            // check if rowcount of current proxymodel is smaller than that (last row as removed) and adjust firstSelectedRow when needed
            index = playQueueProxyModel.index(firstSelectedRow, 0);
            if (firstSelectedRow > playQueueProxyModel.rowCount(index) - 1) {
                firstSelectedRow = playQueueProxyModel.rowCount(index) - 1;
            }
            index = playQueueProxyModel.index(firstSelectedRow, 0);
            playQueue->setCurrentIndex(index);
            playQueue->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    if (1==songs.count() && MPDStatus::State_Playing==MPDStatus::self()->state()) {
        updateCurrentSong(songs.at(0));
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

    if (song.name.isEmpty()) {
        trackLabel->setText(song.title);
    } else {
        trackLabel->setText(QString("%1 (%2)").arg(song.title).arg(song.name));
    }
    artistLabel->setText(song.artist);
    playQueueModel.updateCurrentSong(song.id);

    if (PAGE_LYRICS==tabWidget->current_index()) {
        lyricsPage->update(song);
    } else {
        lyricsNeedUpdating=true;
    }

    if (PAGE_INFO==tabWidget->current_index()) {
        infoPage->update(song);
    } else {
        infoNeedsUpdating=true;
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

            if (trayItem) {
                trayItem->showMessage(song.artist + " - " + song.title, text, QSystemTrayIcon::Information, 5000);
            }
#endif
        }
    }
}

void MainWindow::updateStats()
{
    /*
     * Check if remote db is more recent than local one
     * Also update the dirview
     */
    if (lastDbUpdate.isValid() && MPDStats::self()->dbUpdate() > lastDbUpdate) {
        libraryPage->refresh(LibraryPage::RefreshStandard);
        folderPage->refresh();
    }

    lastDbUpdate = MPDStats::self()->dbUpdate();
}

void MainWindow::updateStatus()
{
    MPDStatus * const status = MPDStatus::self();

    if (!draggingPositionSlider) {
        if (status->state() == MPDStatus::State_Stopped
                || status->state() == MPDStatus::State_Inactive) {
            positionSlider->setValue(0);
        } else {
            positionSlider->setRange(0, status->timeTotal());
            positionSlider->setValue(status->timeElapsed());
        }
    }

    int vol=status->volume();
#ifdef ENABLE_KDE_SUPPORT
    volumeButton->setToolTip(i18n("Volume %1%").arg(vol));
    volumeControl->setToolTip(i18n("Volume %1%").arg(vol));
#else
    volumeButton->setToolTip(tr("Volume %1%").arg(vol));
    volumeControl->setToolTip(tr("Volume %1%").arg(vol));
#endif
    volumeButton->setIcon(QIcon::fromTheme("player-volume"));
    volumeControl->setValue(vol);

    if (0==vol) {
        volumeButton->setIcon(QIcon::fromTheme("audio-volume-muted"));
    } else if (vol<=33) {
        volumeButton->setIcon(QIcon::fromTheme("audio-volume-low"));
    } else if (vol<=67) {
        volumeButton->setIcon(QIcon::fromTheme("audio-volume-medium"));
    } else {
        volumeButton->setIcon(QIcon::fromTheme("audio-volume-high"));
    }

    randomPlaylistAction->setChecked(status->random());
    repeatPlaylistAction->setChecked(status->repeat());
    consumePlaylistAction->setChecked(status->consume());

    QString timeElapsedFormattedString;

    if (!currentIsStream()) {
        if (status->state() == MPDStatus::State_Stopped || status->state() == MPDStatus::State_Inactive) {
            timeElapsedFormattedString = "0:00 / 0:00";
        } else {
            timeElapsedFormattedString += Song::formattedTime(status->timeElapsed());
            timeElapsedFormattedString += " / ";
            timeElapsedFormattedString += Song::formattedTime(status->timeTotal());
            songTime = status->timeTotal();
        }
    }

    songTimeElapsedLabel->setText(timeElapsedFormattedString);

    switch (status->state()) {
    case MPDStatus::State_Playing:
        playPauseTrackAction->setIcon(playbackPause);
        playPauseTrackAction->setEnabled(true);
        //playPauseTrackButton->setChecked(false);
        stopTrackAction->setEnabled(true);
        elapsedTimer.start();

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("media-playback-start");
            #else
            trayItem->setIcon(playbackPlay);
            #endif
        }

        break;
    case MPDStatus::State_Inactive:
    case MPDStatus::State_Stopped:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(true);
        stopTrackAction->setEnabled(false);
        trackLabel->setText(QString());
        artistLabel->setText(QString());
        current.id=0;

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("cantata");
            #else
            trayItem->setIcon(windowIcon());
            #endif
        }

        elapsedTimer.stop();
        break;
    case MPDStatus::State_Paused:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(true);
        stopTrackAction->setEnabled(true);

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("media-playback-pause");
            #else
            trayItem->setIcon(playbackPlay);
            #endif
        }

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
            || lastSongId != status->songId()) {
        emit currentSong();
    }
    // Update status info
    lastState = status->state();
    lastSongId = status->songId();
    lastPlaylist = status->playlist();
}

void MainWindow::playlistItemActivated(const QModelIndex &index)
{
    emit startPlayingSongId(playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(index).row()));
}

void MainWindow::removeFromPlaylist()
{
    const QModelIndexList items = playQueue->selectionModel()->selectedRows();
    QModelIndex sourceIndex;
    QList<qint32> toBeRemoved;

    if (items.isEmpty()) {
        return;
    }

    for (int i = 0; i < items.size(); i++) {
        sourceIndex = playQueueProxyModel.mapToSource(items.at(i));
        toBeRemoved.append(playQueueModel.getIdByRow(sourceIndex.row()));
    }

    emit removeSongs(toBeRemoved);
}

void MainWindow::replacePlaylist()
{
    emit clear();
    emit getStatus();
    addToPlaylist();
}

void MainWindow::addToPlaylist()
{
    searchPlaylistLineEdit->clear();
    if (libraryPage->isVisible()) {
        libraryPage->addSelectionToPlaylist();
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToPlaylist();
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToPlaylist();
    } else if (playlistsPage->isVisible()) {
        playlistsPage->addSelectionToPlaylist();
    } else if (streamsPage->isVisible()) {
        streamsPage->addSelectionToPlaylist();
    }
}

void MainWindow::addToNewStoredPlaylist()
{
    for(;;) {
        #ifdef ENABLE_KDE_SUPPORT
        QString name = QInputDialog::getText(this, i18n("Playlist Name"), i18n("Enter a name for the playlist:"));
        #else
        QString name = QInputDialog::getText(this, tr("Playlist Name"), tr("Enter a name for the playlist:"));
        #endif

        if (PlaylistsModel::self()->exists(name)) {
            #ifdef ENABLE_KDE_SUPPORT
            switch(KMessageBox::warningYesNoCancel(this, i18n("A playlist named <b>%1</b> already exists!<br/>Add to that playlist?").arg(name),
                                                   i18n("Existing Playlist"))) {
            case KMessageBox::Cancel:
                return;
            case KMessageBox::Yes:
                break;
            case KMessageBox::No:
            default:
                continue;
            }
            #else
            switch(QMessageBox::warning(this, tr("Existing Playlist"),
                                        tr("A playlist named <b>%1</b> already exists!<br/>Add to that playlist?").arg(name),
                                        QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel, QMessageBox::Yes)) {
            case QMessageBox::Cancel:
                return;
            case QMessageBox::Yes:
                break;
            case QMessageBox::No:
            default:
                continue;
            }
            #endif
        }

        if (!name.isEmpty()) {
            addToExistingStoredPlaylist(name);
        }
        break;
    }
}

void MainWindow::addToExistingStoredPlaylist(const QString &name)
{
    if (libraryPage->isVisible()) {
        libraryPage->addSelectionToPlaylist(name);
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToPlaylist(name);
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToPlaylist(name);
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
    status+=i18np("1 Track", "%1 Tracks", stats->playlistSongs());
#else
    status += QString::number(stats->playlistArtists())+QString(1==stats->playlistArtists() ? " Artist, " : "Artists, ");
    status += QString::number(stats->playlistAlbums())+QString(1==stats->playlistAlbums() ? " Album, " : "Albums, ");
    status += QString::number(stats->playlistSongs())+QString(1==stats->playlistSongs() ? " Track" : "Tracks");
#endif
    status += " (";
    status += MPDParseUtils::formatDuration(stats->playlistTime());
    status += ")";

    playListStatsLabel->setText(status);
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

void MainWindow::copyTrackInfo()
{
    const QModelIndexList items = playQueue->selectionModel()->selectedRows();
    QModelIndex sourceIndex;
    QString txt = "";
    QClipboard *clipboard = QApplication::clipboard();

    if (items.isEmpty()) {
        return;
    }

    for (int i = 0; i < items.size(); i++) {
        sourceIndex = playQueueProxyModel.mapToSource(items.at(i));
        Song s = playQueueModel.getSongByRow(sourceIndex.row());
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
    if (splitter->isVisible()==expandInterfaceAction->isChecked()) {
        return;
    }
    static int lastHeight=0;

    int widthB4=size().width();
    bool showing=expandInterfaceAction->isChecked();

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
void MainWindow::setupPlaylistView()
{
    playQueueHeader = playQueue->header();

    playQueueHeader->setMovable(true);
    playQueueHeader->setResizeMode(QHeaderView::Interactive);
    playQueueHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_TITLE, QHeaderView::Interactive);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_ARTIST, QHeaderView::Interactive);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_ALBUM, QHeaderView::Stretch);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_TRACK, QHeaderView::ResizeToContents);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_LENGTH, QHeaderView::ResizeToContents);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_DISC, QHeaderView::ResizeToContents);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_YEAR, QHeaderView::ResizeToContents);
    playQueueHeader->setStretchLastSection(false);

    connect(playQueueHeader, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(playQueueContextMenuClicked()));
    connect(streamsPage, SIGNAL(add(const QStringList &)), &playQueueModel, SLOT(addItems(const QStringList &)));

    //Restore state
    QByteArray state = Settings::self()->playQueueHeaderState();

    //Restore
    if (state.isEmpty()) {
        playQueueHeader->setSectionHidden(PlayQueueModel::COL_YEAR, true);
        playQueueHeader->setSectionHidden(PlayQueueModel::COL_DISC, true);
        playQueueHeader->setSectionHidden(PlayQueueModel::COL_GENRE, true);
    } else {
        playQueueHeader->restoreState(state);

        for(int i=3; i<PlayQueueModel::COL_COUNT; ++i) {
            if (playQueueHeader->isSectionHidden(i) || playQueueHeader->sectionSize(i) == 0) {
                playQueueHeader->setSectionHidden(i, true);
            }
        }
    }

    playQueueMenu = new QMenu(this);

    for (int col=PlayQueueModel::COL_ALBUM; col<PlayQueueModel::COL_COUNT; ++col) {
        QAction *act=new QAction(playQueueModel.headerData(col, Qt::Horizontal, Qt::DisplayRole).toString(), playQueueMenu);
        act->setCheckable(true);
        act->setChecked(!playQueueHeader->isSectionHidden(col));
        playQueueMenu->addAction(act);
        viewActions.append(act);
        connect(act, SIGNAL(toggled(bool)), this, SLOT(togglePlayQueueHeaderItem(bool)));
    }
}

void MainWindow::playQueueContextMenuClicked()
{
    playQueueMenu->exec(QCursor::pos());
}

void MainWindow::togglePlayQueueHeaderItem(bool visible)
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        int index=viewActions.indexOf(act);
        if (-1!=index) {
            playQueueHeader->setSectionHidden(index+3, !visible);
        }
    }
}

/*
 * Crop playlist
 * Do this by taking the set off all song id's and subtracting from that
 * the set of selected song id's. Feed that list to emit removeSongs
 */
void MainWindow::cropPlaylist()
{
    QSet<qint32> songs = playQueueModel.getSongIdSet();
    QSet<qint32> selected;
    const QModelIndexList items = playQueue->selectionModel()->selectedRows();
    QModelIndex sourceIndex;

    if (items.isEmpty()) {
        return;
    }

    for(int i = 0; i < items.size(); i++) {
        sourceIndex = playQueueProxyModel.mapToSource(items.at(i));
        selected << playQueueModel.getIdByRow(sourceIndex.row());
    }

    QList<qint32> toBeRemoved = (songs - selected).toList();
    emit removeSongs(toBeRemoved);
}

// Tray Icon //
void MainWindow::setupTrayIcon()
{
    if (!Settings::self()->useSystemTray()) {
        if (trayItem) {
            trayItem->deleteLater();
            trayItem=0;
            trayItemMenu->deleteLater();
            trayItemMenu=0;
        }
        return;
    }
#ifdef ENABLE_KDE_SUPPORT
    trayItem = new KStatusNotifierItem(this);
    trayItem->setCategory(KStatusNotifierItem::ApplicationStatus);
    trayItem->setTitle(i18n("Cantata"));
    trayItem->setIconByName("cantata");

    trayItemMenu = new KMenu(this);
    trayItemMenu->addAction(prevTrackAction);
    trayItemMenu->addAction(nextTrackAction);
    trayItemMenu->addAction(stopTrackAction);
    trayItemMenu->addAction(playPauseTrackAction);
    trayItem->setContextMenu(trayItemMenu);
    connect(trayItem, SIGNAL(scrollRequested(int, Qt::Orientation)), this, SLOT(trayItemScrollRequested(int, Qt::Orientation)));
#else
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        trayItem = NULL;
        return;
    }

    trayItem = new QSystemTrayIcon(this);
    trayItem->installEventFilter(volumeSliderEventHandler);
    QMenu *trayItemMenu = new QMenu(trayItem);
    trayItemMenu->addAction(prevTrackAction);
    trayItemMenu->addAction(nextTrackAction);
    trayItemMenu->addAction(stopTrackAction);
    trayItemMenu->addAction(playPauseTrackAction);
    trayItemMenu->addSeparator();
    trayItemMenu->addAction(quitAction);
    trayItem->setContextMenu(trayItemMenu);
    trayItem->setIcon(windowIcon());
    trayItem->setToolTip(tr("Cantata"));
    trayItem->show();
    connect(trayItem, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayItemClicked(QSystemTrayIcon::ActivationReason)));
#endif
}

#ifdef ENABLE_KDE_SUPPORT
void MainWindow::trayItemScrollRequested(int delta, Qt::Orientation orientation)
{
    if (Qt::Vertical==orientation) {
        if (delta>0) {
            increaseVolumeAction->trigger();
        } else if(delta<0) {
            decreaseVolumeAction->trigger();
        }
    }
}
#else
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
#endif

enum Tabs
{
    TAB_LIBRARY = 0x01,
    TAB_FOLDERS = 0x02,
    TAB_STREAMS = 0x04
};

void MainWindow::currentTabChanged(int index)
{
    switch(index) {
    case PAGE_LIBRARY:
    case PAGE_ALBUMS: // Albums shares refresh with library...
        if (!(loaded&TAB_LIBRARY)) {
            loaded|=TAB_LIBRARY;
            libraryPage->refresh(LibraryPage::RefreshFromCache);
        }
        break;
    case PAGE_FOLDERS:
        if (!(loaded&TAB_FOLDERS)) {
            loaded|=TAB_FOLDERS;
            folderPage->refresh();
        }
        break;
    case PAGE_PLAYLISTS:
        break;
    case PAGE_STREAMS:
        if (!(loaded&TAB_STREAMS)) {
            loaded|=TAB_STREAMS;
            streamsPage->refresh();
        }
        break;
    case PAGE_LYRICS:
        if (lyricsNeedUpdating) {
            lyricsPage->update(current);
            lyricsNeedUpdating=false;
        }
        break;
    case PAGE_INFO:
        if (infoNeedsUpdating) {
            infoPage->update(current);
            infoNeedsUpdating=false;
        }
        break;
    case PAGE_SERVER_INFO:
    default:
        break;
    }
}

bool MainWindow::currentIsStream()
{
    return !current.title.isEmpty() && (current.file.isEmpty() || current.file.contains("://"));
}

void MainWindow::cover(const QString &artist, const QString &album, const QImage &img)
{
    if (artist==current.albumArtist() && album==current.album) {
        if (img.isNull()) {
            coverWidget->setPixmap(currentIsStream() ? noStreamCover : noCover);
        } else {
            coverWidget->setPixmap(QPixmap::fromImage(img).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void MainWindow::showTab(int page)
{
    tabWidget->SetCurrentIndex(page);
}
