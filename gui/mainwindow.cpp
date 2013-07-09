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

#include <QSet>
#include <QString>
#include <QTimer>
#include <QClipboard>
#include <QVBoxLayout>
#include <cstdlib>
#ifdef ENABLE_KDE_SUPPORT
#include <kdeversion.h>
#include <KDE/KApplication>
#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KStandardAction>
#include <KDE/KMenuBar>
#include <KDE/KMenu>
#include <KDE/KShortcutsDialog>
#else
#include <QMenuBar>
#include "networkproxyfactory.h"
#endif
#include "localize.h"
#include "mainwindow.h"
#if defined ENABLE_HTTP_STREAM_PLAYBACK && QT_VERSION < 0x050000
#include <phonon/audiooutput.h>
#endif
#include "trayitem.h"
#include "messagebox.h"
#include "inputdialog.h"
#include "playlistsmodel.h"
#include "covers.h"
#include "preferencesdialog.h"
#include "mpdconnection.h"
#include "mpdstats.h"
#include "mpdstatus.h"
#include "mpdparseutils.h"
#include "settings.h"
#include "config.h"
#include "utils.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemalbum.h"
#include "librarypage.h"
#include "albumspage.h"
#include "folderpage.h"
#include "streamspage.h"
#ifdef ENABLE_ONLINE_SERVICES
#include "onlineservicespage.h"
#include "onlineservicesmodel.h"
#endif
#include "gtkstyle.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "filejob.h"
#include "devicespage.h"
#include "devicesmodel.h"
#include "actiondialog.h"
#include "syncdialog.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "audiocddevice.h"
#endif
#endif
#ifdef TAGLIB_FOUND
#include "httpserver.h"
#include "trackorganiser.h"
#include "tageditor.h"
#ifdef ENABLE_REPLAYGAIN_SUPPORT
#include "rgdialog.h"
#endif
#endif
#include "coverdialog.h"
#include "streamsmodel.h"
#include "streamdialog.h"
#include "playlistspage.h"
#include "fancytabwidget.h"
#include "timeslider.h"
#ifndef Q_OS_WIN
#include "mpris.h"
#include "cantataadaptor.h"
#include "gnomemediakeys.h"
#include "mountpoints.h"
#include "gtkproxystyle.h"
#endif
#include "dynamicpage.h"
#include "dynamic.h"
#include "messagewidget.h"
#include "groupedview.h"
#include "actionitemdelegate.h"
#include "icons.h"
#include "volumecontrol.h"
#include "action.h"
#include "actioncollection.h"
#include "stdactions.h"
#ifdef Q_OS_WIN
static void raiseWindow(QWidget *w);
#endif

enum Tabs
{
    TAB_LIBRARY = 0x01,
    TAB_FOLDERS = 0x02,
    TAB_STREAMS = 0x04
};

bool DeleteKeyEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (view->hasFocus() && QEvent::KeyRelease==event->type() && static_cast<QKeyEvent *>(event)->matches(QKeySequence::Delete)) {
        act->trigger();
        return true;
    }
    return QObject::eventFilter(obj, event);
}

bool VolumeSliderEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (QEvent::Wheel==event->type() && (!MPDConnection::self()->isMuted() || !qstrcmp("VolumeControl", obj->metaObject()->className()))) {
        int numDegrees = static_cast<QWheelEvent *>(event)->delta() / 8;
        int numSteps = numDegrees / 15;
        if (numSteps > 0) {
            for (int i = 0; i < numSteps; ++i) {
                window->increaseVolumeAction->trigger();
            }
        } else {
            for (int i = 0; i > numSteps; --i) {
                window->decreaseVolumeAction->trigger();
            }
        }
        return true;
    }

    return QObject::eventFilter(obj, event);
}

bool VolumeButtonEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (QEvent::MouseButtonPress==event->type() && Qt::MiddleButton==static_cast<QMouseEvent *>(event)->buttons()) {
        down=true;
    } else if (QEvent::MouseButtonRelease==event->type()) {
        if (down) {
            window->muteAction->trigger();
        }
        down=false;
    }
    return VolumeSliderEventHandler::eventFilter(obj, event);
}

static int nextKey(int &key) {
    int k=key;

    if (Qt::Key_0==key) {
        key=Qt::Key_A;
    } else {
        ++key;
        if (Qt::Key_Colon==key) {
            key=Qt::Key_0;
        }
    }
    return k;
}

