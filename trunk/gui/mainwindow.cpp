/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "mainwindow.h"
#include "support/localize.h"
#include "plurals.h"
#include "support/thread.h"
#include "trayitem.h"
#include "support/messagebox.h"
#include "support/inputdialog.h"
#include "models/playlistsmodel.h"
#include "covers.h"
#include "coverdialog.h"
#include "currentcover.h"
#include "preferencesdialog.h"
#include "mpd/mpdconnection.h"
#include "mpd/mpdstats.h"
#include "mpd/mpdstatus.h"
#include "mpd/mpdparseutils.h"
#include "settings.h"
#include "support/utils.h"
#include "support/touchproxystyle.h"
#include "models/musiclibrarymodel.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "librarypage.h"
#include "albumspage.h"
#include "folderpage.h"
#ifdef ENABLE_STREAMS
#include "streams/streamspage.h"
#include "streams/streamdialog.h"
#endif
#include "searchpage.h"
#include "support/gtkstyle.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/filejob.h"
#include "devices/devicespage.h"
#include "models/devicesmodel.h"
#include "devices/actiondialog.h"
#include "devices/syncdialog.h"
#if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
#include "devices/audiocddevice.h"
#endif
#endif
#ifdef ENABLE_ONLINE_SERVICES
#include "online/onlineservicespage.h"
#include "models/onlineservicesmodel.h"
#endif
#include "http/httpserver.h"
#ifdef TAGLIB_FOUND
#include "tags/trackorganiser.h"
#include "tags/tageditor.h"
#include "tags/tags.h"
#ifdef ENABLE_REPLAYGAIN_SUPPORT
#include "replaygain/rgdialog.h"
#endif
#endif
#include "models/streamsmodel.h"
#include "playlistspage.h"
#include "support/fancytabwidget.h"
#ifdef QT_QTDBUS_FOUND
#include "dbus/mpris.h"
#include "cantataadaptor.h"
#include "dbus/powermanagement.h"
#endif
#if !defined Q_OS_WIN && !defined Q_OS_MAC
#include "devices/mountpoints.h"
#endif
#ifdef ENABLE_DYNAMIC
#include "dynamic/dynamicpage.h"
#include "dynamic/dynamic.h"
#endif
#include "support/messagewidget.h"
#include "widgets/groupedview.h"
#include "widgets/actionitemdelegate.h"
#include "widgets/icons.h"
#include "widgets/volumeslider.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "stdactions.h"
#ifdef ENABLE_HTTP_STREAM_PLAYBACK
#include "mpd/httpstream.h"
#endif
#ifdef Q_OS_WIN
static void raiseWindow(QWidget *w);
#endif
#include <QSet>
#include <QString>
#include <QTimer>
#include <QToolBar>
#if defined Q_OS_MAC && QT_VERSION >= 0x050000
// QMacNativeToolBar requres Qt Mac Extras to be installed on Qt 5.0 and 5.1.
#include <QMacNativeToolBar>
#endif
#if QT_VERSION >= 0x050000
#include <QProcess>
#endif
#include <QDialogButtonBox>
#include <QKeyEvent>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KApplication>
#include <KDE/KStandardAction>
#include <KDE/KMenuBar>
#include <KDE/KMenu>
#include <KDE/KShortcutsDialog>
#include <KDE/KWindowSystem>
#include <KDE/KToggleAction>
#else
#include <QMenuBar>
#include "mediakeys.h"
#endif
#include <cstdlib>

static int nextKey(int &key)
{
    int k=key;
    if (Qt::Key_0==key) {
        key=Qt::Key_A;
    } else if (Qt::Key_Colon==++key) {
        key=Qt::Key_0;
    }
    return k;
}