MainWindow::MainWindow(QWidget *parent)
    #ifdef ENABLE_KDE_SUPPORT
    : KXmlGuiWindow(parent)
    #else
    : QMainWindow(parent)
    #endif
    , loaded(0)
    , lastState(MPDState_Inactive)
    , lastSongId(-1)
    , autoScrollPlayQueue(true)
    #if !defined Q_OS_WIN
    , mpris(0)
    , gnomeMediaKeys(0)
    #endif
    , playQueueSearchTimer(0)
    #ifdef Q_OS_LINUX
    , mpdAccessibilityTimer(0)
    #endif
    , connectedState(CS_Init)
    , stopAfterCurrent(false)
    , volumeFade(0)
    , origVolume(0)
    , lastVolume(0)
    , stopState(StopState_None)
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    , httpStreamEnabled(false)
    , httpStream(0)
    #endif
{
    QPoint p=pos();

    ActionCollection::setMainWidget(this);
    trayItem=new TrayItem(this);

    #ifndef Q_OS_WIN
    new CantataAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/cantata", this);
    #endif
    setMinimumHeight(256);
    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
    QMenu *mainMenu=new QMenu(this);

    messageWidget->hide();

    // Need to set these values here, as used in library/device loading...
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    MPDParseUtils::setGroupMultiple(Settings::self()->groupMultiple());

    GtkStyle::applyTheme(toolbar);
    Icons::self()->self()->initToolbarIcons(artistLabel->palette().color(QPalette::Foreground), GtkStyle::useLightIcons());
    Icons::self()->initSidebarIcons();
    menuButton->setIcon(Icons::self()->toolbarMenuIcon);

    // With ambiance (which has a drak toolbar) we need a gap between the toolbar and the earch fields. But, in the context view we dont
    // want a gap - as this looks odd with a background. To workaround this, the tabwidget and playqueue sides of the splitter have a
    // spacer added. The size of this needs to be controllable by the style - so we do this here...
    int spacing=Utils::layoutSpacing(this);
    if (tabWidgetSpacer->minimumSize().height()!=spacing) {
        tabWidgetSpacer->changeSize(spacing, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed);
        playQueueSpacer->changeSize(spacing, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    #ifdef ENABLE_KDE_SUPPORT
    prefAction=static_cast<Action *>(KStandardAction::preferences(this, SLOT(showPreferencesDialog()), ActionCollection::get()));
    quitAction=static_cast<Action *>(KStandardAction::quit(this, SLOT(quit()), ActionCollection::get()));
    #else
    setWindowIcon(Icons::self()->appIcon);
    QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::self());

    quitAction = ActionCollection::get()->createAction("quit", i18n("Quit"), "application-exit");
    connect(quitAction, SIGNAL(triggered(bool)), this, SLOT(quit()));
    quitAction->setShortcut(QKeySequence::Quit);
    #endif // ENABLE_KDE_SUPPORT
    restoreAction = ActionCollection::get()->createAction("showwindow", i18n("Show Window"));
    connect(restoreAction, SIGNAL(triggered(bool)), this, SLOT(restoreWindow()));

    connectAction = ActionCollection::get()->createAction("connect", i18n("Connect"), Icons::self()->connectIcon);
    connectionsAction = ActionCollection::get()->createAction("connections", i18n("Collection"), "network-server");
    outputsAction = ActionCollection::get()->createAction("outputs", i18n("Outputs"), Icons::self()->speakerIcon);
    prevTrackAction = ActionCollection::get()->createAction("prevtrack", i18n("Previous Track"), Icons::self()->toolbarPrevIcon);
    nextTrackAction = ActionCollection::get()->createAction("nexttrack", i18n("Next Track"), Icons::self()->toolbarNextIcon);
    playPauseTrackAction = ActionCollection::get()->createAction("playpausetrack", i18n("Play/Pause"), Icons::self()->toolbarPlayIcon);
    stopPlaybackAction = ActionCollection::get()->createAction("stopplayback", i18n("Stop"), Icons::self()->toolbarStopIcon);
    stopAfterCurrentTrackAction = ActionCollection::get()->createAction("stopaftercurrenttrack", i18n("Stop After Current Track"), Icons::self()->toolbarStopIcon);
    stopAfterTrackAction = ActionCollection::get()->createAction("stopaftertrack", i18n("Stop After Track"), Icons::self()->toolbarStopIcon);
    increaseVolumeAction = ActionCollection::get()->createAction("increasevolume", i18n("Increase Volume"));
    decreaseVolumeAction = ActionCollection::get()->createAction("decreasevolume", i18n("Decrease Volume"));
    muteAction = ActionCollection::get()->createAction("mute", i18n("Mute"));
    addPlayQueueToStoredPlaylistAction = ActionCollection::get()->createAction("addpqtostoredplaylist", i18n("Add To Stored Playlist"), Icons::self()->playlistIcon);
    removeFromPlayQueueAction = ActionCollection::get()->createAction("removefromplaylist", i18n("Remove From Play Queue"), "list-remove");
    copyTrackInfoAction = ActionCollection::get()->createAction("copytrackinfo", i18n("Copy Track Info"));
    cropPlayQueueAction = ActionCollection::get()->createAction("cropplaylist", i18n("Crop"));
    shufflePlayQueueAction = ActionCollection::get()->createAction("shuffleplaylist", i18n("Shuffle"));
    addStreamToPlayQueueAction = ActionCollection::get()->createAction("addstreamtoplayqueue", i18n("Add Stream URL"), Icons::self()->addRadioStreamIcon);
    clearPlayQueueAction = ActionCollection::get()->createAction("clearplaylist", i18n("Clear"), Icons::self()->clearListIcon);
    expandInterfaceAction = ActionCollection::get()->createAction("expandinterface", i18n("Expanded Interface"), "view-media-playlist");
    songInfoAction = ActionCollection::get()->createAction("showsonginfo", i18n("Show Current Song Information"), Icons::self()->infoIcon);
    songInfoAction->setShortcut(Qt::Key_F12);
    songInfoAction->setCheckable(true);
    fullScreenAction = ActionCollection::get()->createAction("fullScreen", i18n("Full Screen"), "view-fullscreen");
    fullScreenAction->setShortcut(Qt::Key_F11);
    randomPlayQueueAction = ActionCollection::get()->createAction("randomplaylist", i18n("Random"), Icons::self()->shuffleIcon);
    repeatPlayQueueAction = ActionCollection::get()->createAction("repeatplaylist", i18n("Repeat"), Icons::self()->repeatIcon);
    singlePlayQueueAction = ActionCollection::get()->createAction("singleplaylist", i18n("Single"), Icons::self()->singleIcon, i18n("When 'Single' is activated, playback is stopped after current song, or song is repeated if 'Repeat' is enabled."));
    consumePlayQueueAction = ActionCollection::get()->createAction("consumeplaylist", i18n("Consume"), Icons::self()->consumeIcon, i18n("When consume is activated, a song is removed from the play queue after it has been played."));
    searchPlayQueueAction = ActionCollection::get()->createAction("searchplaylist", i18n("Search Play Queue"), Icons::self()->searchIcon);
    searchPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_F);
    setPriorityAction = ActionCollection::get()->createAction("setprio", i18n("Set Priority"), Icon("favorites"));
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction = ActionCollection::get()->createAction("streamplay", i18n("Play Stream"));
    #endif
    locateTrackAction = ActionCollection::get()->createAction("locatetrack", i18n("Locate In Library"), "edit-find");
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction = ActionCollection::get()->createAction("editpqtags", i18n("Edit Song Tags"), "document-edit");
    #endif
    showPlayQueueAction = ActionCollection::get()->createAction("showplayqueue", i18n("Play Queue"), Icons::self()->playqueueIcon);
    addAction(libraryTabAction = ActionCollection::get()->createAction("showlibrarytab", i18n("Artists"), Icons::self()->artistsIcon));
    addAction(albumsTabAction = ActionCollection::get()->createAction("showalbumstab", i18n("Albums"), Icons::self()->albumsIcon));
    addAction(foldersTabAction = ActionCollection::get()->createAction("showfolderstab", i18n("Folders"), Icons::self()->foldersIcon));
    addAction(playlistsTabAction = ActionCollection::get()->createAction("showplayliststab", i18n("Playlists"), Icons::self()->playlistsIcon));
    addAction(dynamicTabAction = ActionCollection::get()->createAction("showdynamictab", i18n("Dynamic"), Icons::self()->dynamicIcon));
    addAction(streamsTabAction = ActionCollection::get()->createAction("showstreamstab", i18n("Streams"), Icons::self()->streamsIcon));
    #ifdef ENABLE_ONLINE_SERVICES
    addAction(onlineTabAction = ActionCollection::get()->createAction("showonlinetab", i18n("Online"), Icons::self()->onlineIcon));
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    addAction(devicesTabAction = ActionCollection::get()->createAction("showdevicestab", i18n("Devices"), Icons::self()->devicesIcon));
    #endif
    expandAllAction = ActionCollection::get()->createAction("expandall", i18n("Expand All"));
    collapseAllAction = ActionCollection::get()->createAction("collapseall", i18n("Collapse All"));

    playPauseTrackAction->setEnabled(false);
    nextTrackAction->setEnabled(false);
    updateNextTrack(-1);
    prevTrackAction->setEnabled(false);
    enableStopActions(false);

    #if defined ENABLE_KDE_SUPPORT
    prevTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Left));
    nextTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Right));
    playPauseTrackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_C));
    stopPlaybackAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_X));
    stopAfterCurrentTrackAction->setGlobalShortcut(KShortcut());
    increaseVolumeAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Up));
    decreaseVolumeAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Down));
    muteAction->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_M));
    #endif

    copyTrackInfoAction->setShortcut(QKeySequence::Copy);
    expandAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Plus);
    collapseAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Minus);

    int pageKey=Qt::Key_1;
    showPlayQueueAction->setShortcut(Qt::AltModifier+Qt::Key_Q);
    libraryTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    albumsTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    foldersTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    playlistsTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    dynamicTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    streamsTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    #ifdef ENABLE_ONLINE_SERVICES
    onlineTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setShortcut(Qt::AltModifier+nextKey(pageKey));
    #endif // ENABLE_DEVICES_SUPPORT

    volumeSliderEventHandler = new VolumeSliderEventHandler(this);
    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installEventFilter(volumeSliderEventHandler);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(new VolumeButtonEventHandler(this));
    volumeButton->setMenu(volumeControl);

    connectionsAction->setMenu(new QMenu(this));
    connectionsGroup=new QActionGroup(connectionsAction->menu());
    outputsAction->setMenu(new QMenu(this));
    outputsAction->setVisible(false);
    addPlayQueueToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());

    menuButton->setMenu(mainMenu);
    volumeButton->setIcon(Icons::self()->toolbarVolumeHighIcon);

    playPauseTrackButton->setDefaultAction(playPauseTrackAction);
    stopTrackButton->setDefaultAction(stopPlaybackAction);
    nextTrackButton->setDefaultAction(nextTrackAction);
    prevTrackButton->setDefaultAction(prevTrackAction);

    QMenu *stopMenu=new QMenu(this);
    stopMenu->addAction(stopPlaybackAction);
    stopMenu->addAction(stopAfterCurrentTrackAction);
    stopTrackButton->setMenu(stopMenu);
    stopTrackButton->setPopupMode(QToolButton::DelayedPopup);

    libraryPage = new LibraryPage(this);
    albumsPage = new AlbumsPage(this);
    folderPage = new FolderPage(this);
    playlistsPage = new PlaylistsPage(this);
    dynamicPage = new DynamicPage(this);
    streamsPage = new StreamsPage(this);
    #ifdef ENABLE_ONLINE_SERVICES
    onlinePage = new OnlineServicesPage(this);
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage = new DevicesPage(this);
    #endif

    initSizes();
    clearPlayQueueAction->setEnabled(false);
    StdActions::self()->savePlayQueueAction->setEnabled(false);
    addStreamToPlayQueueAction->setEnabled(false);
    clearPlayQueueButton->setDefaultAction(clearPlayQueueAction);
    savePlayQueueButton->setDefaultAction(StdActions::self()->savePlayQueueAction);
    searchPlayQueueButton->setDefaultAction(searchPlayQueueAction);
    randomButton->setDefaultAction(randomPlayQueueAction);
    repeatButton->setDefaultAction(repeatPlayQueueAction);
    singleButton->setDefaultAction(singlePlayQueueAction);
    consumeButton->setDefaultAction(consumePlayQueueAction);

    #define TAB_ACTION(A) A->icon(), A->text(), A->text()

    QStringList hiddenPages=Settings::self()->hiddenPages();
    playQueuePage=new PlayQueuePage(this);
    contextPage=new ContextPage(this);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, playQueuePage);
    layout->setContentsMargins(0, 0, 0, 0);
    bool playQueueInSidebar=!hiddenPages.contains(playQueuePage->metaObject()->className());
    if (playQueueInSidebar && (Settings::self()->firstRun() || Settings::self()->version()<CANTATA_MAKE_VERSION(0, 8, 0))) {
        playQueueInSidebar=false;
    }
    bool contextInSidebar=!hiddenPages.contains(contextPage->metaObject()->className());
    if (contextInSidebar && (Settings::self()->firstRun() || Settings::self()->version()<CANTATA_MAKE_VERSION(1, 0, 52))) {
        contextInSidebar=false;
    }
    layout=new QBoxLayout(QBoxLayout::TopToBottom, contextPage);
    layout->setContentsMargins(0, 0, 0, 0);
    tabWidget->AddTab(playQueuePage, TAB_ACTION(showPlayQueueAction), playQueueInSidebar);
    tabWidget->AddTab(libraryPage, TAB_ACTION(libraryTabAction), !hiddenPages.contains(libraryPage->metaObject()->className()));
    tabWidget->AddTab(albumsPage, TAB_ACTION(albumsTabAction), !hiddenPages.contains(albumsPage->metaObject()->className()));
    tabWidget->AddTab(folderPage, TAB_ACTION(foldersTabAction), !hiddenPages.contains(folderPage->metaObject()->className()));
    tabWidget->AddTab(playlistsPage, TAB_ACTION(playlistsTabAction), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    tabWidget->AddTab(dynamicPage, TAB_ACTION(dynamicTabAction), !hiddenPages.contains(dynamicPage->metaObject()->className()));
    tabWidget->AddTab(streamsPage, TAB_ACTION(streamsTabAction), !hiddenPages.contains(streamsPage->metaObject()->className()));
    #ifdef ENABLE_ONLINE_SERVICES
    tabWidget->AddTab(onlinePage, TAB_ACTION(onlineTabAction), !hiddenPages.contains(onlinePage->metaObject()->className()));
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    tabWidget->AddTab(devicesPage, TAB_ACTION(devicesTabAction), !hiddenPages.contains(devicesPage->metaObject()->className()));
    DevicesModel::self()->setEnabled(!hiddenPages.contains(devicesPage->metaObject()->className()));
    #endif
    tabWidget->AddTab(contextPage, Icons::self()->infoSidebarIcon, i18n("Info"), songInfoAction->text(),
                      !hiddenPages.contains(contextPage->metaObject()->className()));
    AlbumsModel::self()->setEnabled(!hiddenPages.contains(albumsPage->metaObject()->className()));
    folderPage->setEnabled(!hiddenPages.contains(folderPage->metaObject()->className()));
    streamsPage->setEnabled(!hiddenPages.contains(streamsPage->metaObject()->className()));
    #ifdef ENABLE_ONLINE_SERVICES
    onlinePage->setEnabled(!hiddenPages.contains(onlinePage->metaObject()->className()));
    #endif

    autoHideSplitterAction=new QAction(i18n("Auto Hide"), this);
    autoHideSplitterAction->setCheckable(true);
    autoHideSplitterAction->setChecked(Settings::self()->splitterAutoHide());
    tabWidget->addMenuAction(autoHideSplitterAction);
    connect(autoHideSplitterAction, SIGNAL(toggled(bool)), this, SLOT(toggleSplitterAutoHide()));

    monoIconsAction=new QAction(i18n("Monochrome Icons"), this);
    monoIconsAction->setCheckable(true);
    monoIconsAction->setChecked(Settings::self()->monoSidebarIcons());
    tabWidget->addStyleAction(monoIconsAction);
    connect(monoIconsAction, SIGNAL(toggled(bool)), this, SLOT(toggleMonoIcons()));

    if (playQueueInSidebar) {
        tabToggled(PAGE_PLAYQUEUE);
    } else {
        tabWidget->SetCurrentIndex(PAGE_LIBRARY);
    }

    if (contextInSidebar) {
        tabToggled(PAGE_CONTEXT);
    } else {
        tabWidget->ToggleTab(PAGE_CONTEXT, false);
    }

    tabWidget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    expandInterfaceAction->setCheckable(true);
    fullScreenAction->setCheckable(true);
    randomPlayQueueAction->setCheckable(true);
    repeatPlayQueueAction->setCheckable(true);
    singlePlayQueueAction->setCheckable(true);
    consumePlayQueueAction->setCheckable(true);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction->setCheckable(true);
    #endif

    songInfoButton->setDefaultAction(songInfoAction);
    playQueueSearchWidget->setVisible(false);
    QList<QToolButton *> playbackBtns;
    QList<QToolButton *> controlBtns;
    playbackBtns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    controlBtns << volumeButton << menuButton << songInfoButton;

    int playbackIconSize=28;
    int controlIconSize=22;
    int buttonSize=32;

    if (repeatButton->iconSize().height()>=32) {
        controlIconSize=48;
        playbackIconSize=48;
        buttonSize=54;
    } else if (repeatButton->iconSize().height()>=22) {
        controlIconSize=32;
        playbackIconSize=32;
        buttonSize=36;
    } else if (QLatin1String("oxygen")!=QIcon::themeName()) {
        // Oxygen does not have 24x24 icons, and media players eem to use scaled 28x28 icons...
        // But, if the theme does have media icons at 24x24 use these - as they will be sharper...
        Icon icon("media-playback-start");
        playbackIconSize=24==icon.actualSize(QSize(24, 24)).width() ? 24 : 28;
    }

    foreach (QToolButton *b, controlBtns) {
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setMinimumSize(QSize(buttonSize, buttonSize));
        b->setMaximumSize(QSize(buttonSize, buttonSize));
        b->setIconSize(QSize(controlIconSize, controlIconSize));
    }
    foreach (QToolButton *b, playbackBtns) {
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setMinimumSize(QSize(buttonSize, buttonSize));
        b->setMaximumSize(QSize(buttonSize, buttonSize));
        b->setIconSize(QSize(playbackIconSize, playbackIconSize));
    }

    trackLabel->setText(QString());
    artistLabel->setText(QString());

    expandInterfaceAction->setChecked(Settings::self()->showPlaylist());
    fullScreenAction->setEnabled(expandInterfaceAction->isChecked());
    if (fullScreenAction->isEnabled()) {
        fullScreenAction->setChecked(Settings::self()->showFullScreen());
    }
    randomPlayQueueAction->setChecked(false);
    repeatPlayQueueAction->setChecked(false);
    singlePlayQueueAction->setChecked(false);
    consumePlayQueueAction->setChecked(false);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction->setChecked(false);
    #endif

    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    MusicLibraryItemAlbum::setShowDate(Settings::self()->libraryYear());
    AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());

    tabWidget->SetMode((FancyTabWidget::Mode)Settings::self()->sidebar());
    expandedSize=Settings::self()->mainWindowSize();
    collapsedSize=Settings::self()->mainWindowCollapsedSize();

    if (Settings::self()->firstRun()) {
        int width=playPauseTrackButton->width()*25;
        resize(playPauseTrackButton->width()*25, playPauseTrackButton->height()*18);
        splitter->setSizes(QList<int>() << width*0.4 << width*0.6);
    } else {
        if (expandInterfaceAction->isChecked()) {
            if (!expandedSize.isEmpty() && expandedSize.width()>0) {
                resize(expandedSize);
                expandOrCollapse(false);
            }
        } else {
            if (!collapsedSize.isEmpty() && collapsedSize.width()>0) {
                resize(collapsedSize);
                expandOrCollapse(false);
            }
        }
        if (!playQueueInSidebar) {
            QByteArray state=Settings::self()->splitterState();
            if (!state.isEmpty()) {
                splitter->restoreState(Settings::self()->splitterState());
            }
        }
        if (fullScreenAction->isChecked()) {
            fullScreen();
        }
    }

    #ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys);
    #endif

    mainMenu->addAction(expandInterfaceAction);
    mainMenu->addAction(songInfoAction);
    mainMenu->addAction(fullScreenAction);
    mainMenu->addAction(connectionsAction);
    mainMenu->addAction(outputsAction);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    mainMenu->addAction(streamPlayAction);
    #endif
    serverInfoAction=ActionCollection::get()->createAction("mpdinfo", i18n("Server information..."), "network-server");
    connect(serverInfoAction, SIGNAL(triggered(bool)),this, SLOT(showServerInfo()));
    serverInfoAction->setEnabled(Settings::self()->firstRun());
    #ifdef ENABLE_KDE_SUPPORT
    mainMenu->addAction(prefAction);
    shortcutsAction=static_cast<Action *>(KStandardAction::keyBindings(this, SLOT(configureShortcuts()), ActionCollection::get()));
    mainMenu->addAction(shortcutsAction);
    mainMenu->addSeparator();
    mainMenu->addAction(serverInfoAction);
    mainMenu->addMenu(helpMenu());
    #else
    prefAction=ActionCollection::get()->createAction("configure", i18n("Configure Cantata..."), Icons::self()->configureIcon);
    connect(prefAction, SIGNAL(triggered(bool)),this, SLOT(showPreferencesDialog()));
    mainMenu->addAction(prefAction);
    mainMenu->addSeparator();
    Action *aboutAction=ActionCollection::get()->createAction("about", i18nc("Qt-only", "About Cantata..."), Icons::self()->appIcon);
    connect(aboutAction, SIGNAL(triggered(bool)),this, SLOT(showAboutDialog()));
    mainMenu->addAction(serverInfoAction);
    mainMenu->addAction(aboutAction);
    #endif
    mainMenu->addSeparator();
    mainMenu->addAction(quitAction);

    #if !defined Q_OS_WIN
    if (qgetenv("XDG_CURRENT_DESKTOP")=="Unity") {
        QMenu *menu=new QMenu(i18n("&File"), this);
        menu->addAction(quitAction);
        menuBar()->addMenu(menu);
        menu=new QMenu(i18n("&Settings"), this);
        menu->addAction(expandInterfaceAction);
        menu->addAction(songInfoAction);
        menu->addAction(fullScreenAction);
        menu->addAction(connectionsAction);
        menu->addAction(outputsAction);
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        menu->addAction(streamPlayAction);
        #endif
        menu->addSeparator();
        #ifdef ENABLE_KDE_SUPPORT
        menu->addAction(shortcutsAction);
        #endif
        menu->addAction(prefAction);
        menuBar()->addMenu(menu);
        menu=new QMenu(i18n("&Help"), this);
        menu->addAction(serverInfoAction);
        #ifdef ENABLE_KDE_SUPPORT
        menu->addSeparator();
        foreach (QAction *act, helpMenu()->actions()) {
            menu->addAction(act);
        }
        #else
        menu->addAction(aboutAction);
        #endif
        menuBar()->addMenu(menu);
    }
    #endif

    dynamicLabel->setVisible(false);
    stopDynamicButton->setVisible(false);
    stopDynamicButton->setDefaultAction(Dynamic::self()->stopAct());
    StdActions::self()->addWithPriorityAction->setVisible(false);
    setPriorityAction->setVisible(false);
    setPriorityAction->setMenu(StdActions::self()->addWithPriorityAction->menu());

    // Ensure these objects are created in the GUI thread...
    MPDStatus::self();
    MPDStats::self();

    playQueueProxyModel.setSourceModel(&playQueueModel);
    playQueue->setModel(&playQueueProxyModel);
    playQueue->addAction(removeFromPlayQueueAction);
    playQueue->addAction(clearPlayQueueAction);
    playQueue->addAction(StdActions::self()->savePlayQueueAction);
    playQueue->addAction(addStreamToPlayQueueAction);
    playQueue->addAction(addPlayQueueToStoredPlaylistAction);
    playQueue->addAction(cropPlayQueueAction);
    playQueue->addAction(shufflePlayQueueAction);
    Action *sep=new Action(this);
    sep->setSeparator(true);
    playQueue->addAction(sep);
    playQueue->addAction(stopAfterTrackAction);
    playQueue->addAction(setPriorityAction);
    playQueue->addAction(locateTrackAction);
    #ifdef TAGLIB_FOUND
    playQueue->addAction(editPlayQueueTagsAction);
    #endif
    //playQueue->addAction(copyTrackInfoAction);
    playQueue->tree()->installEventFilter(new DeleteKeyEventHandler(playQueue->tree(), removeFromPlayQueueAction));
    playQueue->list()->installEventFilter(new DeleteKeyEventHandler(playQueue->list(), removeFromPlayQueueAction));

    playQueue->setGrouped(Settings::self()->playQueueGrouped());
    playQueue->setAutoExpand(Settings::self()->playQueueAutoExpand());
    playQueue->setStartClosed(Settings::self()->playQueueStartClosed());
    playQueue->setUseCoverAsBackgrond(Settings::self()->playQueueBackground());

    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());

    connect(StdActions::self()->addPrioHighestAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioHighAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioMediumAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioLowAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioDefaultAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioCustomAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(MPDConnection::self(), SIGNAL(playlistLoaded(const QString &)), SLOT(songLoaded()));
    connect(MPDConnection::self(), SIGNAL(added(const QStringList &)), SLOT(songLoaded()));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(outputsUpdated(const QList<Output> &)));
    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(removeSongs(const QList<qint32> &)), MPDConnection::self(), SLOT(removeSongs(const QList<qint32> &)));
    connect(this, SIGNAL(pause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(play()), MPDConnection::self(), SLOT(startPlayingSong()));
    connect(this, SIGNAL(stop(bool)), MPDConnection::self(), SLOT(stopPlaying(bool)));
    connect(this, SIGNAL(getStatus()), MPDConnection::self(), SLOT(getStatus()));
    connect(this, SIGNAL(getStats(bool)), MPDConnection::self(), SLOT(getStats(bool)));
    connect(this, SIGNAL(updateMpd()), MPDConnection::self(), SLOT(update()));
    connect(this, SIGNAL(clear()), MPDConnection::self(), SLOT(clear()));
    connect(this, SIGNAL(playListInfo()), MPDConnection::self(), SLOT(playListInfo()));
    connect(this, SIGNAL(currentSong()), MPDConnection::self(), SLOT(currentSong()));
    connect(this, SIGNAL(setSeekId(qint32, quint32)), MPDConnection::self(), SLOT(setSeekId(qint32, quint32)));
    connect(this, SIGNAL(startPlayingSongId(qint32)), MPDConnection::self(), SLOT(startPlayingSongId(qint32)));
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(this, SIGNAL(setPriority(const QList<qint32> &, quint8 )), MPDConnection::self(), SLOT(setPriority(const QList<qint32> &, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(this, SIGNAL(addAndPlay(QString)), MPDConnection::self(), SLOT(addAndPlay(QString)));
    connect(&playQueueModel, SIGNAL(statsUpdated(int, quint32)), this, SLOT(updatePlayQueueStats(int, quint32)));
    connect(&playQueueModel, SIGNAL(fetchingStreams()), playQueue, SLOT(showSpinner()));
    connect(&playQueueModel, SIGNAL(streamsFetched()), playQueue, SLOT(hideSpinner()));
    connect(&playQueueModel, SIGNAL(updateCurrent(Song)), SLOT(updateCurrentSong(Song)));
    connect(&playQueueModel, SIGNAL(streamFetchStatus(QString)), playQueue, SLOT(streamFetchStatus(QString)));
    connect(playQueue, SIGNAL(cancelStreamFetch()), &playQueueModel, SLOT(cancelStreamFetch()));
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playQueueItemsSelected(bool)));
    connect(streamsPage, SIGNAL(add(const QStringList &, bool, quint8)), &playQueueModel, SLOT(addItems(const QStringList &, bool, quint8)));
    connect(streamsPage, SIGNAL(error(QString)), this, SLOT(showError(QString)));
    connect(MPDStats::self(), SIGNAL(updated()), this, SLOT(updateStats()));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlayQueue(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
    connect(MPDConnection::self(), SIGNAL(info(const QString &)), SLOT(showInformation(const QString &)));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(checkMpdDir()));
    connect(Dynamic::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(Dynamic::self(), SIGNAL(running(bool)), dynamicLabel, SLOT(setVisible(bool)));
    connect(Dynamic::self(), SIGNAL(running(bool)), this, SLOT(controlDynamicButton()));
    connect(StdActions::self()->refreshAction, SIGNAL(triggered(bool)), this, SLOT(refresh()));
    connect(StdActions::self()->refreshAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(update()));
    connect(connectAction, SIGNAL(triggered(bool)), this, SLOT(connectToMpd()));
    connect(prevTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToPrevious()));
    connect(nextTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToNext()));
    connect(playPauseTrackAction, SIGNAL(triggered(bool)), this, SLOT(playPauseTrack()));
    connect(stopPlaybackAction, SIGNAL(triggered(bool)), this, SLOT(stopPlayback()));
    connect(stopAfterCurrentTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopAfterCurrentTrack()));
    connect(stopAfterTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopAfterTrack()));
    connect(volumeControl, SIGNAL(valueChanged(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(increaseVolumeAction, SIGNAL(triggered(bool)), this, SLOT(increaseVolume()));
    connect(decreaseVolumeAction, SIGNAL(triggered(bool)), this, SLOT(decreaseVolume()));
    connect(increaseVolumeAction, SIGNAL(triggered(bool)), volumeControl, SLOT(increaseVolume()));
    connect(decreaseVolumeAction, SIGNAL(triggered(bool)), volumeControl, SLOT(decreaseVolume()));
    connect(muteAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(toggleMute()));
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(randomPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(repeatPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(singlePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setSingle(bool)));
    connect(consumePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setConsume(bool)));
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    connect(streamPlayAction, SIGNAL(triggered(bool)), this, SLOT(toggleStream(bool)));
    #endif
    connect(StdActions::self()->backAction, SIGNAL(triggered(bool)), this, SLOT(goBack()));
    connect(playQueueSearchWidget, SIGNAL(returnPressed()), this, SLOT(searchPlayQueue()));
    connect(playQueueSearchWidget, SIGNAL(textChanged(const QString)), this, SLOT(searchPlayQueue()));
    connect(playQueue, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playQueueItemActivated(const QModelIndex &)));
    connect(StdActions::self()->removeAction, SIGNAL(triggered(bool)), this, SLOT(removeItems()));
    connect(StdActions::self()->addToPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(addToPlayQueue()));
    connect(StdActions::self()->replacePlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(replacePlayQueue()));
    connect(removeFromPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(removeFromPlayQueue()));
    connect(clearPlayQueueAction, SIGNAL(triggered(bool)), playQueueSearchWidget, SLOT(clear()));
    connect(clearPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(clear()));
    connect(copyTrackInfoAction, SIGNAL(triggered(bool)), this, SLOT(copyTrackInfo()));
    connect(cropPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(cropPlayQueue()));
    connect(shufflePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(shuffle()));
    connect(expandInterfaceAction, SIGNAL(triggered(bool)), this, SLOT(expandOrCollapse()));
    connect(songInfoAction, SIGNAL(triggered(bool)), this, SLOT(showSongInfo()));
    connect(fullScreenAction, SIGNAL(triggered(bool)), this, SLOT(fullScreen()));
    #ifdef TAGLIB_FOUND
    connect(StdActions::self()->editTagsAction, SIGNAL(triggered(bool)), this, SLOT(editTags()));
    connect(editPlayQueueTagsAction, SIGNAL(triggered(bool)), this, SLOT(editPlayQueueTags()));
    connect(StdActions::self()->organiseFilesAction, SIGNAL(triggered(bool)), SLOT(organiseFiles()));
    #endif
    connect(context, SIGNAL(findArtist(QString)), this, SLOT(locateArtist(QString)));
    connect(context, SIGNAL(findAlbum(QString,QString)), this, SLOT(locateAlbum(QString,QString)));
    connect(context, SIGNAL(playSong(QString)), this, SLOT(playSong(QString)));
    connect(locateTrackAction, SIGNAL(triggered(bool)), this, SLOT(locateTrack()));
    connect(showPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(showPlayQueue()));
    connect(libraryTabAction, SIGNAL(triggered(bool)), this, SLOT(showLibraryTab()));
    connect(albumsTabAction, SIGNAL(triggered(bool)), this, SLOT(showAlbumsTab()));
    connect(foldersTabAction, SIGNAL(triggered(bool)), this, SLOT(showFoldersTab()));
    connect(playlistsTabAction, SIGNAL(triggered(bool)), this, SLOT(showPlaylistsTab()));
    connect(dynamicTabAction, SIGNAL(triggered(bool)), this, SLOT(showDynamicTab()));
    connect(streamsTabAction, SIGNAL(triggered(bool)), this, SLOT(showStreamsTab()));
    #ifdef ENABLE_ONLINE_SERVICES
    connect(onlineTabAction, SIGNAL(triggered(bool)), this, SLOT(showOnlineTab()));
    #endif
    addAction(StdActions::self()->searchAction); // Weird, but if I dont add thiis action, it does not work!!!!
    connect(StdActions::self()->searchAction, SIGNAL(triggered(bool)), SLOT(showSearch()));
    connect(searchPlayQueueAction, SIGNAL(triggered(bool)), playQueueSearchWidget, SLOT(toggle()));
    connect(expandAllAction, SIGNAL(triggered(bool)), this, SLOT(expandAll()));
    connect(collapseAllAction, SIGNAL(triggered(bool)), this, SLOT(collapseAll()));
    #ifdef ENABLE_DEVICES_SUPPORT
    connect(devicesTabAction, SIGNAL(triggered(bool)), this, SLOT(showDevicesTab()));
    connect(DevicesModel::self(), SIGNAL(addToDevice(const QString &)), this, SLOT(addToDevice(const QString &)));
    connect(DevicesModel::self(), SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    connect(libraryPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(playlistsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    #ifdef ENABLE_ONLINE_SERVICES
    connect(onlinePage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    #endif
    connect(StdActions::self()->deleteSongsAction, SIGNAL(triggered(bool)), SLOT(deleteSongs()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(addStreamToPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(addStreamToPlayQueue()));
    #endif
    connect(StdActions::self()->setCoverAction, SIGNAL(triggered(bool)), SLOT(setCover()));
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    connect(StdActions::self()->replaygainAction, SIGNAL(triggered(bool)), SLOT(replayGain()));
    #endif
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
    connect(playlistsPage, SIGNAL(add(const QStringList &, bool, quint8)), &playQueueModel, SLOT(addItems(const QStringList &, bool, quint8)));
    connect(coverWidget, SIGNAL(clicked()), expandInterfaceAction, SLOT(trigger()));
    connect(coverWidget, SIGNAL(albumCover(QImage)), playQueue, SLOT(setImage(QImage)));
    #ifdef Q_OS_LINUX
    connect(MountPoints::self(), SIGNAL(updated()), SLOT(checkMpdAccessibility()));
    #endif // Q_OS_LINUX

    playQueueItemsSelected(false);
    playQueue->setFocus();
    playQueue->initHeader();

    MPDConnection::self()->start();
    connectToMpd();

    QString page=Settings::self()->page();
    for (int i=0; i<tabWidget->count(); ++i) {
        if (tabWidget->widget(i)->metaObject()->className()==page) {
            tabWidget->SetCurrentIndex(i);
            break;
        }
    }

    connect(tabWidget, SIGNAL(CurrentChanged(int)), this, SLOT(currentTabChanged(int)));
    connect(tabWidget, SIGNAL(TabToggled(int)), this, SLOT(tabToggled(int)));
    connect(tabWidget, SIGNAL(ModeChanged(FancyTabWidget::Mode)), this, SLOT(sidebarModeChanged()));
    connect(messageWidget, SIGNAL(visible(bool)), this, SLOT(messageWidgetVisibility(bool)));

    toggleSplitterAutoHide();
    readSettings();
    updateConnectionsMenu();
    fadeStop=Settings::self()->stopFadeDuration()>Settings::MinFade;
    playlistsPage->refresh();
    #if !defined Q_OS_WIN
    mpris=new Mpris(this);
    connect(coverWidget, SIGNAL(coverFile(const QString &)), mpris, SLOT(updateCurrentCover(const QString &)));
    #endif
    ActionCollection::get()->readSettings();

    if (testAttribute(Qt::WA_TranslucentBackground)) {
        // Work-around non-showing main window on start-up with transparent QtCurve windows.
        // BUG: 146
        move(p.isNull() ? QPoint(96, 96) : p);
    }

    // If this is the first run, then the wizard will have done the MPD connection. But this will not have loaded the model!
    // So, we need to load this now - which is done in currentTabChanged()
    if (Settings::self()->firstRun() ||
        (PAGE_LIBRARY!=tabWidget->current_index() && PAGE_ALBUMS!=tabWidget->current_index() &&
         PAGE_FOLDERS!=tabWidget->current_index() && PAGE_PLAYLISTS!=tabWidget->current_index())) {
        currentTabChanged(tabWidget->current_index());
    }
    if (Settings::self()->firstRun() && MPDConnection::self()->isConnected()) {
        mpdConnectionStateChanged(true);
    }
}

MainWindow::~MainWindow()
{
    Settings::self()->saveShowFullScreen(fullScreenAction->isChecked());
    if (!fullScreenAction->isChecked()) {
        Settings::self()->saveMainWindowSize(expandInterfaceAction->isChecked() ? size() : expandedSize);
        Settings::self()->saveMainWindowCollapsedSize(expandInterfaceAction->isChecked() ? collapsedSize : size());
    }
    #if defined ENABLE_REMOTE_DEVICES && defined ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->unmountRemote();
    #endif
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    Settings::self()->savePlayStream(streamPlayAction->isChecked());
    #endif
    if (!fullScreenAction->isChecked()) {
        if (!tabWidget->isEnabled(PAGE_PLAYQUEUE)) {
            Settings::self()->saveSplitterState(splitter->saveState());
        }
        Settings::self()->saveShowPlaylist(expandInterfaceAction->isChecked());
    }
    Settings::self()->saveSplitterAutoHide(autoHideSplitterAction->isChecked());
    Settings::self()->saveSidebar((int)(tabWidget->mode()));
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    playQueue->saveHeader();
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
    context->saveConfig();
    streamsPage->save();
    Settings::self()->saveForceSingleClick(TreeView::getForceSingleClick());
    Settings::self()->saveStartHidden(trayItem->isActive() && isHidden() && Settings::self()->minimiseOnClose());
    Settings::self()->save(true);
    disconnect(MPDConnection::self(), 0, 0, 0);
    if (Settings::self()->stopDynamizerOnExit()) {
        Dynamic::self()->stop();
    }
    if (Settings::self()->stopOnExit() || (fadeWhenStop() && StopState_Stopping==stopState)) {
        emit stop();
        Utils::sleep();
    }
    MPDConnection::self()->stop();
    Covers::self()->stop();
    #if defined ENABLE_DEVICES_SUPPORT
    FileThread::self()->stop();
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    OnlineServicesModel::self()->stop();
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->stop();
    #endif
}

void MainWindow::initSizes()
{
    ItemView::setup();
    FancyTabWidget::setup();
    GroupedView::setup();
    ActionItemDelegate::setup();
    MusicLibraryItemAlbum::setup();

    // Calculate size for cover widget...
    int spacing=Utils::layoutSpacing(this);
    playPauseTrackButton->adjustSize();
    trackLabel->adjustSize();
    artistLabel->adjustSize();
    positionSlider->adjustSize();
    int cwSize=qMax(playPauseTrackButton->height(), trackLabel->height()+artistLabel->height()+spacing)+positionSlider->height()+spacing;

    coverWidget->setMinimumSize(cwSize, cwSize);
    coverWidget->setMaximumSize(cwSize, cwSize);
}

void MainWindow::load(const QStringList &urls)
{
    #ifdef TAGLIB_FOUND
    QStringList useable;
    bool haveHttp=HttpServer::self()->isAlive();

    foreach (const QString &path, urls) {
        QUrl u(path);
        #if defined ENABLE_DEVICES_SUPPORT && (defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND)
        QString cdDevice=AudioCdDevice::getDevice(u);
        if (!cdDevice.isEmpty()) {
            DevicesModel::self()->playCd(cdDevice);
        } else
        #endif
        if (QLatin1String("http")==u.scheme()) {
            useable.append(u.toString());
        } else if (haveHttp && (u.scheme().isEmpty() || QLatin1String("file")==u.scheme())) {
            useable.append(HttpServer::self()->encodeUrl(u.path()));
        }
    }
    if (useable.count()) {
        playQueueModel.addItems(useable, playQueueModel.rowCount(), false, 0);
    }
    #else
    Q_UNUSED(urls)
    #endif
}

void MainWindow::songLoaded()
{
    // was song was loaded from commandline when empty...
    bool isInitial=-1==playQueueModel.currentSong() && MPDState_Inactive==lastState && MPDState_Inactive==MPDStatus::self()->state();
    if (MPDState_Stopped==MPDStatus::self()->state() || isInitial) {
        stopVolumeFade();
        if (isInitial) {
            emit play();
        }
    }
}

void MainWindow::showError(const QString &message, bool showActions)
{
    if (QLatin1String("NO_SONGS")==message) {
        messageWidget->setError(i18n("Failed to locate any songs matching the dynamic playlist rules."));
    } else {
        messageWidget->setError(message);
    }
    if (showActions) {
        messageWidget->addAction(prefAction);
        messageWidget->addAction(connectAction);
    } else {
        messageWidget->removeAction(prefAction);
        messageWidget->removeAction(connectAction);
    }
}

void MainWindow::showInformation(const QString &message)
{
    messageWidget->setInformation(message);
    messageWidget->removeAction(prefAction);
    messageWidget->removeAction(connectAction);
}

void MainWindow::messageWidgetVisibility(bool v)
{
    Q_UNUSED(v)

    if (!expandInterfaceAction->isChecked()) {
        int prevWidth=width();
        int compactHeight=calcCompactHeight();
        setFixedHeight(compactHeight);
        resize(prevWidth, compactHeight);
    }
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    serverInfoAction->setEnabled(connected);
    addStreamToPlayQueueAction->setEnabled(connected);
    if (connected) {
        messageWidget->hide();
        if (CS_Connected!=connectedState) {
            emit playListInfo();
            emit outputs();
            if (CS_Init!=connectedState) {
                loaded=(loaded&TAB_STREAMS);
                currentTabChanged(tabWidget->current_index());
            }
            connectedState=CS_Connected;
            StdActions::self()->addWithPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
            setPriorityAction->setVisible(StdActions::self()->addWithPriorityAction->isVisible());
        } else {
            updateWindowTitle();
        }
    } else {
        loaded=(loaded&TAB_STREAMS);
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        connectedState=CS_Disconnected;
        outputsAction->setVisible(false);
        MPDStatus dummyStatus;
        updateStatus(&dummyStatus);
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if ((Qt::Key_Enter==event->key() || Qt::Key_Return==event->key()) &&
        playQueue->hasFocus() && !playQueue->selectionModel()->selectedRows().isEmpty()) {
        //play the first selected song
        QModelIndexList selection=playQueue->selectionModel()->selectedRows();
        qSort(selection);
        playQueueItemActivated(selection.first());
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayItem->isActive() && Settings::self()->minimiseOnClose()) {
        lastPos=pos();
        hide();
        if (event->spontaneous()) {
            event->ignore();
        }
    } else {
        GtkStyle::cleanup();
        #ifdef ENABLE_KDE_SUPPORT
        KXmlGuiWindow::closeEvent(event);
        #else
        QMainWindow::closeEvent(event);
        #endif
    }
}

void MainWindow::playQueueItemsSelected(bool s)
{
    bool haveItems=playQueue->model()->rowCount()>0;
    bool singleSelection=1==playQueue->selectedIndexes().count();
    removeFromPlayQueueAction->setEnabled(s && haveItems);
    locateTrackAction->setEnabled(singleSelection);
    copyTrackInfoAction->setEnabled(s && haveItems);
    cropPlayQueueAction->setEnabled(playQueue->haveUnSelectedItems() && haveItems);
    shufflePlayQueueAction->setEnabled(haveItems);
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction->setEnabled(s && haveItems);
    #endif
    addPlayQueueToStoredPlaylistAction->setEnabled(haveItems);
    stopAfterTrackAction->setEnabled(singleSelection);
}

void MainWindow::connectToMpd(const MPDConnectionDetails &details)
{
    if (!MPDConnection::self()->isConnected() || details!=MPDConnection::self()->getDetails()) {
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        if (!MPDConnection::self()->getDetails().isEmpty() && details!=MPDConnection::self()->getDetails()) {
            Dynamic::self()->stop();
        }
        showInformation(i18n("Connecting to %1").arg(details.description()));
        outputsAction->setVisible(false);
        if (CS_Init!=connectedState) {
            connectedState=CS_Disconnected;
        }
    }
    emit setDetails(details);
}

void MainWindow::connectToMpd()
{
    connectToMpd(Settings::self()->connectionDetails());
}

void MainWindow::refresh()
{
    MusicLibraryModel::self()->removeCache();
    emit getStats(true);
}

#ifdef ENABLE_KDE_SUPPORT
void MainWindow::configureShortcuts()
{
    KShortcutsDialog dlg(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsDisallowed, this);
    dlg.addCollection(ActionCollection::get());
    connect(&dlg, SIGNAL(okClicked()), this, SLOT(saveShortcuts()));
    dlg.exec();
}

void MainWindow::saveShortcuts()
{
    ActionCollection::get()->writeSettings();
}

#endif

bool MainWindow::canShowDialog()
{
    if (PreferencesDialog::instanceCount() || CoverDialog::instanceCount()
        #ifdef TAGLIB_FOUND
        || TagEditor::instanceCount() || TrackOrganiser::instanceCount()
        #endif
        #ifdef ENABLE_DEVICES_SUPPORT
         || ActionDialog::instanceCount() || SyncDialog::instanceCount()
        #endif
        #ifdef ENABLE_REPLAYGAIN_SUPPORT
         || RgDialog::instanceCount()
        #endif
        ) {
        MessageBox::error(this, i18n("Please close other dialogs first."));
        return false;
    }
    return true;
}

void MainWindow::showPreferencesDialog()
{
    if (PreferencesDialog::instanceCount() || !canShowDialog()) {
        return;
    }
    PreferencesDialog *pref=new PreferencesDialog(this);
    controlConnectionsMenu(false);
    connect(pref, SIGNAL(settingsSaved()), this, SLOT(updateSettings()));
    connect(pref, SIGNAL(reloadStreams()), streamsPage, SLOT(refresh()));
    connect(pref, SIGNAL(destroyed()), SLOT(controlConnectionsMenu()));
    pref->show();
}

void MainWindow::quit()
{
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (RgDialog::instanceCount()) {
        return;
    }
    #endif
    #ifdef TAGLIB_FOUND
    if (TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        return;
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (ActionDialog::instanceCount() || 0!=SyncDialog::instanceCount()) {
        return;
    }
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    kapp->quit();
    #else
    qApp->quit();
    #endif
}

void MainWindow::checkMpdDir()
{
    #ifdef Q_OS_LINUX
    if (mpdAccessibilityTimer) {
        mpdAccessibilityTimer->stop();
    }
    QString dir=MPDConnection::self()->getDetails().dir;
    MPDConnection::self()->setDirReadable(dir.isEmpty() ? false : QDir(dir).isReadable());
    #endif

    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction->setEnabled(MPDConnection::self()->getDetails().dirReadable);
    #endif
    switch (tabWidget->current_index()) {
    #if defined ENABLE_DEVICES_SUPPORT && defined TAGLIB_FOUND
    case PAGE_DEVICES:   devicesPage->controlActions();    break;
    #endif
    case PAGE_LIBRARY:   libraryPage->controlActions();    break;
    case PAGE_ALBUMS:    albumsPage->controlActions();     break;
    case PAGE_FOLDERS:   folderPage->controlActions();     break;
    case PAGE_PLAYLISTS: playlistsPage->controlActions();  break;
    case PAGE_DYNAMIC:   dynamicPage->controlActions();    break;
    case PAGE_STREAMS:   streamsPage->controlActions();    break;
    #ifdef ENABLE_ONLINE_SERVICES
    case PAGE_ONLINE:    onlinePage->controlActions();     break;
    #endif
    default:                                               break;
    }
}

void MainWindow::outputsUpdated(const QList<Output> &outputs)
{
    if (outputs.count()<2) {
        outputsAction->setVisible(false);
    } else {
        outputsAction->setVisible(true);
        QSet<QString> mpd;
        QSet<QString> menuItems;
        QMenu *menu=outputsAction->menu();
        foreach (const Output &o, outputs) {
            mpd.insert(o.name);
        }

        foreach (QAction *act, menu->actions()) {
            menuItems.insert(act->data().toString());
        }

        if (menuItems!=mpd) {
            menu->clear();
            QList<Output> out=outputs;
            qSort(out);
            int i=Qt::Key_1;
            foreach (const Output &o, out) {
                QAction *act=menu->addAction(o.name, this, SLOT(toggleOutput()));
                act->setData(o.id);
                act->setCheckable(true);
                act->setChecked(o.enabled);
                act->setShortcut(Qt::MetaModifier+nextKey(i));
            }
        } else {
            foreach (const Output &o, outputs) {
                foreach (QAction *act, menu->actions()) {
                    if (Utils::strippedText(act->text())==o.name) {
                        act->setChecked(o.enabled);
                        break;
                    }
                }
            }
        }
    }
}

void MainWindow::updateConnectionsMenu()
{
    QList<MPDConnectionDetails> connections=Settings::self()->allConnections();
    if (connections.count()<2) {
        connectionsAction->setVisible(false);
    } else {
        connectionsAction->setVisible(true);
        QSet<QString> cfg;
        QSet<QString> menuItems;
        QMenu *menu=connectionsAction->menu();
        foreach (const MPDConnectionDetails &d, connections) {
            cfg.insert(d.name);
        }

        foreach (QAction *act, menu->actions()) {
            menuItems.insert(act->data().toString());
        }

        if (menuItems!=cfg) {
            menu->clear();
            qSort(connections);
            QString current=Settings::self()->currentConnection();
            int i=Qt::Key_1;
            foreach (const MPDConnectionDetails &d, connections) {
                QAction *act=menu->addAction(d.getName(), this, SLOT(changeConnection()));
                act->setData(d.name);
                act->setCheckable(true);
                act->setChecked(d.name==current);
                act->setActionGroup(connectionsGroup);
                act->setShortcut(Qt::ControlModifier+nextKey(i));
            }
        }
    }
}

void MainWindow::controlConnectionsMenu(bool enable)
{
    if (enable) {
        updateConnectionsMenu();
    }

    foreach(QAction *act, connectionsAction->menu()->actions()) {
        act->setEnabled(enable);
    }
}

void MainWindow::controlDynamicButton()
{
    stopDynamicButton->setVisible(dynamicLabel->isVisible() && PAGE_DYNAMIC!=tabWidget->current_index());
}

void MainWindow::readSettings()
{
    checkMpdDir();
    #ifdef TAGLIB_FOUND
    Covers::self()->setSaveInMpdDir(Settings::self()->storeCoversInMpdDir());
    HttpServer::self()->readConfig();
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    MPDParseUtils::setGroupMultiple(Settings::self()->groupMultiple());
    albumsPage->setView(Settings::self()->albumsView());
    AlbumsModel::self()->setAlbumSort(Settings::self()->albumSort());

    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction->setVisible(!Settings::self()->streamUrl().isEmpty());
    streamPlayAction->setChecked(streamPlayAction->isVisible() && Settings::self()->playStream());
    toggleStream(streamPlayAction->isChecked());
    #endif
    libraryPage->setView(Settings::self()->libraryView());
    MusicLibraryModel::self()->setUseArtistImages(Settings::self()->libraryArtistImage());
    playlistsPage->setView(Settings::self()->playlistsView());
    streamsPage->setView(Settings::self()->streamsView());
    #ifdef ENABLE_ONLINE_SERVICES
    onlinePage->setView(Settings::self()->onlineView());
    #endif
    folderPage->setView(Settings::self()->folderView());
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(Settings::self()->devicesView());
    #endif
    trayItem->setup();
    autoScrollPlayQueue=Settings::self()->playQueueScroll();
    updateWindowTitle();
    TreeView::setForceSingleClick(Settings::self()->forceSingleClick());
    #ifndef Q_OS_WIN
    if (!gnomeMediaKeys && Settings::self()->gnomeMediaKeys()) {
        gnomeMediaKeys=new GnomeMediaKeys(this);
    }
    if (gnomeMediaKeys) {
        gnomeMediaKeys->setEnabled(Settings::self()->gnomeMediaKeys());
    }
    #endif
    context->readConfig();
}

void MainWindow::updateSettings()
{
    int stopFadeDuration=Settings::self()->stopFadeDuration();
    fadeStop=stopFadeDuration>Settings::MinFade;
    if (volumeFade) {
        volumeFade->setDuration(stopFadeDuration);
    }

    connectToMpd();
    Settings::self()->save();
    bool useLibSizeForAl=Settings::self()->albumsView()!=ItemView::Mode_IconTop;
    bool diffLibCovers=((int)MusicLibraryItemAlbum::currentCoverSize())!=Settings::self()->libraryCoverSize();
    bool diffLibArtistImages=diffLibCovers ||
                       (libraryPage->viewMode()==ItemView::Mode_IconTop && Settings::self()->libraryView()!=ItemView::Mode_IconTop) ||
                       (libraryPage->viewMode()!=ItemView::Mode_IconTop && Settings::self()->libraryView()==ItemView::Mode_IconTop) ||
                       Settings::self()->libraryArtistImage()!=MusicLibraryModel::self()->useArtistImages();
    bool diffAlCovers=((int)AlbumsModel::currentCoverSize())!=Settings::self()->albumsCoverSize() ||
                      albumsPage->viewMode()!=Settings::self()->albumsView() ||
                      useLibSizeForAl!=AlbumsModel::useLibrarySizes();
    bool diffLibYear=MusicLibraryItemAlbum::showDate()!=Settings::self()->libraryYear();
    bool diffGrouping=MPDParseUtils::groupSingle()!=Settings::self()->groupSingle() ||
                      MPDParseUtils::groupMultiple()!=Settings::self()->groupMultiple();

    readSettings();

    if (diffLibArtistImages) {
        MusicLibraryItemArtist::clearDefaultCover();
        libraryPage->setView(libraryPage->viewMode());
    }
    if (diffLibCovers) {
        MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    }
    if (diffLibYear) {
        MusicLibraryItemAlbum::setShowDate(Settings::self()->libraryYear());
    }
    if (diffAlCovers) {
        AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());
    }

    AlbumsModel::setUseLibrarySizes(useLibSizeForAl);
    if (diffAlCovers || diffGrouping) {
        albumsPage->setView(albumsPage->viewMode());
        albumsPage->clear();
    }

    if (diffGrouping) {
        MusicLibraryModel::self()->toggleGrouping();
        #ifdef ENABLE_DEVICES_SUPPORT
        DevicesModel::self()->toggleGrouping();
        #endif
    }

    if (diffLibCovers || diffLibYear || diffLibArtistImages || diffAlCovers) {
        libraryPage->clear();
        albumsPage->goTop();
        loaded|=TAB_LIBRARY;
        libraryPage->refresh();
    }

    bool wasAutoExpand=playQueue->isAutoExpand();
    bool wasStartClosed=playQueue->isStartClosed();
    bool wasShowingCover=playQueue->useCoverAsBackground();
    playQueue->setAutoExpand(Settings::self()->playQueueAutoExpand());
    playQueue->setStartClosed(Settings::self()->playQueueStartClosed());
    playQueue->setUseCoverAsBackgrond(Settings::self()->playQueueBackground());
    if (!wasShowingCover && playQueue->useCoverAsBackground() && coverWidget->isValid()) {
        playQueue->setImage(coverWidget->image());
    }

    if (Settings::self()->playQueueGrouped()!=playQueue->isGrouped() ||
        (playQueue->isGrouped() && (wasAutoExpand!=playQueue->isAutoExpand() || wasStartClosed!=playQueue->isStartClosed())) ) {
        playQueue->setGrouped(Settings::self()->playQueueGrouped());
        QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
        playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && MPDState_Playing==MPDStatus::self()->state());
    }

    wasStartClosed=playlistsPage->isStartClosed();
    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());
    if (ItemView::Mode_GroupedTree==Settings::self()->playlistsView() && wasStartClosed!=playlistsPage->isStartClosed()) {
        playlistsPage->updateRows();
    }
}

void MainWindow::toggleOutput()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        emit enableOutput(act->data().toInt(), act->isChecked());
    }
}

void MainWindow::changeConnection()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        Settings::self()->saveCurrentConnection(act->data().toString());
        connectToMpd();
    }
}

#ifndef ENABLE_KDE_SUPPORT
void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, i18nc("Qt-only", "About Cantata"),
                       i18nc("Qt-only", "<b>Cantata %1</b><br/><br/>MPD client.<br/><br/>"
                             "(c) Craig Drummond 2011-2013.<br/>Released under the <a href=\"http://www.gnu.org/licenses/gpl.html\">GPLv3</a>").arg(PACKAGE_VERSION_STRING)+
                       QLatin1String("<br/><br/><i><small>")+i18n("Based upon <a href=\"http://qtmpc.lowblog.nl\">QtMPC</a> - (C) 2007-2010 The QtMPC Authors<br/>")+
//                       (ContextWidget::constHtbApiKey.latin1() ? i18nc("Qt-only", "Context view backdrops courtesy of <a href=\"http://www.htbackdrops.com\">Home Theater Backdrops</a><br/>") : "")+
                       i18nc("Qt-only", "Context view backdrops courtesy of <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+QLatin1String("<br/>")+
                       i18nc("Qt-only", "Context view metadata courtesy of <a href=\"http://www.wikipedia.org\">Wikipedia</a> and <a href=\"http://www.last.fm\">Last.fm</a>")+
                       QLatin1String("<br/><br/>")+i18n("Please consider uploading your own music fan-art to <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+
                       QLatin1String("</small></i>"));
}
#endif

void MainWindow::showServerInfo()
{
    QStringList handlers=MPDConnection::self()->urlHandlers().toList();
    qSort(handlers);
    long version=MPDConnection::self()->version();
    MessageBox::information(this, i18n("<p><table>"
                                       "<tr><td colspan=\"2\"><b>Server</b></td></tr>"
                                       "<tr><td align=\"right\">Version:</td><td>%1.%2.%3</td></tr>"
                                       "<tr><td align=\"right\">Uptime:</td><td>%4</td></tr>"
                                       "<tr><td align=\"right\">Time playing:</td><td>%5</td></tr>"
                                       "<tr/>"
                                       "<tr><td colspan=\"2\"><b>Database</b></td></tr>"
                                       "<tr><td align=\"right\">Artists:</td><td>%6</td></tr>"
                                       "<tr><td align=\"right\">Albums:</td><td>%7</td></tr>"
                                       "<tr><td align=\"right\">Songs:</td><td>%8</td></tr>"
                                       "<tr><td align=\"right\">URL handlers:</td><td>%9</td></tr>"
                                       "<tr><td align=\"right\">Total duration:</td><td>%10</td></tr>"
                                       "<tr><td align=\"right\">Last update:</td><td>%11</td></tr></table></p>")
                                       .arg((version>>16)&0xFF).arg((version>>8)&0xFF).arg(version&0xFF)
                                       .arg(MPDParseUtils::formatDuration(MPDStats::self()->uptime()))
                                       .arg(MPDParseUtils::formatDuration(MPDStats::self()->playtime()))
                                       .arg(MPDStats::self()->artists())
                                       .arg(MPDStats::self()->albums())
                                       .arg(MPDStats::self()->songs())
                                       .arg(handlers.join(", "))
                                       .arg(MPDParseUtils::formatDuration(MPDStats::self()->dbPlaytime()))
                                       .arg(MPDStats::self()->dbUpdate().toString(Qt::SystemLocaleShortDate)),
                            i18n("Server Information"));
}

void MainWindow::toggleStream(bool s)
{
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    MPDStatus * const status = MPDStatus::self();
    httpStreamEnabled = s;
    if (!s){
        if (httpStream) {
            httpStream->stop();
        }
    } else {
        static const char *constUrlProperty="url";
        if (httpStream && httpStream->property(constUrlProperty).toString()!=Settings::self()->streamUrl()) {
            httpStream->stop();
            httpStream->deleteLater();
            httpStream=0;
        }
        if (httpStream) {
            switch (status->state()) {
            case MPDState_Playing:
                httpStream->play();
                break;
            case MPDState_Inactive:
            case MPDState_Stopped:
                httpStream->stop();
            break;
            case MPDState_Paused:
                httpStream->pause();
            default:
            break;
            }
        } else {
            #if QT_VERSION < 0x050000
            httpStream=new Phonon::MediaObject(this);
            Phonon::createPath(httpStream, new Phonon::AudioOutput(Phonon::MusicCategory, this));
            httpStream->setCurrentSource(Settings::self()->streamUrl());
            #else
            httpStream=new QMediaPlayer(this);
            httpStream->setMedia(QUrl(Settings::self()->streamUrl()));
            #endif
            httpStream->setProperty(constUrlProperty, Settings::self()->streamUrl());
        }
    }
    #else
    Q_UNUSED(s)
    #endif
}

void MainWindow::enableStopActions(bool enable)
{
    stopAfterCurrentTrackAction->setEnabled(enable);
    stopPlaybackAction->setEnabled(enable);
}

void MainWindow::stopPlayback()
{
    if (!fadeWhenStop() || MPDState_Paused==MPDStatus::self()->state() || 0==volume) {
        emit stop();
    }
    enableStopActions(false);
    nextTrackAction->setEnabled(false);
    prevTrackAction->setEnabled(false);
    startVolumeFade();
}

void MainWindow::stopAfterCurrentTrack()
{
    playQueueModel.clearStopAfterTrack();
    emit stop(true);
}

void MainWindow::stopAfterTrack()
{
    QModelIndexList selected=playQueue->selectedIndexes();

    if (1==selected.count()) {
        QModelIndex idx=playQueueProxyModel.mapToSource(selected.first());
        playQueueModel.setStopAfterTrack(playQueueModel.getIdByRow(idx.row()));
    }
}

void MainWindow::startVolumeFade()
{
    if (!fadeWhenStop()) {
        return;
    }

    stopState=StopState_Stopping;
    if (!volumeFade) {
        volumeFade = new QPropertyAnimation(this, "volume");
        volumeFade->setDuration(Settings::self()->stopFadeDuration());
    }
    origVolume=volume;
    lastVolume=volume;
    volumeFade->setStartValue(volume);
    volumeFade->setEndValue(-1);
    volumeFade->start();
}

void MainWindow::stopVolumeFade()
{
    if (stopState) {
        stopState=StopState_None;
        volumeFade->stop();
        setMpdVolume(-1);
    }
}

void MainWindow::setMpdVolume(int v)
{
    if (-1==v) {
        volume=origVolume;
        emit setVolume(origVolume);
        if (StopState_Stopping==stopState) {
            emit stop();
        }
        stopState=StopState_None;
    } else if (lastVolume!=v) {
        emit setVolume(v);
        lastVolume=v;
    }
}

void MainWindow::playPauseTrack()
{
    MPDStatus * const status = MPDStatus::self();

    if (MPDState_Playing==status->state()) {
        emit pause(true);
    } else if (MPDState_Paused==status->state()) {
        stopVolumeFade();
        emit pause(false);
    } else if (playQueueModel.rowCount()>0) {
        stopVolumeFade();
        if (-1!=playQueueModel.currentSong() && -1!=playQueueModel.currentSongRow()) {
            emit startPlayingSongId(playQueueModel.currentSong());
        } else {
            emit play();
        }
    }
}

void MainWindow::nextTrack()
{
    nextTrackAction->trigger();
}

void MainWindow::prevTrack()
{
    prevTrackAction->trigger();
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

void MainWindow::searchPlayQueue()
{
    if (playQueueSearchWidget->text().isEmpty()) {
        if (playQueueSearchTimer) {
            playQueueSearchTimer->stop();
        }
        realSearchPlayQueue();
    } else {
        if (!playQueueSearchTimer) {
            playQueueSearchTimer=new QTimer(this);
            playQueueSearchTimer->setSingleShot(true);
            connect(playQueueSearchTimer, SIGNAL(timeout()), SLOT(realSearchPlayQueue()));
        }
        playQueueSearchTimer->start(250);
    }
}

void MainWindow::realSearchPlayQueue()
{
    if (playQueueSearchTimer) {
        playQueueSearchTimer->stop();
    }
    QString filter=playQueueSearchWidget->text().trimmed();
    if (filter.length()<2) {
        filter=QString();
    }

    if (filter!=playQueueProxyModel.filterRegExp().pattern()) {
        playQueue->setFilterActive(!filter.isEmpty());
        playQueue->selectionModel()->clear();
        playQueueProxyModel.update(filter);
        QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
        playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && MPDState_Playing==MPDStatus::self()->state());
        scrollPlayQueue();
    }
}

void MainWindow::updatePlayQueue(const QList<Song> &songs)
{
    playPauseTrackAction->setEnabled(!songs.isEmpty());
    nextTrackAction->setEnabled(stopPlaybackAction->isEnabled() && songs.count()>1);
    prevTrackAction->setEnabled(stopPlaybackAction->isEnabled() && songs.count()>1);
    StdActions::self()->savePlayQueueAction->setEnabled(!songs.isEmpty());
    clearPlayQueueAction->setEnabled(!songs.isEmpty());

    bool wasEmpty=0==playQueueModel.rowCount();
    playQueueModel.update(songs);
    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
    playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && wasEmpty && MPDState_Playing==MPDStatus::self()->state());

    /*if (1==songs.count() && MPDState_Playing==MPDStatus::self()->state()) {
        updateCurrentSong(songs.at(0));
    } else*/ if (0==songs.count()) {
        updateCurrentSong(Song());
    } else if (current.isStream()) {
        // Check to see if it has been updated...
        Song pqSong=playQueueModel.getSongByRow(playQueueModel.currentSongRow());
        if (pqSong.artist!=current.artist || pqSong.album!=current.album || pqSong.name!=current.name || pqSong.title!=current.title || pqSong.year!=current.year) {
            updateCurrentSong(pqSong);
        }
    }
    playQueueItemsSelected(playQueue->haveSelectedItems());
    updateNextTrack(MPDStatus::self()->nextSongId());
}

bool MainWindow::currentIsStream() const
{
    return playQueueModel.rowCount() && -1!=current.id && current.isStream();
}

void MainWindow::updateWindowTitle()
{
    MPDStatus * const status = MPDStatus::self();
    bool stopped=MPDState_Stopped==status->state() || MPDState_Inactive==status->state();
    bool multipleConnections=connectionsAction->isVisible();
    QString connection=MPDConnection::self()->getDetails().getName();

    if (stopped) {
        setWindowTitle(multipleConnections ? i18n("Cantata (%1)").arg(connection) : "Cantata");
    } else if (current.isStream() && !current.isCantataStream() && !current.isCdda()) {
        setWindowTitle(multipleConnections
                        ? i18nc("track :: Cantata (connection)", "%1 :: Cantata (%2)").arg(trackLabel->text()).arg(connection)
                        : i18nc("track :: Cantata", "%1 :: Cantata").arg(trackLabel->text()));
    } else if (current.artist.isEmpty()) {
        if (trackLabel->text().isEmpty()) {
            setWindowTitle(multipleConnections ? i18n("Cantata (%1)").arg(connection) : "Cantata");
        } else {
            setWindowTitle(multipleConnections
                            ? i18nc("track :: Cantata (connection)", "%1 :: Cantata (%2)").arg(trackLabel->text()).arg(connection)
                            : i18nc("track :: Cantata", "%1 :: Cantata").arg(trackLabel->text()));
        }
    } else {
        setWindowTitle(multipleConnections
                        ? i18nc("track - artist :: Cantata (connection)", "%1 - %2 :: Cantata (%3)")
                                .arg(trackLabel->text()).arg(current.artist).arg(connection)
                        : i18nc("track - artist :: Cantata", "%1 - %2 :: Cantata").arg(trackLabel->text()).arg(current.artist));
    }
}

void MainWindow::updateCurrentSong(const Song &song)
{
    if (fadeWhenStop() && StopState_None!=stopState) {
        if (StopState_Stopping==stopState) {
            emit stop();
        }
    }

    current=song;
    if (current.isCdda()) {
        emit getStatus();
        if (current.isUnknown()) {
            Song pqSong=playQueueModel.getSongById(current.id);
            if (!pqSong.isEmpty()) {
                current=pqSong;
            }
        }
    }

    #ifdef TAGLIB_FOUND
    if (current.isCantataStream()) {
        Song mod=HttpServer::self()->decodeUrl(current.file);
        if (!mod.title.isEmpty()) {
            current=mod;
            current.id=song.id;
        }
    }
    #endif

    #if !defined Q_OS_WIN
    mpris->updateCurrentSong(current);
    #endif
    if (current.time<5 && MPDStatus::self()->songId()==current.id && MPDStatus::self()->timeTotal()>5) {
        current.time=MPDStatus::self()->timeTotal();
    }
    positionSlider->setEnabled(-1!=current.id && (!currentIsStream() || (!current.isCdda() && current.time>5)));
    coverWidget->update(current);

    if (current.isStream() && !current.isCantataStream() && !current.isCdda()) {
        trackLabel->setText(current.name.isEmpty() ? i18n("Unknown") : current.name);
        if (current.artist.isEmpty() && current.title.isEmpty() && !current.name.isEmpty()) {
            artistLabel->setText(i18n("(Stream)"));
        } else {
            artistLabel->setText(current.artist.isEmpty() ? current.title : i18nc("title - artist", "%1 - %2").arg(current.artist).arg(current.title));
        }
    } else {
        if (current.title.isEmpty() && current.artist.isEmpty() && (!current.name.isEmpty() || !current.file.isEmpty())) {
            trackLabel->setText(current.name.isEmpty() ? current.file : current.name);
        } else {
            trackLabel->setText(current.title);
        }
        if (current.album.isEmpty() && current.artist.isEmpty()) {
            artistLabel->setText(trackLabel->text().isEmpty() ? QString() : i18n("Unknown"));
        } else if (current.album.isEmpty()) {
            artistLabel->setText(current.artist);
        } else {
            QString album=current.album;
            quint16 year=Song::albumYear(current);
            if (year>0) {
                album+=QString(" (%1)").arg(year);
            }
            artistLabel->setText(i18nc("artist - album", "%1 - %2").arg(current.artist).arg(album));
        }
    }

    bool isPlaying=MPDState_Playing==MPDStatus::self()->state();
    playQueueModel.updateCurrentSong(current.id);
    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
    playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && isPlaying);
    scrollPlayQueue();
    updateWindowTitle();
    context->update(current);
    trayItem->songChanged(song, isPlaying);
}

void MainWindow::scrollPlayQueue()
{
    if (autoScrollPlayQueue && MPDState_Playing==MPDStatus::self()->state() && !playQueue->isGrouped()) {
        qint32 row=playQueueModel.currentSongRow();
        if (row>=0) {
            playQueue->scrollTo(playQueueProxyModel.mapFromSource(playQueueModel.index(row, 0)), QAbstractItemView::PositionAtCenter);
        }
    }
}

void MainWindow::updateStats()
{
    // Check if remote db is more recent than local one
    if (!MusicLibraryModel::self()->lastUpdate().isValid() || MPDStats::self()->dbUpdate() > MusicLibraryModel::self()->lastUpdate()) {
        loaded|=TAB_LIBRARY|TAB_FOLDERS;
        if (!MusicLibraryModel::self()->lastUpdate().isValid()) {
            libraryPage->clear();
            //albumsPage->clear();
            folderPage->clear();
            playlistsPage->clear();
        }
        albumsPage->goTop();
        libraryPage->refresh();
        folderPage->refresh();
        playlistsPage->refresh();
    }
}

void MainWindow::updateStatus()
{
    updateStatus(MPDStatus::self());
}

void MainWindow::updateStatus(MPDStatus * const status)
{
    if (MPDState_Stopped==status->state() || MPDState_Inactive==status->state()) {
        positionSlider->clearTimes();
        playQueueModel.clearStopAfterTrack();
    } else {
        positionSlider->setRange(0, 0==status->timeTotal() && 0!=current.time && (current.isCdda() || current.isCantataStream())
                                    ? current.time : status->timeTotal());
        positionSlider->setValue(status->timeElapsed());
    }

    if (!stopState) {
        volume=status->volume();

        if (volume<=0) {
            volumeButton->setIcon(Icons::self()->toolbarVolumeMutedIcon);
        } else if (volume<=33) {
            volumeButton->setIcon(Icons::self()->toolbarVolumeLowIcon);
        } else if (volume<=67) {
            volumeButton->setIcon(Icons::self()->toolbarVolumeMediumIcon);
        } else {
            volumeButton->setIcon(Icons::self()->toolbarVolumeHighIcon);
        }

        volumeControl->blockSignals(true);
        if (volume<0) {
            volumeButton->setEnabled(false);
            volumeButton->setToolTip(i18n("Volume Disabled"));
            volumeControl->setToolTip(i18n("Volume Disabled"));
            volumeControl->setValue(0);
        } else {
            int unmuteVolume=-1;
            if (0==volume) {
                unmuteVolume=MPDConnection::self()->unmuteVolume();
                if (unmuteVolume>0) {
                    volume=unmuteVolume;
                }
            }
            volumeButton->setEnabled(true);
            volumeButton->setToolTip(unmuteVolume>0 ? i18n("Volume %1% (Muted)").arg(volume) : i18n("Volume %1%").arg(volume));
            volumeControl->setToolTip(volumeButton->toolTip());
            volumeControl->setValue(volume);
        }
        muteAction->setEnabled(volumeButton->isEnabled());
        increaseVolumeAction->setEnabled(volumeButton->isEnabled());
        decreaseVolumeAction->setEnabled(volumeButton->isEnabled());
        volumeControl->blockSignals(false);
    }

    randomPlayQueueAction->setChecked(status->random());
    repeatPlayQueueAction->setChecked(status->repeat());
    singlePlayQueueAction->setChecked(status->single());
    consumePlayQueueAction->setChecked(status->consume());
    updateNextTrack(status->nextSongId());

    if (status->timeElapsed()<172800 && (!currentIsStream() || (status->timeTotal()>0 && status->timeElapsed()<=status->timeTotal()))) {
        if (status->state() == MPDState_Stopped || status->state() == MPDState_Inactive) {
            positionSlider->setRange(0, 0);
        } else {
            positionSlider->setValue(status->timeElapsed());
        }
    }

    playQueueModel.setState(status->state());
    switch (status->state()) {
    case MPDState_Playing:
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        if (httpStreamEnabled && httpStream) {
            httpStream->play();
        }
        #endif
        playPauseTrackAction->setIcon(Icons::self()->toolbarPauseIcon);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        //playPauseTrackButton->setChecked(false);
        if (StopState_Stopping!=stopState) {
            enableStopActions(true);
            nextTrackAction->setEnabled(playQueueModel.rowCount()>1);
            prevTrackAction->setEnabled(playQueueModel.rowCount()>1);
        }
        positionSlider->startTimer();

        #ifdef ENABLE_KDE_SUPPORT
        trayItem->setIconByName(Icons::self()->toolbarPlayIcon.name());
        #else
        trayItem->setIcon(Icons::self()->toolbarPlayIcon);
        #endif
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        if (httpStreamEnabled && httpStream) {
            httpStream->stop();
        }
        #endif
        playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        enableStopActions(false);
        nextTrackAction->setEnabled(false);
        prevTrackAction->setEnabled(false);
        if (!playPauseTrackAction->isEnabled()) {
            trackLabel->setText(QString());
            artistLabel->setText(QString());
            current=Song();
            coverWidget->update(current);
        }
        current.id=0;
        updateWindowTitle();

        #ifdef ENABLE_KDE_SUPPORT
        trayItem->setIconByName("cantata");
        #else
        trayItem->setIcon(Icons::self()->appIcon);
        #endif
        trayItem->setToolTip("cantata", i18n("Cantata"), "<i>Playback stopped</i>");
        positionSlider->stopTimer();
        break;
    case MPDState_Paused:
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        if (httpStreamEnabled && httpStream) {
            httpStream->pause();
        }
        #endif
        playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        enableStopActions(0!=playQueueModel.rowCount());
        nextTrackAction->setEnabled(playQueueModel.rowCount()>1);
        prevTrackAction->setEnabled(playQueueModel.rowCount()>1);
        #ifdef ENABLE_KDE_SUPPORT
        trayItem->setIconByName(Icons::self()->toolbarPauseIcon.name());
        #else
        trayItem->setIcon(Icons::self()->toolbarPauseIcon);
        #endif
        positionSlider->stopTimer();
        break;
    default:
        qDebug("Invalid state");
        break;
    }

    // Check if song has changed or we're playing again after being stopped
    // and update song info if needed
    if (MPDState_Inactive!=status->state() &&
        (MPDState_Inactive==lastState || (MPDState_Stopped==lastState && MPDState_Playing==status->state()) || lastSongId != status->songId())) {
        emit currentSong();
    }
    // Update status info
    lastState = status->state();
    lastSongId = status->songId();
}