MainWindow::MainWindow(QWidget *parent)
    : MAIN_WINDOW_BASE_CLASS(parent)
    , prevPage(-1)
    , lastState(MPDState_Inactive)
    , lastSongId(-1)
    , autoScrollPlayQueue(true)
    , showMenuAction(0)
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    , httpStream(new HttpStream(this))
    #endif
    , currentPage(0)
    #ifdef QT_QTDBUS_FOUND
    , mpris(0)
    #endif
    , statusTimer(0)
    , playQueueSearchTimer(0)
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    , mpdAccessibilityTimer(0)
    #endif
    , contextTimer(0)
    , contextSwitchTime(0)
    , connectedState(CS_Init)
    , stopAfterCurrent(false)
    , volumeFade(0)
    , origVolume(0)
    , lastVolume(0)
    , stopState(StopState_None)
{
    QPoint p=pos();
    ActionCollection::setMainWidget(this);
    trayItem=new TrayItem(this);
    #ifdef ENABLE_KDE_SUPPORT
    bool menuIcons=true;
    #else
    bool menuIcons=!QCoreApplication::testAttribute(Qt::AA_DontShowIconsInMenus);
    #endif
    #ifdef QT_QTDBUS_FOUND
    new CantataAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/cantata", this);
    #endif
    setMinimumHeight(256);
    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
    messageWidget->hide();

    // Need to set these values here, as used in library/device loading...
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    Song::setUseComposer(Settings::self()->useComposer());

    #ifdef Q_OS_WIN
    GtkStyle::applyTheme(this); // Despite its name, it *might* also apply touch style to spinboxes...
    #else
    #if defined Q_OS_MAC && QT_VERSION>=0x050000
    QMacNativeToolBar *topToolBar = new QMacNativeToolBar(this);
    topToolBar->showInWindowForWidget(this);
    #else // defined Q_OS_MAC && QT_VERSION>=0x050000
    setUnifiedTitleAndToolBarOnMac(true);
    QToolBar *topToolBar = addToolBar("ToolBar");
    #endif // defined Q_OS_MAC && QT_VERSION>=0x050000
    toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    topToolBar->setObjectName("MainToolBar");
    topToolBar->addWidget(toolbar);
    topToolBar->setMovable(false);
    topToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    topToolBar->ensurePolished();
    GtkStyle::applyTheme(topToolBar); // Despite its name, it *might* also apply touch style to spinboxes...
    #endif // Q_OS_WIN

    toolbar->ensurePolished();
    toolbar->adjustSize();
    coverWidget->setSize(toolbar->height());
    Icons::self()->initToolbarIcons(toolbar->palette().color(QPalette::Foreground));
    Icons::self()->initSidebarIcons();

    #ifdef ENABLE_KDE_SUPPORT
    prefAction=static_cast<Action *>(KStandardAction::preferences(this, SLOT(showPreferencesDialog()), ActionCollection::get()));
    quitAction=static_cast<Action *>(KStandardAction::quit(this, SLOT(quit()), ActionCollection::get()));
    shortcutsAction=static_cast<Action *>(KStandardAction::keyBindings(this, SLOT(configureShortcuts()), ActionCollection::get()));
    StdActions::self()->prevTrackAction->setGlobalShortcut(KShortcut(Qt::Key_MediaPrevious));
    StdActions::self()->nextTrackAction->setGlobalShortcut(KShortcut(Qt::Key_MediaNext));
    StdActions::self()->playPauseTrackAction->setGlobalShortcut(KShortcut(Qt::Key_MediaPlay));
    StdActions::self()->stopPlaybackAction->setGlobalShortcut(KShortcut(Qt::Key_MediaStop));
    StdActions::self()->stopAfterCurrentTrackAction->setGlobalShortcut(KShortcut());
    #else
    setWindowIcon(Icons::self()->appIcon);

    prefAction=ActionCollection::get()->createAction("configure", Utils::KDE==Utils::currentDe() ? i18n("Configure Cantata...") : i18n("Preferences"),
                                                     menuIcons ? Icons::self()->configureIcon : Icon());
    connect(prefAction, SIGNAL(triggered(bool)),this, SLOT(showPreferencesDialog()));
    quitAction = ActionCollection::get()->createAction("quit", i18n("Quit"), menuIcons ? "application-exit" : "");
    connect(quitAction, SIGNAL(triggered(bool)), this, SLOT(quit()));
    quitAction->setShortcut(QKeySequence::Quit);
    Action *aboutAction=ActionCollection::get()->createAction("about", i18nc("Qt-only", "About Cantata..."), menuIcons ? Icons::self()->appIcon : Icon());
    connect(aboutAction, SIGNAL(triggered(bool)),this, SLOT(showAboutDialog()));
    #ifdef Q_OS_MAC
    prefAction->setMenuRole(QAction::PreferencesRole);
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction->setMenuRole(QAction::AboutRole);
    #endif
    #endif // ENABLE_KDE_SUPPORT
    restoreAction = ActionCollection::get()->createAction("showwindow", i18n("Show Window"));
    connect(restoreAction, SIGNAL(triggered(bool)), this, SLOT(restoreWindow()));

    serverInfoAction=ActionCollection::get()->createAction("mpdinfo", i18n("Server information..."), menuIcons ? "network-server" : "");
    connect(serverInfoAction, SIGNAL(triggered(bool)),this, SLOT(showServerInfo()));
    serverInfoAction->setEnabled(Settings::self()->firstRun());
    refreshDbAction = ActionCollection::get()->createAction("refresh", i18n("Refresh Database"), menuIcons ? "view-refresh" : "");
    doDbRefreshAction = new Action(refreshDbAction->icon(), i18n("Refresh"), this);
    refreshDbAction->setEnabled(false);
    connectAction = ActionCollection::get()->createAction("connect", i18n("Connect"), Icons::self()->connectIcon);
    connectionsAction = ActionCollection::get()->createAction("connections", i18n("Collection"), menuIcons ? "network-server" : "");
    outputsAction = ActionCollection::get()->createAction("outputs", i18n("Outputs"), menuIcons ? Icons::self()->speakerIcon : Icon());
    stopAfterTrackAction = ActionCollection::get()->createAction("stopaftertrack", i18n("Stop After Track"), Icons::self()->toolbarStopIcon);
    addPlayQueueToStoredPlaylistAction = ActionCollection::get()->createAction("addpqtostoredplaylist", i18n("Add To Stored Playlist"), Icons::self()->playlistIcon);
    cropPlayQueueAction = ActionCollection::get()->createAction("cropplaylist", i18n("Crop"));
    addStreamToPlayQueueAction = ActionCollection::get()->createAction("addstreamtoplayqueue", i18n("Add Stream URL"), menuIcons ? Icons::self()->addRadioStreamIcon : Icon());
    promptClearPlayQueueAction = ActionCollection::get()->createAction("clearplaylist", i18n("Clear"), Icons::self()->clearListIcon);
    centerPlayQueueAction = ActionCollection::get()->createAction("centerplaylist", i18n("Center On Current Track"), Qt::RightToLeft==layoutDirection() ? "go-previous" : "go-next");
    songInfoAction = ActionCollection::get()->createAction("showsonginfo", i18n("Show Current Song Information"), Icons::self()->infoIcon);
    songInfoAction->setShortcut(Qt::Key_F12);
    songInfoAction->setCheckable(true);
    fullScreenAction = ActionCollection::get()->createAction("fullScreen", i18n("Full Screen"), menuIcons ? "view-fullscreen" : "");
    fullScreenAction->setShortcut(Qt::Key_F11);
    randomPlayQueueAction = ActionCollection::get()->createAction("randomplaylist", i18n("Random"), Icons::self()->shuffleIcon);
    repeatPlayQueueAction = ActionCollection::get()->createAction("repeatplaylist", i18n("Repeat"), Icons::self()->repeatIcon);
    singlePlayQueueAction = ActionCollection::get()->createAction("singleplaylist", i18n("Single"), Icons::self()->singleIcon, i18n("When 'Single' is activated, playback is stopped after current song, or song is repeated if 'Repeat' is enabled."));
    consumePlayQueueAction = ActionCollection::get()->createAction("consumeplaylist", i18n("Consume"), Icons::self()->consumeIcon, i18n("When consume is activated, a song is removed from the play queue after it has been played."));
    setPriorityAction = ActionCollection::get()->createAction("setprio", i18n("Set Priority"), Icon("favorites"));
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction = ActionCollection::get()->createAction("streamplay", i18n("Play Stream"));
    streamPlayAction->setCheckable(true);
    streamPlayAction->setChecked(false);
    streamPlayAction->setVisible(false);
    #endif
    locateTrackAction = ActionCollection::get()->createAction("locatetrack", i18n("Locate In Library"), "edit-find");
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction = ActionCollection::get()->createAction("editpqtags", i18n("Edit Song Tags"), "document-edit");
    #endif
    addAction(expandAllAction = ActionCollection::get()->createAction("expandall", i18n("Expand All")));
    expandAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Plus);
    addAction(collapseAllAction = ActionCollection::get()->createAction("collapseall", i18n("Collapse All")));
    collapseAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Minus);
    clearPlayQueueAction = ActionCollection::get()->createAction("confimclearplaylist", i18n("Remove All Songs"), Icons::self()->clearListIcon);
    clearPlayQueueAction->setShortcut(Qt::AltModifier+Qt::Key_Return);
    cancelAction = ActionCollection::get()->createAction("cancel", i18n("Cancel"), Icons::self()->cancelIcon);
    cancelAction->setShortcut(Qt::AltModifier+Qt::Key_Escape);
    connect(cancelAction, SIGNAL(triggered(bool)), messageWidget, SLOT(animatedHide()));
    connect(clearPlayQueueAction, SIGNAL(triggered(bool)), messageWidget, SLOT(animatedHide()));
    connect(clearPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(clearPlayQueue()));
    clearNewStateAction = ActionCollection::get()->createAction("clearnewstate", i18n("Clear 'New' State Of Artists and Albums"));
    connect(clearNewStateAction, SIGNAL(triggered(bool)), MusicLibraryModel::self(), SLOT(clearNewState()));
    connect(clearNewStateAction, SIGNAL(triggered(bool)), AlbumsModel::self(), SLOT(clearNewState()));
    connect(MusicLibraryModel::self(), SIGNAL(haveNewItems(bool)), clearNewStateAction, SLOT(setEnabled(bool)));
    clearNewStateAction->setEnabled(false);

    StdActions::self()->playPauseTrackAction->setEnabled(false);
    StdActions::self()->nextTrackAction->setEnabled(false);
    updateNextTrack(-1);
    StdActions::self()->prevTrackAction->setEnabled(false);
    enableStopActions(false);
    volumeSlider->initActions();

    connectionsAction->setMenu(new QMenu(this));
    connectionsGroup=new QActionGroup(connectionsAction->menu());
    outputsAction->setMenu(new QMenu(this));
    outputsAction->setVisible(false);
    addPlayQueueToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());

    playPauseTrackButton->setDefaultAction(StdActions::self()->playPauseTrackAction);
    stopTrackButton->setDefaultAction(StdActions::self()->stopPlaybackAction);
    nextTrackButton->setDefaultAction(StdActions::self()->nextTrackAction);
    prevTrackButton->setDefaultAction(StdActions::self()->prevTrackAction);

    QMenu *stopMenu=new QMenu(this);
    stopMenu->addAction(StdActions::self()->stopPlaybackAction);
    stopMenu->addAction(StdActions::self()->stopAfterCurrentTrackAction);
    stopTrackButton->setMenu(stopMenu);
    stopTrackButton->setPopupMode(QToolButton::DelayedPopup);

    promptClearPlayQueueAction->setEnabled(false);
    centerPlayQueueAction->setEnabled(false);
    StdActions::self()->savePlayQueueAction->setEnabled(false);
    addStreamToPlayQueueAction->setEnabled(false);
    clearPlayQueueButton->setDefaultAction(promptClearPlayQueueAction);
    savePlayQueueButton->setDefaultAction(StdActions::self()->savePlayQueueAction);
    centerPlayQueueButton->setDefaultAction(centerPlayQueueAction);
    randomButton->setDefaultAction(randomPlayQueueAction);
    repeatButton->setDefaultAction(repeatPlayQueueAction);
    singleButton->setDefaultAction(singlePlayQueueAction);
    consumeButton->setDefaultAction(consumePlayQueueAction);

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

    // Build sidebar...
    #define TAB_ACTION(A) A->icon(), A->text(), A->text()
    int sidebarPageShortcutKey=Qt::Key_1;
    addAction(showPlayQueueAction = ActionCollection::get()->createAction("showplayqueue", i18n("Play Queue"), Icons::self()->playqueueIcon));
    showPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_Q);
    tabWidget->addTab(playQueuePage, TAB_ACTION(showPlayQueueAction), playQueueInSidebar);
    connect(showPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(showPlayQueue()));
    libraryPage = new LibraryPage(this);
    addAction(libraryTabAction = ActionCollection::get()->createAction("showlibrarytab", i18n("Artists"), Icons::self()->artistsIcon));
    libraryTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(libraryPage, TAB_ACTION(libraryTabAction), !hiddenPages.contains(libraryPage->metaObject()->className()));
    connect(libraryTabAction, SIGNAL(triggered(bool)), this, SLOT(showLibraryTab()));
    albumsPage = new AlbumsPage(this);
    addAction(albumsTabAction = ActionCollection::get()->createAction("showalbumstab", i18n("Albums"), Icons::self()->albumsIcon));
    albumsTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(albumsPage, TAB_ACTION(albumsTabAction), !hiddenPages.contains(albumsPage->metaObject()->className()));
    connect(albumsTabAction, SIGNAL(triggered(bool)), this, SLOT(showAlbumsTab()));
    AlbumsModel::self()->setEnabled(!hiddenPages.contains(albumsPage->metaObject()->className()));
    folderPage = new FolderPage(this);
    addAction(foldersTabAction = ActionCollection::get()->createAction("showfolderstab", i18n("Folders"), Icons::self()->foldersIcon));
    foldersTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(folderPage, TAB_ACTION(foldersTabAction), !hiddenPages.contains(folderPage->metaObject()->className()));
    connect(foldersTabAction, SIGNAL(triggered(bool)), this, SLOT(showFoldersTab()));
    folderPage->setEnabled(!hiddenPages.contains(folderPage->metaObject()->className()));
    playlistsPage = new PlaylistsPage(this);
    addAction(playlistsTabAction = ActionCollection::get()->createAction("showplayliststab", i18n("Playlists"), Icons::self()->playlistsIcon));
    playlistsTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(playlistsPage, TAB_ACTION(playlistsTabAction), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    connect(playlistsTabAction, SIGNAL(triggered(bool)), this, SLOT(showPlaylistsTab()));
    setPlaylistsEnabled(!hiddenPages.contains(playlistsPage->metaObject()->className()));
    #ifdef ENABLE_DYNAMIC
    dynamicPage = new DynamicPage(this);
    addAction(dynamicTabAction = ActionCollection::get()->createAction("showdynamictab", i18n("Dynamic"), Icons::self()->dynamicIcon));
    dynamicTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(dynamicPage, TAB_ACTION(dynamicTabAction), !hiddenPages.contains(dynamicPage->metaObject()->className()));
    connect(dynamicTabAction, SIGNAL(triggered(bool)), this, SLOT(showDynamicTab()));
    connect(Dynamic::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(Dynamic::self(), SIGNAL(running(bool)), dynamicLabel, SLOT(setVisible(bool)));
    connect(Dynamic::self(), SIGNAL(running(bool)), this, SLOT(controlDynamicButton()));
    stopDynamicButton->setDefaultAction(Dynamic::self()->stopAct());
    #endif
    #ifdef ENABLE_STREAMS
    streamsPage = new StreamsPage(this);
    addAction(streamsTabAction = ActionCollection::get()->createAction("showstreamstab", i18n("Streams"), Icons::self()->streamsIcon));
    streamsTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(streamsPage, TAB_ACTION(streamsTabAction), !hiddenPages.contains(streamsPage->metaObject()->className()));
    connect(streamsTabAction, SIGNAL(triggered(bool)), this, SLOT(showStreamsTab()));
    connect(streamsPage, SIGNAL(add(const QStringList &, bool, quint8)), &playQueueModel, SLOT(addItems(const QStringList &, bool, quint8)));
    connect(streamsPage, SIGNAL(error(QString)), this, SLOT(showError(QString)));
    connect(streamsPage, SIGNAL(showPreferencesPage(QString)), this, SLOT(showPreferencesDialog(QString)));
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    onlinePage = new OnlineServicesPage(this);
    addAction(onlineTabAction = ActionCollection::get()->createAction("showonlinetab", i18n("Online"), Icons::self()->onlineIcon));
    onlineTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(onlinePage, TAB_ACTION(onlineTabAction), !hiddenPages.contains(onlinePage->metaObject()->className()));
    onlinePage->setEnabled(!hiddenPages.contains(onlinePage->metaObject()->className()));
    connect(onlineTabAction, SIGNAL(triggered(bool)), this, SLOT(showOnlineTab()));
    connect(onlinePage, SIGNAL(showPreferencesPage(QString)), this, SLOT(showPreferencesDialog(QString)));
    connect(onlinePage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(OnlineServicesModel::self(), SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage = new DevicesPage(this);
    addAction(devicesTabAction = ActionCollection::get()->createAction("showdevicestab", i18n("Devices"), Icons::self()->devicesIcon));
    devicesTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(devicesPage, TAB_ACTION(devicesTabAction), !hiddenPages.contains(devicesPage->metaObject()->className()));
    DevicesModel::self()->setEnabled(!hiddenPages.contains(devicesPage->metaObject()->className()));
    connect(devicesTabAction, SIGNAL(triggered(bool)), this, SLOT(showDevicesTab()));
    #endif
    searchPage = new SearchPage(this);
    addAction(searchTabAction = ActionCollection::get()->createAction("showsearchtab", i18n("Search"), Icons::self()->searchTabIcon));
    searchTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    connect(searchTabAction, SIGNAL(triggered(bool)), this, SLOT(showSearchTab()));
    connect(searchPage, SIGNAL(locate(QList<Song>)), this, SLOT(locateTracks(QList<Song>)));
    tabWidget->addTab(searchPage, TAB_ACTION(searchTabAction), !hiddenPages.contains(searchPage->metaObject()->className()));
    tabWidget->addTab(contextPage, Icons::self()->infoSidebarIcon, i18n("Info"), songInfoAction->text(),
                      !hiddenPages.contains(contextPage->metaObject()->className()));
    tabWidget->setStyle(Settings::self()->sidebar());

    if (playQueueInSidebar) {
        tabToggled(PAGE_PLAYQUEUE);
    } else {
        tabWidget->setCurrentIndex(PAGE_LIBRARY);
    }

    if (contextInSidebar) {
        tabToggled(PAGE_CONTEXT);
    } else {
        tabWidget->toggleTab(PAGE_CONTEXT, false);
    }
    initSizes();

    fullScreenAction->setCheckable(true);
    randomPlayQueueAction->setCheckable(true);
    repeatPlayQueueAction->setCheckable(true);
    singlePlayQueueAction->setCheckable(true);
    consumePlayQueueAction->setCheckable(true);

    songInfoButton->setDefaultAction(songInfoAction);
    if (Configuration(playQueuePage->metaObject()->className()).get(ItemView::constSearchActiveKey, false)) {
        playQueueSearchWidget->activate();
    } else {
        playQueueSearchWidget->setVisible(false);
    }
    QList<QToolButton *> playbackBtns=QList<QToolButton *>() << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    QList<QToolButton *> controlBtns=QList<QToolButton *>() << menuButton << songInfoButton;
    int playbackIconSize=28;
    int playPauseIconSize=32;
    int controlIconSize=22;
    int controlButtonSize=32;

    if (repeatButton->iconSize().height()>=32) {
        controlIconSize=playbackIconSize=48;
        controlButtonSize=54;
        playPauseIconSize=64;
    } else if (repeatButton->iconSize().height()>=22) {
        controlIconSize=playbackIconSize=32;
        controlButtonSize=36;
        playPauseIconSize=48;
    } else if (QLatin1String("oxygen")!=Icon::currentTheme().toLower() || (GtkStyle::isActive() && GtkStyle::useSymbolicIcons())) {
        // Oxygen does not have 24x24 icons, and media players seem to use scaled 28x28 icons...
        // But, if the theme does have media icons at 24x24 use these - as they will be sharper...
        playbackIconSize=24==Icons::self()->toolbarPlayIcon.actualSize(QSize(24, 24)).width() ? 24 : 28;
    }
    #ifdef USE_SYSTEM_MENU_ICON
    if (GtkStyle::isActive() && GtkStyle::useSymbolicIcons()) {
        controlIconSize=22==controlIconSize ? 16 : 32==controlIconSize ? 22 : 32;
    }
    #endif
    stopTrackButton->setHideMenuIndicator(true);
    int playbackButtonSize=28==playbackIconSize ? 34 : controlButtonSize;
    int controlButtonWidth=Utils::touchFriendly() ? controlButtonSize*TouchProxyStyle::constScaleFactor : controlButtonSize;
    int playbackButtonWidth=Utils::touchFriendly() ? playbackButtonSize*TouchProxyStyle::constScaleFactor : playbackButtonSize;
    foreach (QToolButton *b, controlBtns) {
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        #ifdef USE_SYSTEM_MENU_ICON
        if (b==menuButton && !GtkStyle::isActive()) {
            b->setFixedHeight(controlButtonSize);
        } else
        #endif
        b->setFixedSize(QSize(controlButtonWidth, controlButtonSize));
        b->setIconSize(QSize(controlIconSize, controlIconSize));
    }
    foreach (QToolButton *b, playbackBtns) {
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setFixedSize(QSize(playbackButtonWidth, playbackButtonSize));
        b->setIconSize(QSize(playbackIconSize, playbackIconSize));
    }

    playPauseTrackButton->setIconSize(QSize(playPauseIconSize, playPauseIconSize));
    playPauseTrackButton->setFixedSize(QSize((playPauseIconSize+6)*(Utils::touchFriendly() ? TouchProxyStyle::constScaleFactor : 1.0), playPauseIconSize+6));

    if (fullScreenAction->isEnabled()) {
        fullScreenAction->setChecked(Settings::self()->showFullScreen());
    }

    randomPlayQueueAction->setChecked(false);
    repeatPlayQueueAction->setChecked(false);
    singlePlayQueueAction->setChecked(false);
    consumePlayQueueAction->setChecked(false);

    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    MusicLibraryItemAlbum::setSortByDate(Settings::self()->libraryYear());
    AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());

    #ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys);
    #endif

    if (Settings::self()->firstRun()) {
        int width=playPauseTrackButton->width()*25;
        resize(playPauseTrackButton->width()*25, playPauseTrackButton->height()*18);
        splitter->setSizes(QList<int>() << width*0.4 << width*0.6);
    } else {
        QSize sz=Settings::self()->mainWindowSize();
        if (!sz.isEmpty() && sz.width()>0) {
            resize(sz);
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

    int menuCfg=Settings::self()->menu();
    #ifndef Q_OS_MAC
    if (Utils::Unity!=Utils::currentDe() && menuCfg&Settings::MC_Bar && menuCfg&Settings::MC_Button) {
        #ifdef ENABLE_KDE_SUPPORT
        showMenuAction=KStandardAction::showMenubar(this, SLOT(toggleMenubar()), ActionCollection::get());
        #else
        showMenuAction=ActionCollection::get()->createAction("showmenubar", i18n("Show Menubar"));
        showMenuAction->setShortcut(Qt::ControlModifier+Qt::Key_M);
        showMenuAction->setCheckable(true);
        connect(showMenuAction, SIGNAL(toggled(bool)), this, SLOT(toggleMenubar()));
        #endif
    }
    #endif

    if (menuCfg&Settings::MC_Button) {
        QMenu *mainMenu=new QMenu(this);
//        mainMenu->addAction(songInfoAction);
        mainMenu->addAction(fullScreenAction);
        mainMenu->addAction(connectionsAction);
        mainMenu->addAction(outputsAction);
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        mainMenu->addAction(streamPlayAction);
        #endif
        if (showMenuAction) {
            mainMenu->addAction(showMenuAction);
        }
        #ifdef ENABLE_KDE_SUPPORT
        mainMenu->addAction(prefAction);
        mainMenu->addAction(refreshDbAction);
        mainMenu->addAction(shortcutsAction);
        mainMenu->addSeparator();
        mainMenu->addAction(StdActions::self()->searchAction);
        mainMenu->addSeparator();
        mainMenu->addAction(serverInfoAction);
        mainMenu->addMenu(helpMenu());
        #else
        mainMenu->addAction(prefAction);
        mainMenu->addAction(refreshDbAction);
        mainMenu->addAction(clearNewStateAction);
        mainMenu->addSeparator();
        mainMenu->addAction(StdActions::self()->searchAction);
        mainMenu->addSeparator();
        mainMenu->addAction(serverInfoAction);
        mainMenu->addAction(aboutAction);
        #endif
        mainMenu->addSeparator();
        mainMenu->addAction(quitAction);
        menuButton->setIcon(Icons::self()->toolbarMenuIcon);
        menuButton->setAlignedMenu(mainMenu);
    } else {
        menuButton->deleteLater();
        menuButton=0;
    }

    if (menuCfg&Settings::MC_Bar) {
        QMenu *menu=new QMenu(i18n("&Music"), this);
        addMenuAction(menu, refreshDbAction);
        menu->addSeparator();
        addMenuAction(menu, connectionsAction);
        addMenuAction(menu, outputsAction);
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        addMenuAction(menu, streamPlayAction);
        #endif
        menu->addSeparator();
        addMenuAction(menu, quitAction);
        menuBar()->addMenu(menu);
        menu=new QMenu(i18n("&Edit"), this);
        addMenuAction(menu, playQueueModel.undoAct());
        addMenuAction(menu, playQueueModel.redoAct());
        menu->addSeparator();
        addMenuAction(menu, clearNewStateAction);
        menu->addSeparator();
        addMenuAction(menu, StdActions::self()->searchAction);
        #ifndef ENABLE_KDE_SUPPORT
        if (Utils::KDE!=Utils::currentDe()) {
            menu->addSeparator();
            addMenuAction(menu, prefAction);
        }
        #endif
        menuBar()->addMenu(menu);
        #ifndef ENABLE_KDE_SUPPORT
        if (Utils::KDE!=Utils::currentDe()) {
            menu=new QMenu(i18n("&View"), this);
            if (showMenuAction) {
                addMenuAction(menu, showMenuAction);
                menu->addSeparator();
            }
//            addMenuAction(menu, songInfoAction);
//            menu->addSeparator();
            addMenuAction(menu, fullScreenAction);
            menuBar()->addMenu(menu);
        }
        #endif
        menu=new QMenu(i18n("&Queue"), this);
//        addMenuAction(menu, promptClearPlayQueueAction);
//        addMenuAction(menu, StdActions::self()->savePlayQueueAction);
//        menu->addSeparator();
        addMenuAction(menu, addStreamToPlayQueueAction);
        menu->addSeparator();
        addMenuAction(menu, playQueueModel.shuffleAct());
        addMenuAction(menu, playQueueModel.sortAct());
        menuBar()->addMenu(menu);
        #ifndef ENABLE_KDE_SUPPORT
        if (Utils::KDE==Utils::currentDe())
        #endif
        {
            menu=new QMenu(i18n("&Settings"), this);
            if (showMenuAction) {
                addMenuAction(menu, showMenuAction);
            }
            addMenuAction(menu, fullScreenAction);
            addMenuAction(menu, songInfoAction);
            menu->addSeparator();
            #ifdef ENABLE_KDE_SUPPORT
            addMenuAction(menu, shortcutsAction);
            #endif
            addMenuAction(menu, prefAction);
            menuBar()->addMenu(menu);
        }
        menu=new QMenu(i18n("&Help"), this);
        addMenuAction(menu, serverInfoAction);
        #ifdef ENABLE_KDE_SUPPORT
        menu->addSeparator();
        foreach (QAction *act, helpMenu()->actions()) {
            addMenuAction(menu, act);
        }
        #else
        addMenuAction(menu, aboutAction);
        #endif
        menuBar()->addMenu(menu);
        GtkStyle::registerWidget(menuBar());
    }

    if (showMenuAction) {
        showMenuAction->setChecked(Settings::self()->showMenubar());
        toggleMenubar();
    }

    dynamicLabel->setVisible(false);
    stopDynamicButton->setVisible(false);
    StdActions::self()->addWithPriorityAction->setVisible(false);
    setPriorityAction->setVisible(false);
    setPriorityAction->setMenu(StdActions::self()->addWithPriorityAction->menu());

    playQueueProxyModel.setSourceModel(&playQueueModel);
    playQueue->setModel(&playQueueProxyModel);
    playQueue->addAction(playQueue->removeFromAct());
    playQueue->addAction(promptClearPlayQueueAction);
    playQueue->addAction(StdActions::self()->savePlayQueueAction);
    playQueue->addAction(addStreamToPlayQueueAction);
    playQueue->addAction(addPlayQueueToStoredPlaylistAction);
    playQueue->addAction(cropPlayQueueAction);
    playQueue->addAction(playQueueModel.shuffleAct());
    playQueue->addAction(playQueueModel.sortAct());
    playQueue->addAction(playQueueModel.undoAct());
    playQueue->addAction(playQueueModel.redoAct());
    Action *sep=new Action(this);
    sep->setSeparator(true);
    playQueue->addAction(sep);
    playQueue->addAction(stopAfterTrackAction);
    playQueue->addAction(setPriorityAction);
    playQueue->addAction(locateTrackAction);
    #ifdef TAGLIB_FOUND
    playQueue->addAction(editPlayQueueTagsAction);
    #endif
    playQueue->addAction(playQueueModel.removeDuplicatesAct());
    playQueue->readConfig();
    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());

    #ifdef ENABLE_DEVICES_SUPPORT
    connect(DevicesModel::self(), SIGNAL(addToDevice(const QString &)), this, SLOT(addToDevice(const QString &)));
    connect(DevicesModel::self(), SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    connect(libraryPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(playlistsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(searchPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(StdActions::self()->deleteSongsAction, SIGNAL(triggered(bool)), SLOT(deleteSongs()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    #endif
    connect(StdActions::self()->addPrioHighestAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioHighAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioMediumAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioLowAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioDefaultAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(StdActions::self()->addPrioCustomAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(outputsUpdated(const QList<Output> &)));
    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(pause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(play()), MPDConnection::self(), SLOT(play()));
    connect(this, SIGNAL(stop(bool)), MPDConnection::self(), SLOT(stopPlaying(bool)));
    connect(this, SIGNAL(getStatus()), MPDConnection::self(), SLOT(getStatus()));
    connect(this, SIGNAL(playListInfo()), MPDConnection::self(), SLOT(playListInfo()));
    connect(this, SIGNAL(currentSong()), MPDConnection::self(), SLOT(currentSong()));
    connect(this, SIGNAL(setSeekId(qint32, quint32)), MPDConnection::self(), SLOT(setSeekId(qint32, quint32)));
    connect(this, SIGNAL(startPlayingSongId(qint32)), MPDConnection::self(), SLOT(startPlayingSongId(qint32)));
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(this, SIGNAL(setPriority(const QList<qint32> &, quint8 )), MPDConnection::self(), SLOT(setPriority(const QList<qint32> &, quint8)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(&playQueueModel, SIGNAL(statsUpdated(int, quint32)), this, SLOT(updatePlayQueueStats(int, quint32)));
    connect(&playQueueModel, SIGNAL(fetchingStreams()), playQueue, SLOT(showSpinner()));
    connect(&playQueueModel, SIGNAL(streamsFetched()), playQueue, SLOT(hideSpinner()));
    connect(&playQueueModel, SIGNAL(updateCurrent(Song)), SLOT(updateCurrentSong(Song)));
    connect(&playQueueModel, SIGNAL(streamFetchStatus(QString)), playQueue, SLOT(streamFetchStatus(QString)));
    connect(playQueue, SIGNAL(cancelStreamFetch()), &playQueueModel, SLOT(cancelStreamFetch()));
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playQueueItemsSelected(bool)));
    connect(MPDStats::self(), SIGNAL(updated()), this, SLOT(updateStats()));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlayQueue(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
    connect(MPDConnection::self(), SIGNAL(info(const QString &)), SLOT(showInformation(const QString &)));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(checkMpdDir()));
    connect(refreshDbAction, SIGNAL(triggered(bool)), this, SLOT(refreshDbPromp()));
    connect(doDbRefreshAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(update()));
    connect(doDbRefreshAction, SIGNAL(triggered(bool)), messageWidget, SLOT(animatedHide()));
    connect(connectAction, SIGNAL(triggered(bool)), this, SLOT(connectToMpd()));
    connect(StdActions::self()->prevTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToPrevious()));
    connect(StdActions::self()->nextTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToNext()));
    connect(StdActions::self()->playPauseTrackAction, SIGNAL(triggered(bool)), this, SLOT(playPauseTrack()));
    connect(StdActions::self()->stopPlaybackAction, SIGNAL(triggered(bool)), this, SLOT(stopPlayback()));
    connect(StdActions::self()->stopAfterCurrentTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopAfterCurrentTrack()));
    connect(stopAfterTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopAfterTrack()));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(nowPlaying, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(randomPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(repeatPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(singlePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setSingle(bool)));
    connect(consumePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setConsume(bool)));
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    connect(streamPlayAction, SIGNAL(triggered(bool)), httpStream, SLOT(setEnabled(bool)));
    connect(MPDConnection::self(), SIGNAL(streamUrl(QString)), SLOT(streamUrl(QString)));
    #endif
    connect(playQueueSearchWidget, SIGNAL(returnPressed()), this, SLOT(searchPlayQueue()));
    connect(playQueueSearchWidget, SIGNAL(textChanged(const QString)), this, SLOT(searchPlayQueue()));
    connect(playQueueSearchWidget, SIGNAL(active(bool)), this, SLOT(playQueueSearchActivated(bool)));
    connect(playQueue, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playQueueItemActivated(const QModelIndex &)));
    connect(StdActions::self()->removeAction, SIGNAL(triggered(bool)), this, SLOT(removeItems()));
    connect(StdActions::self()->addToPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(addToPlayQueue()));
    connect(StdActions::self()->addRandomToPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(addRandomToPlayQueue()));
    connect(StdActions::self()->replacePlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(replacePlayQueue()));
    connect(playQueue->removeFromAct(), SIGNAL(triggered(bool)), this, SLOT(removeFromPlayQueue()));
    connect(promptClearPlayQueueAction, SIGNAL(triggered(bool)), playQueueSearchWidget, SLOT(clear()));
    connect(promptClearPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(promptClearPlayQueue()));
    connect(centerPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(centerPlayQueue()));
    connect(cropPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(cropPlayQueue()));
    connect(songInfoAction, SIGNAL(triggered(bool)), this, SLOT(showSongInfo()));
    connect(fullScreenAction, SIGNAL(triggered(bool)), this, SLOT(fullScreen()));
    #ifdef TAGLIB_FOUND
    connect(StdActions::self()->editTagsAction, SIGNAL(triggered(bool)), this, SLOT(editTags()));
    connect(editPlayQueueTagsAction, SIGNAL(triggered(bool)), this, SLOT(editPlayQueueTags()));
    connect(StdActions::self()->organiseFilesAction, SIGNAL(triggered(bool)), SLOT(organiseFiles()));
    #endif
    connect(context, SIGNAL(findArtist(QString)), this, SLOT(locateArtist(QString)));
    connect(context, SIGNAL(findAlbum(QString,QString)), this, SLOT(locateAlbum(QString,QString)));
    connect(context, SIGNAL(playSong(QString)), &playQueueModel, SLOT(playSong(QString)));
    connect(locateTrackAction, SIGNAL(triggered(bool)), this, SLOT(locateTrack()));
    connect(StdActions::self()->searchAction, SIGNAL(triggered(bool)), SLOT(showSearch()));
    connect(playQueue, SIGNAL(focusSearch(QString)), playQueueSearchWidget, SLOT(activate(QString)));
    connect(expandAllAction, SIGNAL(triggered(bool)), this, SLOT(expandAll()));
    connect(collapseAllAction, SIGNAL(triggered(bool)), this, SLOT(collapseAll()));
    connect(addStreamToPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(addStreamToPlayQueue()));
    connect(StdActions::self()->setCoverAction, SIGNAL(triggered(bool)), SLOT(setCover()));
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    connect(StdActions::self()->replaygainAction, SIGNAL(triggered(bool)), SLOT(replayGain()));
    #endif
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
    connect(playlistsPage, SIGNAL(add(const QStringList &, bool, quint8)), &playQueueModel, SLOT(addItems(const QStringList &, bool, quint8)));
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    connect(MountPoints::self(), SIGNAL(updated()), SLOT(checkMpdAccessibility()));
    #endif
    playQueueItemsSelected(false);
    playQueue->setFocus();

    MPDConnection::self()->start();
    connectToMpd();

    QString page=Settings::self()->page();
    for (int i=0; i<tabWidget->count(); ++i) {
        if (tabWidget->widget(i)->metaObject()->className()==page) {
            tabWidget->setCurrentIndex(i);
            break;
        }
    }

    connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
    connect(tabWidget, SIGNAL(tabToggled(int)), this, SLOT(tabToggled(int)));
    connect(tabWidget, SIGNAL(configRequested()), this, SLOT(showSidebarPreferencesPage()));

    #ifdef QT_QTDBUS_FOUND
    mpris=new Mpris(this);
    connect(mpris, SIGNAL(showMainWindow()), this, SLOT(restoreWindow()));
    CurrentCover::self()->setEnabled(true);
    #endif
    readSettings();
    updateConnectionsMenu();
    fadeStop=Settings::self()->stopFadeDuration()>Settings::MinFade;
    ActionCollection::get()->readSettings();

    if (testAttribute(Qt::WA_TranslucentBackground)) {
        // BUG: 146 - Work-around non-showing main window on start-up with transparent QtCurve windows.
        move(p.isNull() ? QPoint(96, 96) : p);
    }

    currentTabChanged(tabWidget->currentIndex());

    if (Settings::self()->firstRun() && MPDConnection::self()->isConnected()) {
        mpdConnectionStateChanged(true);
        updateStats();
    }
    #ifndef ENABLE_KDE_SUPPORT
    MediaKeys::self()->load();
    #endif
    updateActionToolTips();
    calcMinHeight();
}

MainWindow::~MainWindow()
{
    if (showMenuAction) {
        Settings::self()->saveShowMenubar(showMenuAction->isChecked());
    }
    bool hadCantataStreams=playQueueModel.removeCantataStreams();
    Settings::self()->saveShowFullScreen(fullScreenAction->isChecked());
    if (!fullScreenAction->isChecked()) {
        Settings::self()->saveMainWindowSize(size());
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    Settings::self()->savePlayStream(streamPlayAction->isVisible() && streamPlayAction->isChecked());
    #endif
    if (!fullScreenAction->isChecked() &&!tabWidget->isEnabled(PAGE_PLAYQUEUE)) {
        Settings::self()->saveSplitterState(splitter->saveState());
    }
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    playQueue->saveConfig();
    playlistsPage->saveConfig();
    context->saveConfig();
    #ifdef ENABLE_STREAMS
    StreamsModel::self()->save();
    #endif
    searchPage->saveConfig();
    nowPlaying->saveConfig();
    #ifdef ENABLE_ONLINE_SERVICES
    OnlineServicesModel::self()->save();
    #endif
    Settings::self()->saveForceSingleClick(TreeView::getForceSingleClick());
    Settings::StartupState startupState=Settings::self()->startupState();
    Settings::self()->saveStartHidden(trayItem->isActive() && Settings::self()->minimiseOnClose() &&
                                      ((isHidden() && Settings::SS_ShowMainWindow!=startupState) || (Settings::SS_HideMainWindow==startupState)));
    Settings::self()->save(true);
    disconnect(MPDConnection::self(), 0, 0, 0);
    #ifdef ENABLE_DYNAMIC
    if (Settings::self()->stopOnExit()) {
        Dynamic::self()->stop();
    }
    #endif
    if (Settings::self()->stopOnExit() || (fadeWhenStop() && StopState_Stopping==stopState)) {
        emit stop();
        // Allow time for stop to be sent...
        Utils::sleep();
    } else if (hadCantataStreams) {
        // Allow time for removal of cantata streams...
        Utils::sleep();
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->stop();
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    MediaKeys::self()->stop();
    #endif
    #ifdef TAGLIB_FOUND
    Tags::stop();
    #endif
    ThreadCleaner::self()->stopAll();
    Configuration(playQueuePage->metaObject()->className()).set(ItemView::constSearchActiveKey, playQueueSearchWidget->isActive());
}

void MainWindow::addMenuAction(QMenu *menu, QAction *action)
{
    menu->addAction(action);
    addAction(action); // Bind action to window, so that it works when fullscreen!
}

void MainWindow::initSizes()
{
    ItemView::setup();
    FancyTabWidget::setup();
    GroupedView::setup();
    ActionItemDelegate::setup();
    MusicLibraryItemAlbum::setup();
}

void MainWindow::showError(const QString &message, bool showActions)
{
    if (QLatin1String("NO_SONGS")==message) {
        messageWidget->setError(i18n("Failed to locate any songs matching the dynamic playlist rules."));
    } else {
        messageWidget->setError(message);
    }
    if (showActions) {
        messageWidget->setActions(QList<QAction*>() << prefAction << connectAction);
    } else {
        messageWidget->removeAllActions();
    }
    QApplication::alert(this);
}

void MainWindow::showInformation(const QString &message)
{
    messageWidget->setInformation(message);
    messageWidget->removeAllActions();
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    serverInfoAction->setEnabled(connected && !MPDConnection::self()->isMopdidy());
    refreshDbAction->setEnabled(connected);
    addStreamToPlayQueueAction->setEnabled(connected);
    if (connected) {
        messageWidget->hide();
        if (CS_Connected!=connectedState) {
            emit playListInfo();
            emit outputs();
            if (CS_Init!=connectedState) {
                currentTabChanged(tabWidget->currentIndex());
            }
            connectedState=CS_Connected;
            StdActions::self()->addWithPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
            setPriorityAction->setVisible(StdActions::self()->addWithPriorityAction->isVisible());
        }
        updateWindowTitle();
    } else {
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        searchPage->clear();
        connectedState=CS_Disconnected;
        outputsAction->setVisible(false);
        MPDStatus dummyStatus;
        updateStatus(&dummyStatus);
    }
    controlPlaylistActions();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if ((Qt::Key_Enter==event->key() || Qt::Key_Return==event->key()) && playQueue->hasFocus()) {
        QModelIndexList selection=playQueue->selectedIndexes();
        if (!selection.isEmpty()) {
            //play the first selected song
            playQueueItemActivated(selection.first());
        }
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
        MAIN_WINDOW_BASE_CLASS::closeEvent(event);
    }
}

void MainWindow::playQueueItemsSelected(bool s)
{
    int rc=playQueue->model()->rowCount();
    bool haveItems=rc>0;
    bool singleSelection=1==playQueue->selectedIndexes(false).count(); // Dont need sorted selection here...
    playQueue->removeFromAct()->setEnabled(s && haveItems);
    setPriorityAction->setEnabled(s && haveItems);
    locateTrackAction->setEnabled(singleSelection);
    cropPlayQueueAction->setEnabled(playQueue->haveUnSelectedItems() && haveItems);
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction->setEnabled(s && haveItems && MPDConnection::self()->getDetails().dirReadable);
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
        searchPage->clear();
        #ifdef ENABLE_DYNAMIC
        if (!MPDConnection::self()->getDetails().isEmpty() && details!=MPDConnection::self()->getDetails()) {
            Dynamic::self()->stop();
        }
        #endif
        showInformation(i18n("Connecting to %1", details.description()));
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

void MainWindow::streamUrl(const QString &u)
{
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction->setVisible(!u.isEmpty());
    streamPlayAction->setChecked(streamPlayAction->isVisible() && Settings::self()->playStream());
    httpStream->setEnabled(streamPlayAction->isChecked());
    #else
    Q_UNUSED(u)
    #endif
}

void MainWindow::refreshDbPromp()
{
    if (QDialogButtonBox::GnomeLayout==style()->styleHint(QStyle::SH_DialogButtonLayout)) {
        messageWidget->setActions(QList<QAction*>() << cancelAction << doDbRefreshAction);
    } else {
        messageWidget->setActions(QList<QAction*>() << doDbRefreshAction << cancelAction);
    }
    messageWidget->setWarning(i18n("Refresh MPD Database?"), false);
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
#else
void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, i18nc("Qt-only", "About Cantata"),
                       i18nc("Qt-only", "<b>Cantata %1</b><br/><br/>MPD client.<br/><br/>"
                             "&copy; 2011-2014 Craig Drummond<br/>Released under the <a href=\"http://www.gnu.org/licenses/gpl.html\">GPLv3</a>", PACKAGE_VERSION_STRING)+
                       QLatin1String("<br/><br/><i><small>")+i18n("Based upon <a href=\"http://qtmpc.lowblog.nl\">QtMPC</a> - &copy; 2007-2010 The QtMPC Authors<br/>")+
                       i18nc("Qt-only", "Context view backdrops courtesy of <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+QLatin1String("<br/>")+
                       i18nc("Qt-only", "Context view metadata courtesy of <a href=\"http://www.wikipedia.org\">Wikipedia</a> and <a href=\"http://www.last.fm\">Last.fm</a>")+
                       QLatin1String("<br/><br/>")+i18n("Please consider uploading your own music fan-art to <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+
                       QLatin1String("</small></i>"));
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

void MainWindow::showPreferencesDialog(const QString &page)
{
    if (PreferencesDialog::instanceCount()) {
        emit showPreferencesPage(page.isEmpty() ? "collection" : page);
    }
    if (PreferencesDialog::instanceCount() || !canShowDialog()) {
        return;
    }
    PreferencesDialog *pref=new PreferencesDialog(this);
    controlConnectionsMenu(false);
    connect(pref, SIGNAL(settingsSaved()), this, SLOT(updateSettings()));
    connect(pref, SIGNAL(destroyed()), SLOT(controlConnectionsMenu()));
    connect(this, SIGNAL(showPreferencesPage(QString)), pref, SLOT(showPage(QString)));
    pref->show();
    if (!page.isEmpty()) {
        pref->showPage(page);
    }
}

void MainWindow::quit()
{
    #ifdef ENABLE_ONLINE_SERVICES
    if (OnlineServicesModel::self()->isDownloading() &&
        MessageBox::No==MessageBox::warningYesNo(this, i18n("Podcasts are currently being downloaded\n\nQuiting now will abort all downloads."),
                                                 QString(), GuiItem(i18n("Abort downloads and quit")), GuiItem("Do not quit just yet"))) {
        return;
    }
    OnlineServicesModel::self()->cancelAll();
    #endif
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
    MPDConnection::self()->setDirReadable();
    #endif

    #ifdef TAGLIB_FOUND
    if (editPlayQueueTagsAction->isEnabled()) {
        editPlayQueueTagsAction->setEnabled(MPDConnection::self()->getDetails().dirReadable);
    }
    #endif
    if (currentPage) {
        currentPage->controlActions();
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
        QString current=Settings::self()->currentConnection();
        QSet<QString> cfg;
        QSet<QString> menuItems;
        QMenu *menu=connectionsAction->menu();
        foreach (const MPDConnectionDetails &d, connections) {
            cfg.insert(d.name);
        }

        foreach (QAction *act, menu->actions()) {
            menuItems.insert(act->data().toString());
            act->setChecked(act->data().toString()==current);
        }

        if (menuItems!=cfg) {
            menu->clear();
            qSort(connections);
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
    #ifdef ENABLE_DYNAMIC
    stopDynamicButton->setVisible(dynamicLabel->isVisible() && PAGE_DYNAMIC!=tabWidget->currentIndex());
    playQueueModel.enableUndo(!Dynamic::self()->isRunning());
    #endif
}

void MainWindow::readSettings()
{
    #ifndef QT_QTDBUS_FOUND
    CurrentCover::self()->setEnabled(Settings::self()->showPopups() || 0!=Settings::self()->playQueueBackground() || Settings::self()->showCoverWidget());
    #endif
    checkMpdDir();
    Covers::self()->readConfig();
    HttpServer::self()->readConfig();
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    Song::setUseComposer(Settings::self()->useComposer());
    albumsPage->setView(Settings::self()->albumsView());
    AlbumsModel::self()->setAlbumSort(Settings::self()->albumSort());
    libraryPage->setView(Settings::self()->libraryView());
    MusicLibraryModel::self()->setUseArtistImages(Settings::self()->libraryArtistImage());
    playlistsPage->setView(Settings::self()->playlistsView());
    #ifdef ENABLE_STREAMS
    streamsPage->setView(Settings::self()->streamsView());
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    onlinePage->setView(Settings::self()->onlineView());
    #endif
    folderPage->setView(Settings::self()->folderView());
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(Settings::self()->devicesView());
    #endif
    searchPage->setView(Settings::self()->searchView());
    trayItem->setup();
    autoScrollPlayQueue=Settings::self()->playQueueScroll();
    updateWindowTitle();
    TreeView::setForceSingleClick(Settings::self()->forceSingleClick());
    #ifdef QT_QTDBUS_FOUND
    PowerManagement::self()->setInhibitSuspend(Settings::self()->inhibitSuspend());
    #endif
    #ifndef ENABLE_KDE_SUPPORT
    MediaKeys::self()->load();
    #endif
    context->readConfig();
    tabWidget->setHiddenPages(Settings::self()->hiddenPages());
    tabWidget->setStyle(Settings::self()->sidebar());
    coverWidget->setEnabled(Settings::self()->showCoverWidget());
    stopTrackButton->setVisible(Settings::self()->showStopButton());
    calcMinHeight();
    toggleMonoIcons();
    toggleSplitterAutoHide();
    if (contextSwitchTime!=Settings::self()->contextSwitchTime()) {
        contextSwitchTime=Settings::self()->contextSwitchTime();
        if (0==contextSwitchTime && contextTimer) {
            contextTimer->stop();
            contextTimer->deleteLater();
            contextTimer=0;
        }
    }
}

static inline bool diffCoverSize(int a, int b)
{
    return (a==ItemView::Mode_IconTop && b!=ItemView::Mode_IconTop) || (a!=ItemView::Mode_IconTop && b==ItemView::Mode_IconTop);
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
    bool diffLibCovers=((int)MusicLibraryItemAlbum::currentCoverSize())!=Settings::self()->libraryCoverSize() ||
                       diffCoverSize(Settings::self()->libraryView(), libraryPage->viewMode());
    bool diffLibArtistImages=diffLibCovers ||
                       (libraryPage->viewMode()==ItemView::Mode_IconTop && Settings::self()->libraryView()!=ItemView::Mode_IconTop) ||
                       (libraryPage->viewMode()!=ItemView::Mode_IconTop && Settings::self()->libraryView()==ItemView::Mode_IconTop) ||
                       Settings::self()->libraryArtistImage()!=MusicLibraryModel::self()->useArtistImages();
    bool diffAlCovers=((int)AlbumsModel::currentCoverSize())!=Settings::self()->albumsCoverSize() ||
                      albumsPage->viewMode()!=Settings::self()->albumsView() ||
                      diffCoverSize(Settings::self()->albumsView(), albumsPage->viewMode());
    bool diffLibYear=MusicLibraryItemAlbum::sortByDate()!=Settings::self()->libraryYear();
    bool diffGrouping=MPDParseUtils::groupSingle()!=Settings::self()->groupSingle() ||
                      Song::useComposer()!=Settings::self()->useComposer();

    readSettings();

    if (diffLibArtistImages) {
        MusicLibraryItemArtist::clearDefaultCover();
    }
    if (diffLibCovers) {
        MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    }
    if (diffLibYear) {
        MusicLibraryItemAlbum::setSortByDate(Settings::self()->libraryYear());
    }
    if (diffLibArtistImages || diffLibCovers) {
        libraryPage->setView(libraryPage->viewMode());
    }
    if (diffAlCovers) {
        AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());
    }
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
        libraryPage->refresh();
    }
    #if defined ENABLE_ONLINE_SERVICES || defined ENABLE_DEVICES_SUPPORT
    if (diffLibCovers || diffLibYear || Settings::self()->libraryArtistImage()!=MusicLibraryModel::self()->useArtistImages()) {
        #ifdef ENABLE_ONLINE_SERVICES
        onlinePage->refresh();
        #endif
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesPage->refresh();
        #endif
    }
    #endif

    bool wasAutoExpand=playQueue->isAutoExpand();
    bool wasStartClosed=playQueue->isStartClosed();
    bool wasGrouped=playQueue->isGrouped();
    playQueue->readConfig();

    if (wasGrouped!=playQueue->isGrouped() ||
        (playQueue->isGrouped() && (wasAutoExpand!=playQueue->isAutoExpand() || wasStartClosed!=playQueue->isStartClosed())) ) {
        QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
        playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && MPDState_Playing==MPDStatus::self()->state());
    }

    wasStartClosed=playlistsPage->isStartClosed();
    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());
    if (ItemView::Mode_GroupedTree==Settings::self()->playlistsView() && wasStartClosed!=playlistsPage->isStartClosed()) {
        playlistsPage->updateRows();
    }
    updateActionToolTips();
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

void MainWindow::showServerInfo()
{
    QStringList handlers=MPDConnection::self()->urlHandlers().toList();
    QStringList tags=MPDConnection::self()->tags().toList();
    qSort(handlers);
    qSort(tags);
    long version=MPDConnection::self()->version();
    MessageBox::information(this, QLatin1String("<p><table>")+
                                  i18n("<tr><td colspan=\"2\"><b>Server</b></td></tr>"
                                       "<tr><td align=\"right\">Protocol:&nbsp;</td><td>%1.%2.%3</td></tr>"
                                       "<tr><td align=\"right\">Uptime:&nbsp;</td><td>%4</td></tr>"
                                       "<tr><td align=\"right\">Playing:&nbsp;</td><td>%5</td></tr>"
                                       "<tr><td align=\"right\">Handlers:&nbsp;</td><td>%6</td></tr>"
                                       "<tr><td align=\"right\">Tags:&nbsp;</td><td>%7</td></tr>",
                                       (version>>16)&0xFF, (version>>8)&0xFF, version&0xFF,
                                       Utils::formatDuration(MPDStats::self()->uptime()),
                                       Utils::formatDuration(MPDStats::self()->playtime()),
                                       handlers.join(", "), tags.join(", "))+
                                  QLatin1String("<tr/>")+
                                  i18n("<tr><td colspan=\"2\"><b>Database</b></td></tr>"
                                       "<tr><td align=\"right\">Artists:&nbsp;</td><td>%1</td></tr>"
                                       "<tr><td align=\"right\">Albums:&nbsp;</td><td>%2</td></tr>"
                                       "<tr><td align=\"right\">Songs:&nbsp;</td><td>%3</td></tr>"
                                       "<tr><td align=\"right\">Duration:&nbsp;</td><td>%4</td></tr>"
                                       "<tr><td align=\"right\">Updated:&nbsp;</td><td>%5</td></tr>",
                                       MPDStats::self()->artists(), MPDStats::self()->albums(), MPDStats::self()->songs(),
                                       Utils::formatDuration(MPDStats::self()->dbPlaytime()), MPDStats::self()->dbUpdate().toString(Qt::SystemLocaleShortDate))+
                                  QLatin1String("</table></p>"),
                            i18n("Server Information"));
}

void MainWindow::enableStopActions(bool enable)
{
    StdActions::self()->stopAfterCurrentTrackAction->setEnabled(enable);
    StdActions::self()->stopPlaybackAction->setEnabled(enable);
}

void MainWindow::stopPlayback()
{
    if (!fadeWhenStop() || MPDState_Paused==MPDStatus::self()->state() || 0==volume) {
        emit stop();
    }
    enableStopActions(false);
    StdActions::self()->nextTrackAction->setEnabled(false);
    StdActions::self()->prevTrackAction->setEnabled(false);
    startVolumeFade();
}

void MainWindow::stopAfterCurrentTrack()
{
    playQueueModel.clearStopAfterTrack();
    emit stop(true);
}

void MainWindow::stopAfterTrack()
{
    QModelIndexList selected=playQueue->selectedIndexes(false); // Dont need sorted selection here...
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
    volumeSlider->setFadingStop(true);
    if (!volumeFade) {
        volumeFade = new QPropertyAnimation(this, "volume");
        volumeFade->setDuration(Settings::self()->stopFadeDuration());
    }
    origVolume=lastVolume=volumeSlider->value();
    volumeFade->setStartValue(origVolume);
    volumeFade->setEndValue(-1);
    volumeFade->start();
}

void MainWindow::stopVolumeFade()
{
    if (StopState_None!=stopState) {
        stopState=StopState_None;
        volumeFade->stop();
        setMpdVolume(-1);
    }
}

void MainWindow::setMpdVolume(int v)
{
    if (-1==v) {
        volumeSlider->setFadingStop(false);
        emit setVolume(volumeSlider->value());
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
    switch (MPDStatus::self()->state()) {
    case MPDState_Playing:
        emit pause(true);
        break;
    case MPDState_Paused:
        stopVolumeFade();
        emit pause(false);
        break;
    default:
        if (playQueueModel.rowCount()>0) {
            stopVolumeFade();
            if (-1!=playQueueModel.currentSong() && -1!=playQueueModel.currentSongRow()) {
                emit startPlayingSongId(playQueueModel.currentSong());
            } else {
                emit play();
            }
        }
    }
}

void MainWindow::setPosition()
{
    emit setSeekId(MPDStatus::self()->songId(), nowPlaying->value());
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

    if (filter!=playQueueProxyModel.filterText()) {
        playQueue->setFilterActive(!filter.isEmpty());
        playQueue->clearSelection();
        playQueueProxyModel.update(filter);
        QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
        playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && MPDState_Playing==MPDStatus::self()->state());
        scrollPlayQueue();
    }
}

void MainWindow::playQueueSearchActivated(bool a)
{
    if (!a && playQueue->isVisible()) {
        playQueue->setFocus();
    }
}

void MainWindow::updatePlayQueue(const QList<Song> &songs)
{
    StdActions::self()->playPauseTrackAction->setEnabled(!songs.isEmpty());
    StdActions::self()->nextTrackAction->setEnabled(StdActions::self()->stopPlaybackAction->isEnabled() && songs.count()>1);
    StdActions::self()->prevTrackAction->setEnabled(StdActions::self()->stopPlaybackAction->isEnabled() && songs.count()>1);
    StdActions::self()->savePlayQueueAction->setEnabled(!songs.isEmpty());
    promptClearPlayQueueAction->setEnabled(!songs.isEmpty());
    centerPlayQueueAction->setEnabled(songs.count()>1);

    int topRow=-1;
    QModelIndex topIndex=playQueueModel.lastCommandWasUnodOrRedo() ? playQueue->indexAt(QPoint(0, 0)) : QModelIndex();
    if (topIndex.isValid()) {
        topRow=playQueueProxyModel.mapToSource(topIndex).row();
    }
    bool wasEmpty=0==playQueueModel.rowCount();
    playQueueModel.update(songs);
    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
    bool scroll=autoScrollPlayQueue && playQueueProxyModel.isEmpty() && wasEmpty && MPDState_Playing==MPDStatus::self()->state();
    playQueue->updateRows(idx.row(), current.key, scroll);
    if (!scroll && topRow>0 && topRow<playQueueModel.rowCount()) {
        playQueue->scrollTo(playQueueProxyModel.mapFromSource(playQueueModel.index(topRow, 0)), QAbstractItemView::PositionAtTop);
    }

    if (songs.isEmpty()) {
        updateCurrentSong(Song());
    } else if (current.isStandardStream()) {
        // Check to see if it has been updated...
        Song pqSong=playQueueModel.getSongByRow(playQueueModel.currentSongRow());
        if (pqSong.artist!=current.artist || pqSong.album!=current.album || pqSong.title!=current.title || pqSong.year!=current.year || pqSong.name()!=current.name() ) {
            updateCurrentSong(pqSong);
        }
    }
    playQueueItemsSelected(playQueue->haveSelectedItems());
}

void MainWindow::updateWindowTitle()
{
    bool multipleConnections=connectionsAction->isVisible();
    QString connection=MPDConnection::self()->getDetails().getName();
    setWindowTitle(multipleConnections ? i18n("Cantata (%1)", connection) : "Cantata");
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

    if (current.isCantataStream()) {
        Song mod=HttpServer::self()->decodeUrl(current.file);
        if (!mod.title.isEmpty()) {
            current=mod;
            current.id=song.id;
        }
    }

    CurrentCover::self()->update(current);
    #ifdef QT_QTDBUS_FOUND
    mpris->updateCurrentSong(current);
    #endif
    if (current.time<5 && MPDStatus::self()->songId()==current.id && MPDStatus::self()->timeTotal()>5) {
        current.time=MPDStatus::self()->timeTotal();
    }
    nowPlaying->setEnabled(-1!=current.id && !current.isCdda() && (!currentIsStream() || current.time>5));
    nowPlaying->update(current);
    bool isPlaying=MPDState_Playing==MPDStatus::self()->state();
    playQueueModel.updateCurrentSong(current.id);
    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
    playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && isPlaying);
    scrollPlayQueue();
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
        if (!MusicLibraryModel::self()->lastUpdate().isValid()) {
            libraryPage->clear();
            //albumsPage->clear();
            folderPage->clear();
            playlistsPage->clear();
        }
        albumsPage->goTop();
        libraryPage->refresh();
        folderPage->refresh();
    }
}

void MainWindow::updateStatus()
{
    updateStatus(MPDStatus::self());
}

void MainWindow::updateStatus(MPDStatus * const status)
{
    if (!status->error().isEmpty()) {
        showError(i18n("MPD reported the following error: %1", status->error()));
    }

    if (MPDState_Stopped==status->state() || MPDState_Inactive==status->state()) {
        nowPlaying->clearTimes();
        playQueueModel.clearStopAfterTrack();
        if (statusTimer) {
            statusTimer->stop();
            statusTimer->setProperty("count", 0);
        }
    } else {
        nowPlaying->setRange(0, 0==status->timeTotal() && 0!=current.time && (current.isCdda() || current.isCantataStream())
                                    ? current.time : status->timeTotal());
        nowPlaying->setValue(status->timeElapsed());
        if (0==status->timeTotal() && 0==status->timeElapsed()) {
            if (!statusTimer) {
                statusTimer=new QTimer(this);
                statusTimer->setSingleShot(true);
                connect(statusTimer, SIGNAL(timeout()), SIGNAL(getStatus()));
            }
            QVariant id=statusTimer->property("id");
            if (!id.isValid() || id.toInt()!=current.id) {
                statusTimer->setProperty("id", current.id);
                statusTimer->setProperty("count", 0);
                statusTimer->start(250);
            } else if (statusTimer->property("count").toInt()<12) {
                statusTimer->setProperty("count", statusTimer->property("count").toInt()+1);
                statusTimer->start(250);
            }
        } else if (!nowPlaying->isEnabled()) {
            nowPlaying->setEnabled(-1!=current.id && !current.isCdda() && (!currentIsStream() || status->timeTotal()>5));
        }
    }

    randomPlayQueueAction->setChecked(status->random());
    repeatPlayQueueAction->setChecked(status->repeat());
    singlePlayQueueAction->setChecked(status->single());
    consumePlayQueueAction->setChecked(status->consume());
    updateNextTrack(status->nextSongId());

    if (status->timeElapsed()<172800 && (!currentIsStream() || (status->timeTotal()>0 && status->timeElapsed()<=status->timeTotal()))) {
        if (status->state() == MPDState_Stopped || status->state() == MPDState_Inactive) {
            nowPlaying->setRange(0, 0);
        } else {
            nowPlaying->setValue(status->timeElapsed());
        }
    }

    playQueueModel.setState(status->state());
    switch (status->state()) {
    case MPDState_Playing:
        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPauseIcon);
        StdActions::self()->playPauseTrackAction->setEnabled(0!=status->playlistLength());
        if (StopState_Stopping!=stopState) {
            enableStopActions(true);
            StdActions::self()->nextTrackAction->setEnabled(status->playlistLength()>1);
            StdActions::self()->prevTrackAction->setEnabled(status->playlistLength()>1);
        }
        nowPlaying->startTimer();
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
        StdActions::self()->playPauseTrackAction->setEnabled(0!=status->playlistLength());
        enableStopActions(false);
        StdActions::self()->nextTrackAction->setEnabled(false);
        StdActions::self()->prevTrackAction->setEnabled(false);
        if (!StdActions::self()->playPauseTrackAction->isEnabled()) {
            current=Song();
            nowPlaying->update(current);
            CurrentCover::self()->update(current);
        }
        current.id=0;
        trayItem->setToolTip("cantata", i18n("Cantata"), QLatin1String("<i>")+i18n("Playback stopped")+QLatin1String("</i>"));
        nowPlaying->stopTimer();
        break;
    case MPDState_Paused:
        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
        StdActions::self()->playPauseTrackAction->setEnabled(0!=status->playlistLength());
        enableStopActions(0!=status->playlistLength());
        StdActions::self()->nextTrackAction->setEnabled(status->playlistLength()>1);
        StdActions::self()->prevTrackAction->setEnabled(status->playlistLength()>1);
        nowPlaying->stopTimer();
    default:
        break;
    }

    // Check if song has changed or we're playing again after being stopped, and update song info if needed
    if (MPDState_Inactive!=status->state() &&
        (MPDState_Inactive==lastState || (MPDState_Stopped==lastState && MPDState_Playing==status->state()) || lastSongId != status->songId())) {
        emit currentSong();
    }
    if (status->state()!=lastState && (MPDState_Playing==status->state() || MPDState_Stopped==status->state())) {
        startContextTimer();
    }

    // Update status info
    lastState = status->state();
    lastSongId = status->songId();
}

void MainWindow::playQueueItemActivated(const QModelIndex &index)
{
    emit startPlayingSongId(playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(index).row()));
}

void MainWindow::promptClearPlayQueue()
{
    if (Settings::self()->playQueueConfirmClear()) {
        if (QDialogButtonBox::GnomeLayout==style()->styleHint(QStyle::SH_DialogButtonLayout)) {
            messageWidget->setActions(QList<QAction*>() << cancelAction << clearPlayQueueAction);
        } else {
            messageWidget->setActions(QList<QAction*>() << clearPlayQueueAction << cancelAction);
        }
        messageWidget->setWarning(i18n("Remove all songs from play queue?"), false);
    } else {
        clearPlayQueue();
    }
}

void MainWindow::clearPlayQueue()
{
    if (dynamicLabel->isVisible()) {
        #ifdef ENABLE_DYNAMIC
        Dynamic::self()->stop(true);
        #endif
    } else {
        playQueueModel.removeAll();
    }
}

void MainWindow::centerPlayQueue()
{
    QModelIndex idx=playQueueProxyModel.mapFromSource(playQueueModel.index(playQueueModel.currentSongRow(), 0));
    if (idx.isValid()) {
        if (playQueue->isGrouped()) {
            playQueue->updateRows(idx.row(), current.key, true, true);
        } else {
            playQueue->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }
    }
}

void MainWindow::addToPlayQueue(bool replace, quint8 priority, bool randomAlbums)
{
    playQueueSearchWidget->clear();
    if (currentPage) {
        currentPage->addSelectionToPlaylist(QString(), replace, priority, randomAlbums);
    }
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

        if (name==StreamsModel::constPlayListName) {
            MessageBox::error(this, i18n("<b>%1</b> is used to store favorite streams, please choose another name.", name));
            continue;
        }
        if (PlaylistsModel::self()->exists(name)) {
            switch(MessageBox::warningYesNoCancel(this, i18n("A playlist named <b>%1</b> already exists!<br/>Add to that playlist?", name),
                                                  i18n("Existing Playlist"))) {
            case MessageBox::Cancel:  return;
            case MessageBox::Yes:     break;
            case MessageBox::No:
            default:                  continue;
            }
        }

        if (!name.isEmpty()) {
            addToExistingStoredPlaylist(name, pq);
        }
        break;
    }
}

void MainWindow::addToExistingStoredPlaylist(const QString &name, bool pq)
{
    if (pq) {
        QModelIndexList items = playQueue->selectedIndexes();
        QStringList files;
        if (items.isEmpty()) {
            files = playQueueModel.filenames();
        } else {
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
    } else if (currentPage) {
        currentPage->addSelectionToPlaylist(name);
    }
}

void MainWindow::addStreamToPlayQueue()
{
    #ifdef ENABLE_STREAMS
    StreamDialog dlg(this, true);

    if (QDialog::Accepted==dlg.exec()) {
        QString url=dlg.url();

        if (dlg.save()) {
            StreamsModel::self()->addToFavourites(url, dlg.name());
        }
        playQueueModel.addItems(QStringList() << StreamsModel::modifyUrl(url), false, 0);
    }
    #else
    QString url = InputDialog::getText(i18n("Stream URL"), i18n("Enter URL of stream:"), QString(), 0, this).trimmed();
    if (!url.isEmpty()) {
        if (!MPDConnection::self()->urlHandlers().contains(QUrl(url).scheme())) {
            MessageBox::error(this, i18n("Invalid, or unsupported, URL!"));
        } else {
            playQueueModel.addItems(QStringList() << StreamsModel::modifyUrl(url), false, 0);
        }
    }
    #endif
}

void MainWindow::removeItems()
{
    if (currentPage) {
        currentPage->removeItems();
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
    if (0==songs) {
        playQueueStatsLabel->setText(QString());
    } else if (0==time) {
        playQueueStatsLabel->setText(Plurals::tracks(songs));
    } else {
        playQueueStatsLabel->setText(Plurals::tracksWithDuration(songs, Utils::formatDuration(time)));
    }
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
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::currentTabChanged(int index)
{
    prevPage=index;
    controlDynamicButton();
    switch(index) {
    case PAGE_LIBRARY:   currentPage=libraryPage;   break;
    case PAGE_ALBUMS:    currentPage=albumsPage;    break;
    case PAGE_FOLDERS:   folderPage->load();  currentPage=folderPage; break;
    case PAGE_PLAYLISTS: currentPage=playlistsPage; break;
    #ifdef ENABLE_DYNAMIC
    case PAGE_DYNAMIC:   currentPage=dynamicPage;   break;
    #endif
    #ifdef ENABLE_STREAMS
    case PAGE_STREAMS:   currentPage=streamsPage;   break;
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    case PAGE_ONLINE:    currentPage=onlinePage;    break;
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    case PAGE_DEVICES:   currentPage=devicesPage;   break;
    #endif
    case PAGE_SEARCH:    currentPage=searchPage;    break;
    default:             currentPage=0;             break;
    }
    if (currentPage) {
        currentPage->controlActions();
    }
}

void MainWindow::tabToggled(int index)
{
    switch (index) {
    case PAGE_PLAYQUEUE:
        if (tabWidget->isEnabled(index)) {
            splitter->setAutohidable(0, Settings::self()->splitterAutoHide() && !tabWidget->isEnabled(PAGE_PLAYQUEUE));
            playQueueWidget->setParent(playQueuePage);
            playQueuePage->layout()->addWidget(playQueueWidget);
            playQueueWidget->setVisible(true);
        } else {
            playQueuePage->layout()->removeWidget(playQueueWidget);
            playQueueWidget->setParent(splitter);
            playQueueWidget->setVisible(true);
            splitter->setAutohidable(0, Settings::self()->splitterAutoHide() && !tabWidget->isEnabled(PAGE_PLAYQUEUE));
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
        break;
    case PAGE_PLAYLISTS:
        setPlaylistsEnabled(tabWidget->isEnabled(index));
        break;
    #ifdef ENABLE_STREAMS
    case PAGE_STREAMS:
        break;
    #endif
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
}

void MainWindow::toggleSplitterAutoHide()
{
    bool ah=Settings::self()->splitterAutoHide();
    if (splitter->isAutoHideEnabled()!=ah) {
        splitter->setAutoHideEnabled(ah);
        splitter->setAutohidable(0, ah);
    }
}

void MainWindow::toggleMonoIcons()
{
    if (Settings::self()->monoSidebarIcons()!=Icons::self()->monoSidebarIcons()) {
        Icons::self()->initSidebarIcons();
        showPlayQueueAction->setIcon(Icons::self()->playqueueIcon);
        tabWidget->setIcon(PAGE_PLAYQUEUE, showPlayQueueAction->icon());
        libraryTabAction->setIcon(Icons::self()->artistsIcon);
        tabWidget->setIcon(PAGE_LIBRARY, libraryTabAction->icon());
        albumsTabAction->setIcon(Icons::self()->albumsIcon);
        tabWidget->setIcon(PAGE_ALBUMS, albumsTabAction->icon());
        foldersTabAction->setIcon(Icons::self()->foldersIcon);
        tabWidget->setIcon(PAGE_FOLDERS, foldersTabAction->icon());
        playlistsTabAction->setIcon(Icons::self()->playlistsIcon);
        tabWidget->setIcon(PAGE_PLAYLISTS, playlistsTabAction->icon());
        #ifdef ENABLE_DYNAMIC
        dynamicTabAction->setIcon(Icons::self()->dynamicIcon);
        tabWidget->setIcon(PAGE_DYNAMIC, dynamicTabAction->icon());
        #endif
        #ifdef ENABLE_STREAMS
        streamsTabAction->setIcon(Icons::self()->streamsIcon);
        tabWidget->setIcon(PAGE_STREAMS, streamsTabAction->icon());
        #endif
        #ifdef ENABLE_ONLINE_SERVICES
        onlineTabAction->setIcon(Icons::self()->onlineIcon);
        tabWidget->setIcon(PAGE_ONLINE, onlineTabAction->icon());
        #endif
        tabWidget->setIcon(PAGE_CONTEXT, Icons::self()->infoSidebarIcon);
        #ifdef ENABLE_DEVICES_SUPPORT
        devicesTabAction->setIcon(Icons::self()->devicesIcon);
        tabWidget->setIcon(PAGE_DEVICES, devicesTabAction->icon());
        #endif
        searchTabAction->setIcon(Icons::self()->searchTabIcon);
        tabWidget->setIcon(PAGE_SEARCH, searchTabAction->icon());
        tabWidget->recreate();
    }
}

void MainWindow::locateTracks(const QList<Song> &songs)
{
    if (!songs.isEmpty() && tabWidget->isEnabled(PAGE_LIBRARY)) {
        showLibraryTab();
        libraryPage->showSongs(songs);
    }
}

void MainWindow::locateTrack()
{
    locateTracks(playQueue->selectedSongs());
}

void MainWindow::locateArtist(const QString &artist)
{
    if (songInfoAction->isCheckable()) {
        songInfoAction->setChecked(false);
        showSongInfo();
    }
    showLibraryTab();
    libraryPage->showArtist(artist);
}

void MainWindow::locateAlbum(const QString &artist, const QString &album)
{
    if (songInfoAction->isCheckable()) {
        songInfoAction->setChecked(false);
        showSongInfo();
    }
    showLibraryTab();
    libraryPage->showAlbum(artist, album);
}

void MainWindow::dynamicStatus(const QString &message)
{
    #ifdef ENABLE_DYNAMIC
    Dynamic::self()->helperMessage(message);
    #else
    Q_UNUSED(message)
    #endif
}

void MainWindow::showSearch()
{
    if (playQueue->hasFocus()) {
        playQueueSearchWidget->activate();
    } else if (context->isVisible()) {
        context->search();
    } else if (currentPage && splitter->sizes().at(0)>0) {
        currentPage->focusSearch();
    } else if (playQueuePage->isVisible() || playQueue->isVisible()) {
        playQueueSearchWidget->activate();
    }
}

void MainWindow::expandAll()
{
    QWidget *f=QApplication::focusWidget();
    if (f && qobject_cast<TreeView *>(f) && !qobject_cast<GroupedView *>(f)) {
        static_cast<TreeView *>(f)->expandAll(QModelIndex(), true);
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
    if (currentPage==folderPage) {
        songs=folderPage->selectedSongs(FolderPage::ES_FillEmpty);
    } else if (currentPage) {
        songs=currentPage->selectedSongs();
    }
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
    if (songs.isEmpty() && isPlayQueue) {
        MessageBox::error(this, i18n("Can only edit tags of songs within MPD's music collection."));
    }
    if (songs.isEmpty() || TagEditor::instanceCount() || !canShowDialog()) {
        return;
    }
    QSet<QString> artists, albumArtists, composers, albums, genres;
    QString udi;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!isPlayQueue && currentPage==devicesPage) {
        DevicesModel::self()->getDetails(artists, albumArtists, composers, albums, genres);
        udi=devicesPage->activeFsDeviceUdi();
        if (udi.isEmpty()) {
            return;
        }
    } else
    #else
    Q_UNUSED(isPlayQueue)
    #endif
    MusicLibraryModel::self()->getDetails(artists, albumArtists, composers, albums, genres);
    TagEditor *dlg=new TagEditor(this, songs, artists, albumArtists, composers, albums, genres, udi);
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
    if (currentPage) {
        songs=currentPage->selectedSongs();
    }

    if (!songs.isEmpty()) {
        QString udi;
        #ifdef ENABLE_DEVICES_SUPPORT
        if (currentPage==devicesPage) {
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
    if (currentPage) {
        currentPage->addSelectionToDevice(udi);
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
    if (currentPage) {
        currentPage->deleteSongs();
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
    Q_UNUSED(from) Q_UNUSED(to) Q_UNUSED(songs)
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
    Q_UNUSED(from) Q_UNUSED(songs)
    #endif
}

void MainWindow::replayGain()
{
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (RgDialog::instanceCount() || !canShowDialog()) {
        return;
    }

    QList<Song> songs;
    if (currentPage==folderPage) {
        songs=folderPage->selectedSongs(FolderPage::ES_GuessTags);
    } else if (currentPage) {
        songs=currentPage->selectedSongs();
    }

    if (!songs.isEmpty()) {
        QString udi;
        #ifdef ENABLE_DEVICES_SUPPORT
        if (currentPage==devicesPage) {
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
    if (currentPage) {
        song=currentPage->coverRequest();
    }

    if (!song.isEmpty()) {
        CoverDialog *dlg=new CoverDialog(this);
        dlg->show(song);
    }
}

void MainWindow::updateNextTrack(int nextTrackId)
{
    if (-1!=nextTrackId && MPDState_Stopped==MPDStatus::self()->state()) {
        nextTrackId=-1; // nextSongId is not accurate if we are stopped.
    }
    QString tt=StdActions::self()->nextTrackAction->property("tooltip").toString();
    if (-1==nextTrackId && tt.isEmpty()) {
        StdActions::self()->nextTrackAction->setProperty("tooltip", StdActions::self()->nextTrackAction->toolTip());
    } else if (-1==nextTrackId) {
        StdActions::self()->nextTrackAction->setToolTip(tt);
        StdActions::self()->nextTrackAction->setProperty("trackid", nextTrackId);
    } else if (nextTrackId!=StdActions::self()->nextTrackAction->property("trackid").toInt()) {
        Song s=playQueueModel.getSongByRow(playQueueModel.getRowById(nextTrackId));
        if (!s.artist.isEmpty() && !s.title.isEmpty()) {
            tt+=QLatin1String("<br/><i><small>")+s.artistSong()+QLatin1String("<small></i>");
        } else {
            nextTrackId=-1;
        }
        StdActions::self()->nextTrackAction->setToolTip(tt);
        StdActions::self()->nextTrackAction->setProperty("trackid", nextTrackId);
    }
}

void MainWindow::updateActionToolTips()
{
    ActionCollection::get()->updateToolTips();
    tabWidget->setToolTip(PAGE_PLAYQUEUE, showPlayQueueAction->toolTip());
    tabWidget->setToolTip(PAGE_LIBRARY, libraryTabAction->toolTip());
    tabWidget->setToolTip(PAGE_ALBUMS, albumsTabAction->toolTip());
    tabWidget->setToolTip(PAGE_FOLDERS, foldersTabAction->toolTip());
    tabWidget->setToolTip(PAGE_PLAYLISTS, playlistsTabAction->toolTip());
    #ifdef ENABLE_DYNAMIC
    tabWidget->setToolTip(PAGE_DYNAMIC, dynamicTabAction->toolTip());
    #endif
    #ifdef ENABLE_STREAMS
    tabWidget->setToolTip(PAGE_STREAMS, streamsTabAction->toolTip());
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    tabWidget->setToolTip(PAGE_ONLINE, onlineTabAction->toolTip());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    tabWidget->setToolTip(PAGE_DEVICES, devicesTabAction->toolTip());
    #endif
    tabWidget->setToolTip(PAGE_SEARCH, searchTabAction->toolTip());
    tabWidget->setToolTip(PAGE_CONTEXT, songInfoAction->toolTip());
}

void MainWindow::setPlaylistsEnabled(bool e)
{
    PlaylistsModel::self()->setEnabled(e);
    controlPlaylistActions();
}

void MainWindow::controlPlaylistActions()
{
    bool enable=!MPDConnection::self()->isMopdidy() && PlaylistsModel::self()->isEnabled();
    StdActions::self()->addToStoredPlaylistAction->setVisible(enable);
    StdActions::self()->savePlayQueueAction->setVisible(enable);
    savePlayQueueButton->setVisible(enable);
    addPlayQueueToStoredPlaylistAction->setVisible(enable);
}

void MainWindow::startContextTimer()
{
    if (!contextSwitchTime || current.isStandardStream()) {
        return;
    }
    if (!contextTimer) {
        contextTimer=new QTimer(this);
        contextTimer->setSingleShot(true);
        connect(contextTimer, SIGNAL(timeout()), this, SLOT(toggleContext()));
    }
    contextTimer->start(contextSwitchTime);
}

void MainWindow::calcMinHeight()
{
    int minH=0;
    if (tabWidget->style()&FancyTabWidget::Side && tabWidget->style()&FancyTabWidget::Large) {
        minH=toolbar->height()+(tabWidget->visibleCount()*tabWidget->tabSize().height());
    } else {
        minH=Utils::scaleForDpi(256);
    }
    setMinimumHeight(minH);
    if (height()<minH) {
        resize(width(), minH);
    }
}

void MainWindow::toggleContext()
{
    if ( songInfoButton->isVisible()) {
         if ( (MPDState_Playing==MPDStatus::self()->state() && !songInfoAction->isChecked()) ||
               (MPDState_Stopped==MPDStatus::self()->state() && songInfoAction->isChecked()) ) {
            songInfoAction->trigger();
         }
    } else if (MPDState_Playing==MPDStatus::self()->state() && PAGE_CONTEXT!=tabWidget->currentIndex()) {
        int pp=prevPage;
        showTab(PAGE_CONTEXT);
        prevPage=pp;
    } else if (MPDState_Stopped==MPDStatus::self()->state() && PAGE_CONTEXT==tabWidget->currentIndex() && -1!=prevPage) {
        showTab(prevPage);
    }
}

void MainWindow::toggleMenubar()
{
    if (showMenuAction && menuButton) {
        menuButton->setVisible(!showMenuAction->isChecked());
        menuBar()->setVisible(showMenuAction->isChecked());
    }
}

#if !defined Q_OS_WIN && !defined Q_OS_MAC && QT_VERSION < 0x050000
#include <QX11Info>
#include <X11/Xlib.h>
#endif

void MainWindow::hideWindow()
{
    lastPos=pos();
    hide();
}

void MainWindow::restoreWindow()
{
    bool wasHidden=isHidden();
    #ifdef Q_OS_WIN // FIXME - need on mac?
    raiseWindow(this);
    #endif
    raise();
    showNormal();
    activateWindow();
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    // This section seems to be required for compiz, so that MPRIS.Raise actually shows the window, and not just highlight launcher.
    #if QT_VERSION < 0x050000
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
    xev.xclient.data.l[1] = xev.xclient.data.l[2] = xev.xclient.data.l[3] = xev.xclient.data.l[4] = 0;
    XSendEvent(QX11Info::display(), QX11Info::appRootWindow(info.screen()), False, SubstructureRedirectMask|SubstructureNotifyMask, &xev);
    #else // QT_VERSION < 0x050000
    QString wmctrl=Utils::findExe(QLatin1String("wmctrl"));
    if (!wmctrl.isEmpty()) {
        if (wasHidden) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        QProcess::execute(wmctrl, QStringList() << QLatin1String("-i") << QLatin1String("-a") << QString::number(effectiveWinId()));
    }
    #endif // QT_VERSION < 0x050000
    #endif // !defined Q_OS_WIN && !defined Q_OS_MAC
    if (wasHidden && !lastPos.isNull()) {
        move(lastPos);
    }
    #ifdef ENABLE_KDE_SUPPORT
    KWindowSystem::forceActiveWindow(effectiveWinId());
    #endif
}

#ifdef Q_OS_WIN
// This is down here, because windows.h includes ALL windows stuff - and we get conflicts with MessageBox :-(
#include <windows.h>
static void raiseWindow(QWidget *w)
{
    ::SetWindowPos(reinterpret_cast<HWND>(w->effectiveWinId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::SetWindowPos(reinterpret_cast<HWND>(w->effectiveWinId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}
#endif