void MainWindow::playQueueItemActivated(const QModelIndex &index)
{
    emit startPlayingSongId(playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(index).row()));
}

void MainWindow::removeFromPlayQueue()
{
    const QModelIndexList items = playQueue->selectedIndexes();
    QList<qint32> toBeRemoved;

    if (items.isEmpty()) {
        return;
    }

    foreach (const QModelIndex &idx, items) {
        toBeRemoved.append(playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(idx).row()));
    }

    emit removeSongs(toBeRemoved);
}

void MainWindow::replacePlayQueue()
{
    addToPlayQueue(true);
}

void MainWindow::addToPlayQueue()
{
    addToPlayQueue(false);
}

void MainWindow::addToPlayQueue(bool replace, quint8 priority)
{
    playQueueSearchWidget->clear();
    if (libraryPage->isVisible()) {
        libraryPage->addSelectionToPlaylist(QString(), replace, priority);
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToPlaylist(QString(), replace, priority);
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToPlaylist(QString(), replace, priority);
    } else if (playlistsPage->isVisible()) {
        playlistsPage->addSelectionToPlaylist(replace, priority);
    } else if (streamsPage->isVisible()) {
        streamsPage->addSelectionToPlaylist(replace, priority);
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        devicesPage->addSelectionToPlaylist(QString(), replace, priority);
    }
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    else if (onlinePage->isVisible()) {
        onlinePage->addSelectionToPlaylist(QString(), replace, priority);
    }
    #endif
}

void MainWindow::addWithPriority()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (!act || !MPDConnection::self()->canUsePriority() || !StdActions::self()->addWithPriorityAction->isVisible()) {
        return;
    }

    int prio=act->data().toInt();
    bool isPlayQueue=playQueue->hasFocus();
    QModelIndexList pqItems;

    if (isPlayQueue) {
        pqItems=playQueue->selectedIndexes();
        if (pqItems.isEmpty()) {
            return;
        }
    }

    if (-1==prio) {
        bool ok=false;
        prio=InputDialog::getInteger(i18n("Priority"), i18n("Enter priority (0..255):"), 150, 0, 255, 5, 10, &ok, this);
        if (!ok) {
            return;
        }
    }

    if (prio>=0 && prio<=255) {
        if (isPlayQueue) {
            QList<qint32> ids;
            foreach (const QModelIndex &idx, pqItems) {
                ids.append(playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(idx).row()));
            }
            emit setPriority(ids, prio);
        } else {
            addToPlayQueue(false, prio);
        }
    }
}

void MainWindow::addToNewStoredPlaylist()
{
    bool pq=playQueue->hasFocus();
    for(;;) {
        QString name = InputDialog::getText(i18n("Playlist Name"), i18n("Enter a name for the playlist:"), QString(), 0, this);

        if (PlaylistsModel::self()->exists(name)) {
            switch(MessageBox::warningYesNoCancel(this, i18n("A playlist named <b>%1</b> already exists!<br/>Add to that playlist?").arg(name),
                                                  i18n("Existing Playlist"))) {
            case MessageBox::Cancel:
                return;
            case MessageBox::Yes:
                break;
            case MessageBox::No:
            default:
                continue;
            }
        }

        if (!name.isEmpty()) {
            addToExistingStoredPlaylist(name, pq);
        }
        break;
    }
}

void MainWindow::addToExistingStoredPlaylist(const QString &name)
{
    addToExistingStoredPlaylist(name, playQueue->hasFocus());
}

void MainWindow::addToExistingStoredPlaylist(const QString &name, bool pq)
{
    if (pq) {
        QModelIndexList items = playQueue->selectedIndexes();
        QStringList files;
        if (items.isEmpty()) {
            files = playQueueModel.filenames();
        } else {
            qSort(items);
            foreach (const QModelIndex &idx, items) {
                Song s = playQueueModel.getSongByRow(playQueueProxyModel.mapToSource(idx).row());
                if (!s.file.isEmpty()) {
                    files.append(s.file);
                }
            }
        }
        if (!files.isEmpty()) {
            emit addSongsToPlaylist(name, files);
        }
    } else if (libraryPage->isVisible()) {
        libraryPage->addSelectionToPlaylist(name);
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToPlaylist(name);
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToPlaylist(name);
    }
}

void MainWindow::addStreamToPlayQueue()
{
    StreamDialog dlg(this, true);

    if (QDialog::Accepted==dlg.exec()) {
        QString url=dlg.url();

        if (dlg.save()) {
            StreamsModel::self()->addToFavourites(url, dlg.name());
        }
        playQueueModel.addItems(QStringList() << StreamsModel::modifyUrl(url), false, 0);
    }
}

void MainWindow::removeItems()
{
    if (playlistsPage->isVisible()) {
        playlistsPage->removeItems();
    } else if (streamsPage->isVisible()) {
        streamsPage->removeItems();
    }
}

void MainWindow::checkMpdAccessibility()
{
    #ifdef Q_OS_LINUX
    if (!mpdAccessibilityTimer) {
        mpdAccessibilityTimer=new QTimer(this);
        connect(mpdAccessibilityTimer, SIGNAL(timeout()), SLOT(checkMpdDir()));
    }
    mpdAccessibilityTimer->start(500);
    #endif
}

void MainWindow::updatePlayQueueStats(int songs, quint32 time)
{
    if (0==time) {
        playQueueStatsLabel->setText(QString());
        return;
    }

    #ifdef ENABLE_KDE_SUPPORT
    playQueueStatsLabel->setText(i18np("1 Track (%2)", "%1 Tracks (%2)", songs, MPDParseUtils::formatDuration(time)));
    #else
    playQueueStatsLabel->setText(QTP_TRACKS_DURATION_STR(songs, MPDParseUtils::formatDuration(time)));
    #endif
}

void MainWindow::copyTrackInfo()
{
    const QModelIndexList items = playQueue->selectedIndexes();

    if (items.isEmpty()) {
        return;
    }

    QString txt;
    QTextStream str(&txt);

    foreach (const QModelIndex &idx, items) {
        Song s = playQueueModel.getSongByRow(playQueueProxyModel.mapToSource(idx).row());
        if (!s.isEmpty()) {
            if (!txt.isEmpty()) {
                str << QChar('\n');
            }
            str << s.format();
        }
    }
    QApplication::clipboard()->setText(txt);
}

int MainWindow::calcMinHeight()
{
    if (FancyTabWidget::Mode_LargeSidebar==tabWidget->mode()) {
        return coverWidget->height()+(tabWidget->visibleCount()*(32+fontMetrics().height()+4));
    } else if (FancyTabWidget::Mode_IconOnlyLargeSidebar==tabWidget->mode()) {
        return coverWidget->height()+(tabWidget->visibleCount()*(32+6));
    }
    return 256;
}

int MainWindow::calcCompactHeight()
{
    int spacing=Utils::layoutSpacing(this);
    // For some reason height is always larger than it needs to be - so fix this to cover height +4
    return qMax(qMax(playPauseTrackButton->height(),
                         trackLabel->height()+artistLabel->height()+spacing)+
                         positionSlider->height(),
                    coverWidget->height())+
           (messageWidget->isActive() ? (messageWidget->sizeHint().height()+spacing) : 0);
}

void MainWindow::expandOrCollapse(bool saveCurrentSize)
{
    if (isFullScreen()) {
        return;
    }
    static bool lastMax=false;

    bool showing=expandInterfaceAction->isChecked();
    QPoint p(isVisible() ? pos() : QPoint());
    int compactHeight=0;

    if (!showing) {
        setMinimumHeight(0);
        lastMax=isMaximized();
        if (saveCurrentSize) {
            expandedSize=size();
        }
        compactHeight=calcCompactHeight();
    } else {
        if (saveCurrentSize) {
            collapsedSize=size();
        }
        setMinimumHeight(calcMinHeight());
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    int prevWidth=size().width();
    stack->setVisible(showing);
    if (!showing) {
        setWindowState(windowState()&~Qt::WindowMaximized);
    }
    QApplication::processEvents();
    adjustSize();

    if (showing) {
        bool adjustWidth=size().width()!=expandedSize.width();
        bool adjustHeight=size().height()!=expandedSize.height();
        if (adjustWidth || adjustHeight) {
            resize(adjustWidth ? expandedSize.width() : size().width(), adjustHeight ? expandedSize.height() : size().height());
        }
        if (lastMax) {
            showMaximized();
        }
    } else {
        // Widths also sometimes expands, so make sure this is no larger than it was before...
        collapsedSize=QSize(collapsedSize.isValid() ? collapsedSize.width() : (size().width()>prevWidth ? prevWidth : size().width()), compactHeight);
        resize(collapsedSize);
        setFixedHeight(size().height());
    }
    if (!p.isNull()) {
        move(p);
    }

    fullScreenAction->setEnabled(showing);
    songInfoButton->setVisible(songInfoAction->isCheckable() && showing);
    songInfoAction->setEnabled(showing);
}

void MainWindow::showSongInfo()
{
    if (songInfoAction->isCheckable()) {
        stack->setCurrentWidget(songInfoAction->isChecked() ? (QWidget *)context : (QWidget *)splitter);
    } else {
        showTab(PAGE_CONTEXT);
    }
}

void MainWindow::fullScreen()
{
    if (expandInterfaceAction->isChecked()) {
        if (isFullScreen()) {
            showNormal();
            expandInterfaceAction->setEnabled(true);
            connect(coverWidget, SIGNAL(clicked()), expandInterfaceAction, SLOT(trigger()));
        } else {
            showFullScreen();
            expandInterfaceAction->setEnabled(false);
            disconnect(coverWidget, SIGNAL(clicked()), expandInterfaceAction, SLOT(trigger()));
        }
    } else {
        fullScreenAction->setChecked(false);
    }
}

void MainWindow::sidebarModeChanged()
{
    if (expandInterfaceAction->isChecked()) {
        setMinimumHeight(calcMinHeight());
    }
}

// Do this by taking the set off all song id's and subtracting from that the set of selected song id's. Feed that list to emit removeSongs
void MainWindow::cropPlayQueue()
{
    const QModelIndexList items = playQueue->selectedIndexes();
    if (items.isEmpty()) {
        return;
    }

    QSet<qint32> songs = playQueueModel.getSongIdSet();
    QSet<qint32> selected;

    foreach (const QModelIndex &idx, items) {
        selected << playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(idx).row());
    }

    emit removeSongs((songs - selected).toList());
}

void MainWindow::currentTabChanged(int index)
{
    controlDynamicButton();
    switch(index) {
    #ifdef ENABLE_DEVICES_SUPPORT
    case PAGE_DEVICES: // Need library to be loaded to check if song exists...
        devicesPage->controlActions();
        break;
    #endif
    case PAGE_LIBRARY:
    case PAGE_ALBUMS: // Albums shares refresh with library...
        if (!(loaded&TAB_LIBRARY)) {
            loaded|=TAB_LIBRARY;
            albumsPage->goTop();
            libraryPage->refresh();
        }
        if (PAGE_LIBRARY==index) {
            libraryPage->controlActions();
        } else {
//             AlbumsModel::self()->getCovers();
            albumsPage->controlActions();
        }
        break;
    case PAGE_FOLDERS:
        if (!(loaded&TAB_FOLDERS)) {
            loaded|=TAB_FOLDERS;
            folderPage->refresh();
        }
        folderPage->controlActions();
        break;
    case PAGE_PLAYLISTS:
        playlistsPage->controlActions();
        break;
    case PAGE_DYNAMIC:
        dynamicPage->controlActions();
        break;
    case PAGE_STREAMS:
        if (!(loaded&TAB_STREAMS)) {
            loaded|=TAB_STREAMS;
            streamsPage->refresh();
        }
        streamsPage->controlActions();
        break;
    #ifdef ENABLE_ONLINE_SERVICES
    case PAGE_ONLINE:
        onlinePage->controlActions();
        break;
    #endif
    default:
        break;
    }
}

void MainWindow::tabToggled(int index)
{
    switch (index) {
    case PAGE_PLAYQUEUE:
        if (tabWidget->isEnabled(index)) {
            autoHideSplitterAction->setVisible(false);
            splitter->setAutohidable(0, autoHideSplitterAction->isChecked() && !tabWidget->isEnabled(PAGE_PLAYQUEUE));
            playQueueWidget->setParent(playQueuePage);
            playQueuePage->layout()->addWidget(playQueueWidget);
            playQueueWidget->setVisible(true);
        } else {
            playQueuePage->layout()->removeWidget(playQueueWidget);
            playQueueWidget->setParent(splitter);
            playQueueWidget->setVisible(true);
            autoHideSplitterAction->setVisible(true);
            splitter->setAutohidable(0, autoHideSplitterAction->isChecked() && !tabWidget->isEnabled(PAGE_PLAYQUEUE));
        }
        playQueue->updatePalette();
        break;
    case PAGE_CONTEXT:
        if (tabWidget->isEnabled(index) && songInfoAction->isCheckable()) {
            context->setParent(contextPage);
            contextPage->layout()->addWidget(context);
            context->setVisible(true);
            songInfoButton->setVisible(false);
            songInfoAction->setCheckable(false);
        } else if (!songInfoAction->isCheckable()) {
            contextPage->layout()->removeWidget(context);
            stack->addWidget(context);
            songInfoButton->setVisible(true);
            songInfoAction->setCheckable(true);
        }
        break;
    case PAGE_LIBRARY:
        locateTrackAction->setVisible(tabWidget->isEnabled(index));
        break;
    case PAGE_ALBUMS:
        AlbumsModel::self()->setEnabled(!AlbumsModel::self()->isEnabled());
        break;
    case PAGE_FOLDERS:
        folderPage->setEnabled(!folderPage->isEnabled());
        if (folderPage->isEnabled() && loaded&TAB_FOLDERS) loaded-=TAB_FOLDERS;
        break;
    case PAGE_STREAMS:
        streamsPage->setEnabled(!streamsPage->isEnabled());
        if (streamsPage->isEnabled() && loaded&TAB_STREAMS) loaded-=TAB_STREAMS;
        break;
    #ifdef ENABLE_ONLINE_SERVICES
    case PAGE_ONLINE:
        OnlineServicesModel::self()->setEnabled(!OnlineServicesModel::self()->isEnabled());
        break;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    case PAGE_DEVICES:
        DevicesModel::self()->setEnabled(!DevicesModel::self()->isEnabled());
        StdActions::self()->copyToDeviceAction->setVisible(DevicesModel::self()->isEnabled());
        break;
    #endif
    default:
        break;
    }
    sidebarModeChanged();
}

void MainWindow::toggleSplitterAutoHide()
{
    bool ah=autoHideSplitterAction->isChecked() && !tabWidget->isEnabled(PAGE_PLAYQUEUE);
    splitter->setAutoHideEnabled(ah);
    splitter->setAutohidable(0, ah);
}

void MainWindow::toggleMonoIcons()
{
    bool mono=monoIconsAction->isChecked();
    if (mono!=Settings::self()->monoSidebarIcons()) {
        Settings::self()->saveMonoSidebarIcons(mono);
        Icons::self()->initSidebarIcons();
        showPlayQueueAction->setIcon(Icons::self()->playqueueIcon);
        tabWidget->SetIcon(PAGE_PLAYQUEUE, showPlayQueueAction->icon());
        libraryTabAction->setIcon(Icons::self()->artistsIcon);
        tabWidget->SetIcon(PAGE_LIBRARY, libraryTabAction->icon());
        albumsTabAction->setIcon(Icons::self()->albumsIcon);
        tabWidget->SetIcon(PAGE_ALBUMS, albumsTabAction->icon());
        foldersTabAction->setIcon(Icons::self()->foldersIcon);
        tabWidget->SetIcon(PAGE_FOLDERS, foldersTabAction->icon());
        playlistsTabAction->setIcon(Icons::self()->playlistsIcon);
        tabWidget->SetIcon(PAGE_PLAYLISTS, playlistsTabAction->icon());
        dynamicTabAction->setIcon(Icons::self()->dynamicIcon);
        tabWidget->SetIcon(PAGE_DYNAMIC, dynamicTabAction->icon());
        streamsTabAction->setIcon(Icons::self()->streamsIcon);
        tabWidget->SetIcon(PAGE_STREAMS, streamsTabAction->icon());
        #ifdef ENABLE_ONLINE_SERVICES
        onlineTabAction->setIcon(Icons::self()->onlineIcon);
        tabWidget->SetIcon(PAGE_ONLINE, onlineTabAction->icon());
        #endif
        tabWidget->SetIcon(PAGE_CONTEXT, Icons::self()->infoSidebarIcon);
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesTabAction->setIcon(Icons::self()->devicesIcon);
        tabWidget->SetIcon(PAGE_DEVICES, devicesTabAction->icon());
        #endif
        tabWidget->Recreate();
    }
}

void MainWindow::locateTrack()
{
    if (!libraryPage->isVisible()) {
        showLibraryTab();
    }
    libraryPage->showSongs(playQueue->selectedSongs());
}

void MainWindow::locateArtist(const QString &artist)
{
    if (songInfoAction->isCheckable()) {
        songInfoAction->setChecked(false);
        showSongInfo();
    }
    if (!libraryPage->isVisible()) {
        showLibraryTab();
    }
    libraryPage->showArtist(artist);
}

void MainWindow::locateAlbum(const QString &artist, const QString &album)
{
    if (songInfoAction->isCheckable()) {
        songInfoAction->setChecked(false);
        showSongInfo();
    }
    if (!libraryPage->isVisible()) {
        showLibraryTab();
    }
    libraryPage->showAlbum(artist, album);
}

void MainWindow::playSong(const QString &song)
{
    qint32 id=playQueueModel.getSongId(song);
    if (-1==id) {
        addAndPlay(song);
    } else {
        emit startPlayingSongId(id);
    }
}

void MainWindow::showPage(const QString &page, bool focusSearch)
{
    QString p=page.toLower();
    if (QLatin1String("library")==p || QLatin1String("artists")==p) {
        showTab(MainWindow::PAGE_LIBRARY);
        if (focusSearch) {
            libraryPage->focusSearch();
        }
    } else if (QLatin1String("albums")==p) {
        showTab(MainWindow::PAGE_ALBUMS);
        if (focusSearch) {
            albumsPage->focusSearch();
        }
    } else if (QLatin1String("folders")==p) {
        showTab(MainWindow::PAGE_FOLDERS);
        if (focusSearch) {
            folderPage->focusSearch();
        }
    } else if (QLatin1String("playlists")==p) {
        showTab(MainWindow::PAGE_PLAYLISTS);
        if (focusSearch) {
            playlistsPage->focusSearch();
        }
    } else if (QLatin1String("dynamic")==p) {
        showTab(MainWindow::PAGE_DYNAMIC);
        if (focusSearch) {
            dynamicPage->focusSearch();
        }
    } else if (QLatin1String("streams")==p) {
        showTab(MainWindow::PAGE_STREAMS);
        if (focusSearch) {
            streamsPage->focusSearch();
        }
    } else if (QLatin1String("info")==p) {
        if (songInfoAction->isCheckable()) {
            songInfoAction->setChecked(true);
        }
        showSongInfo();
    }
    #ifdef ENABLE_ONLINE_SERVICES
    else if (QLatin1String("online")==p) {
        showTab(MainWindow::PAGE_ONLINE);
        if (focusSearch) {
            onlinePage->focusSearch();
        }
    }
    #endif
    #if defined ENABLE_KDE_SUPPORT && defined TAGLIB_FOUND
    else if (QLatin1String("devices")==p) {
        showTab(MainWindow::PAGE_DEVICES);
        if (focusSearch) {
            devicesPage->focusSearch();
        }
    }
    #endif
    else if (tabWidget->isEnabled(PAGE_PLAYQUEUE) && QLatin1String("playqueue")==p) {
        showTab(MainWindow::PAGE_PLAYQUEUE);
        if (focusSearch) {
            playQueueSearchWidget->setFocus();
        }
    }

    if (!expandInterfaceAction->isChecked()) {
        expandInterfaceAction->setChecked(true);
        expandOrCollapse();
    }
}

void MainWindow::dynamicStatus(const QString &message)
{
    Dynamic::self()->helperMessage(message);
}

void MainWindow::showTab(int page)
{
    tabWidget->SetCurrentIndex(page);
}

void MainWindow::goBack()
{
    switch (tabWidget->current_index()) {
    #if defined ENABLE_DEVICES_SUPPORT && defined TAGLIB_FOUND
    case PAGE_DEVICES:   devicesPage->goBack();    break;
    #endif
    case PAGE_LIBRARY:   libraryPage->goBack();    break;
    case PAGE_ALBUMS:    albumsPage->goBack();     break;
    case PAGE_FOLDERS:   folderPage->goBack();     break;
    case PAGE_PLAYLISTS: playlistsPage->goBack();  break;
    case PAGE_STREAMS:   streamsPage->goBack();    break;
    #ifdef ENABLE_ONLINE_SERVICES
    case PAGE_ONLINE:    onlinePage->goBack();     break;
    #endif
    default:                                       break;
    }
}

void MainWindow::showSearch()
{
    if (context->isVisible()) {
        context->search();
    } else if (libraryPage->isVisible()) {
        libraryPage->focusSearch();
    } else if (albumsPage->isVisible()) {
        albumsPage->focusSearch();
    } else if (folderPage->isVisible()) {
        folderPage->focusSearch();
    } else if (playlistsPage->isVisible()) {
        playlistsPage->focusSearch();
    } else if (dynamicPage->isVisible()) {
        dynamicPage->focusSearch();
    } else if (streamsPage->isVisible()) {
        streamsPage->focusSearch();
    }
    #ifdef ENABLE_ONLINE_SERVICES
    else if (onlinePage->isVisible()) {
        onlinePage->focusSearch();
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        devicesPage->focusSearch();
    }
    #endif
    else if (playQueuePage->isVisible()) {
        playQueueSearchWidget->toggle();
    }
}

bool MainWindow::fadeWhenStop() const
{
    return fadeStop && volumeButton->isEnabled();
}

void MainWindow::expandAll()
{
    QWidget *f=QApplication::focusWidget();
    if (f && qobject_cast<QTreeView *>(f) && !qobject_cast<GroupedView *>(f)) {
        static_cast<QTreeView *>(f)->expandAll();
    }
}

void MainWindow::collapseAll()
{
    QWidget *f=QApplication::focusWidget();
    if (f && qobject_cast<QTreeView *>(f) && !qobject_cast<GroupedView *>(f)) {
        static_cast<QTreeView *>(f)->collapseAll();
    }
}

void MainWindow::editTags()
{
    #ifdef TAGLIB_FOUND
    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (folderPage->isVisible()) {
        songs=folderPage->selectedSongs(FolderPage::ES_FillEmpty);
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }
    #endif
    editTags(songs, false);
    #endif
}

void MainWindow::editPlayQueueTags()
{
    #ifdef TAGLIB_FOUND
    editTags(playQueue->selectedSongs(), true);
    #endif
}

#ifdef TAGLIB_FOUND
void MainWindow::editTags(const QList<Song> &songs, bool isPlayQueue)
{
    if (songs.isEmpty() || TagEditor::instanceCount() || !canShowDialog()) {
        return;
    }
    QSet<QString> artists;
    QSet<QString> albumArtists;
    QSet<QString> albums;
    QSet<QString> genres;
    QString udi;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!isPlayQueue && devicesPage->isVisible()) {
        DevicesModel::self()->getDetails(artists, albumArtists, albums, genres);
        udi=devicesPage->activeFsDeviceUdi();
        if (udi.isEmpty()) {
            return;
        }
    } else
    #else
    Q_UNUSED(isPlayQueue)
    #endif
    MusicLibraryModel::self()->getDetails(artists, albumArtists, albums, genres);
    TagEditor *dlg=new TagEditor(this, songs, artists, albumArtists, albums, genres, udi);
    dlg->show();
}
#endif

void MainWindow::organiseFiles()
{
    #ifdef TAGLIB_FOUND
    if (TrackOrganiser::instanceCount() || !canShowDialog()) {
        return;
    }

    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (folderPage->isVisible()) {
        songs=folderPage->selectedSongs(FolderPage::ES_None);
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }
    #endif

    if (!songs.isEmpty()) {
        QString udi;
        #ifdef ENABLE_DEVICES_SUPPORT
        if (devicesPage->isVisible()) {
            udi=devicesPage->activeFsDeviceUdi();
            if (udi.isEmpty()) {
                return;
            }
        }
        #endif

        TrackOrganiser *dlg=new TrackOrganiser(this);
        dlg->show(songs, udi);
    }
    #endif
}

void MainWindow::addToDevice(const QString &udi)
{
    #ifdef ENABLE_DEVICES_SUPPORT
    if (libraryPage->isVisible()) {
        libraryPage->addSelectionToDevice(udi);
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToDevice(udi);
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToDevice(udi);
    } else if (playlistsPage->isVisible()) {
        playlistsPage->addSelectionToDevice(udi);
    }
    #else
    Q_UNUSED(udi)
    #endif
}

void MainWindow::deleteSongs()
{
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!StdActions::self()->deleteSongsAction->isVisible()) {
        return;
    }
    if (libraryPage->isVisible()) {
        libraryPage->deleteSongs();
    } else if (albumsPage->isVisible()) {
        albumsPage->deleteSongs();
    } else if (folderPage->isVisible()) {
        folderPage->deleteSongs();
    } else if (devicesPage->isVisible()) {
        devicesPage->deleteSongs();
    }
    #endif
}

void MainWindow::copyToDevice(const QString &from, const QString &to, const QList<Song> &songs)
{
    #ifdef ENABLE_DEVICES_SUPPORT
    if (songs.isEmpty() || ActionDialog::instanceCount() || !canShowDialog()) {
        return;
    }
    ActionDialog *dlg=new ActionDialog(this);
    dlg->copy(from, to, songs);
    #else
    Q_UNUSED(from)
    Q_UNUSED(to)
    Q_UNUSED(songs)
    #endif
}

void MainWindow::deleteSongs(const QString &from, const QList<Song> &songs)
{
    #ifdef ENABLE_DEVICES_SUPPORT
    if (songs.isEmpty() || ActionDialog::instanceCount() || !canShowDialog()) {
        return;
    }
    ActionDialog *dlg=new ActionDialog(this);
    dlg->remove(from, songs);
    #else
    Q_UNUSED(from)
    Q_UNUSED(songs)
    #endif
}

void MainWindow::replayGain()
{
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (RgDialog::instanceCount() || !canShowDialog()) {
        return;
    }

    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (folderPage->isVisible()) {
        songs=folderPage->selectedSongs(FolderPage::ES_GuessTags);
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }
    #endif

    if (!songs.isEmpty()) {
        QString udi;
        #ifdef ENABLE_DEVICES_SUPPORT
        if (devicesPage->isVisible()) {
            udi=devicesPage->activeFsDeviceUdi();
            if (udi.isEmpty()) {
                return;
            }
        }
        #endif

        RgDialog *dlg=new RgDialog(this);
        dlg->show(songs, udi);
    }
    #endif
}

void MainWindow::setCover()
{
    if (CoverDialog::instanceCount() || !canShowDialog()) {
        return;
    }

    Song song;
    if (libraryPage->isVisible()) {
        song=libraryPage->coverRequest();
    } else if (albumsPage->isVisible()) {
        song=albumsPage->coverRequest();
    }

    if (!song.isEmpty()) {
        CoverDialog *dlg=new CoverDialog(this);
        dlg->show(song);
    }
}

int MainWindow::currentTrackPosition() const
{
    return positionSlider->value();
}

QString MainWindow::coverFile() const
{
    return coverWidget->fileName();
}

void MainWindow::updateNextTrack(int nextTrackId)
{
    if (-1!=nextTrackId && MPDState_Stopped==MPDStatus::self()->state()) {
        nextTrackId=-1; // nextSongId is not accurate if we are stopped.
    }
    QString tt=nextTrackAction->property("tooltip").toString();
    if (-1==nextTrackId && tt.isEmpty()) {
        nextTrackAction->setProperty("tooltip", nextTrackAction->toolTip());
    } else if (-1==nextTrackId) {
        nextTrackAction->setToolTip(tt);
        nextTrackAction->setProperty("trackid", nextTrackId);
    } else if (nextTrackId!=nextTrackAction->property("trackid").toInt()) {
        Song s=playQueueModel.getSongByRow(playQueueModel.getRowById(nextTrackId));
        if (!s.artist.isEmpty() && !s.title.isEmpty()) {
            tt+=QLatin1String("<br/><i><small>")+s.artistSong()+QLatin1String("<small></i>");
        } else {
            nextTrackId=-1;
        }
        nextTrackAction->setToolTip(tt);
        nextTrackAction->setProperty("trackid", nextTrackId);
    }
}

#ifndef Q_OS_WIN
#if QT_VERSION < 0x050000
#include <QX11Info>
#include <X11/Xlib.h>
#endif
#endif

void MainWindow::hideWindow()
{
    lastPos=pos();
    hide();
}

void MainWindow::restoreWindow()
{
    bool wasHidden=isHidden();
    #ifdef Q_OS_WIN
    raiseWindow(this);
    #endif
    raise();
    showNormal();
    activateWindow();
    #ifndef Q_OS_WIN
    #if QT_VERSION < 0x050000
    // This section seems to be required for compiz...
    // ...without this, when 'qdbus com.googlecode.cantata /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Raise' is used
    // the Unity launcher item is highlighted, but the window is not shown!
    static const Atom constNetActive=XInternAtom(QX11Info::display(), "_NET_ACTIVE_WINDOW", False);
    QX11Info info;
    XEvent xev;
    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.message_type = constNetActive;
    xev.xclient.display = QX11Info::display();
    xev.xclient.window = effectiveWinId();
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 2;
    xev.xclient.data.l[1] = 0;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;
    XSendEvent(QX11Info::display(), QX11Info::appRootWindow(info.screen()), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    #endif
    #endif
    if (wasHidden && !lastPos.isNull()) {
        move(lastPos);
    }
}

#ifdef Q_OS_WIN
// This is down here, because windows.h includes ALL windows stuff - and we get conflics with MessageBox :-(
#include <windows.h>
static void raiseWindow(QWidget *w)
{
    ::SetWindowPos(w->effectiveWinId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::SetWindowPos(w->effectiveWinId(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}
#endif
