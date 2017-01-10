/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "mpd-interface/mpdstats.h"
#include "mpd-interface/mpdparseutils.h"
#include "settings.h"
#include "support/utils.h"
#include "support/touchproxystyle.h"
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "models/mpdlibrarymodel.h"
#include "librarypage.h"
#include "folderpage.h"
#include "streams/streamdialog.h"
#include "searchpage.h"
#include "customactions.h"
#include "support/gtkstyle.h"
#include "widgets/mirrormenu.h"
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
#include "online/onlineservicespage.h"
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
#include "support/monoicon.h"
#ifdef QT_QTDBUS_FOUND
#include "dbus/mpris.h"
#include "cantataadaptor.h"
#ifdef Q_OS_LINUX
#include "dbus/powermanagement.h"
#endif
#endif
#if !defined Q_OS_WIN && !defined Q_OS_MAC
#include "devices/mountpoints.h"
#endif
#ifdef Q_OS_MAC
#include "support/windowmanager.h"
#include "support/osxstyle.h"
#include "mac/dockmenu.h"
#ifdef IOKIT_FOUND
#include "mac/powermanagement.h"
#endif
#endif
#include "dynamic/dynamic.h"
#include "support/messagewidget.h"
#include "widgets/groupedview.h"
#include "widgets/actionitemdelegate.h"
#include "widgets/icons.h"
#include "widgets/volumeslider.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "stdactions.h"
#ifdef ENABLE_HTTP_STREAM_PLAYBACK
#include "mpd-interface/httpstream.h"
#endif
#include <QSet>
#include <QString>
#include <QTimer>
#include <QToolBar>
#if QT_VERSION >= 0x050000
#include <QProcess>
#ifdef Q_OS_WIN
#include "windows/thumbnailtoolbar.h"
#endif
#endif
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QSpacerItem>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KApplication>
#include <KDE/KStandardAction>
#include <KDE/KMenuBar>
#include <KDE/KMenu>
#include <KDE/KShortcutsDialog>
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

static const char *constRatingKey="rating";

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
    #if defined Q_OS_WIN && QT_VERSION >= 0x050000
    , thumbnailTooolbar(0)
    #endif
{
    #if defined Q_OS_MAC || defined Q_OS_WIN || QT_VERSION<0x050000
    init();
    #else
    if ("qt5ct"==qgetenv("QT_QPA_PLATFORMTHEME")) {
        // Work-aroud Qt5Ct colour scheme issues. Qt5Ct applies its palette after Cantata's main window
        // is shown. Issue 944
        // TODO: The real fix would be for Cantata to properly repect colour scheme changes
        show();
        resize(0, 0);
        QTimer::singleShot(5, this, SLOT(init()));
    }
    else {
        init();
    }
    #endif
}

void MainWindow::init()
{
    #if !defined Q_OS_MAC && !defined Q_OS_WIN && QT_VERSION>=0x050000
    // Work-aroud Qt5Ct incorrectly setting icon theme
    if (!Settings::self()->useStandardIcons()) {
        QIcon::setThemeSearchPaths(QStringList() << CANTATA_SYS_ICONS_DIR << QIcon::themeSearchPaths());
        QIcon::setThemeName(QLatin1String("cantata"));
    }
    #endif

    QPoint p=pos();
    ActionCollection::setMainWidget(this);
    trayItem=new TrayItem(this);
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
    Song::setComposerGenres(Settings::self()->composerGenres());

    int hSpace=Utils::layoutSpacing(this);
    int vSpace=fontMetrics().height()<14 ? hSpace/2 : 0;
    toolbarLayout->setContentsMargins(hSpace, vSpace, hSpace, vSpace);
    toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    #ifdef Q_OS_MAC
    setUnifiedTitleAndToolBarOnMac(true);
    QToolBar *topToolBar = addToolBar("ToolBar");
    WindowManager *wm=new WindowManager(topToolBar);
    wm->initialize(WindowManager::WM_DRAG_MENU_AND_TOOLBAR);
    wm->registerWidgetAndChildren(topToolBar);
    topToolBar->setObjectName("MainToolBar");
    topToolBar->addWidget(toolbar);
    topToolBar->setMovable(false);
    topToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    topToolBar->ensurePolished();
    toolbar=topToolBar;
    #elif !defined Q_OS_WIN
    if (style()->inherits("Kvantum::Style")) {
        // Create a real toolbar so that window can be moved by this - if set in CSS
        QToolBar *topToolBar = addToolBar("ToolBar");
        topToolBar->setObjectName("MainToolBar");
        topToolBar->addWidget(toolbar);
        topToolBar->setMovable(false);
        topToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
        topToolBar->ensurePolished();
        toolbar=topToolBar;
    } else {
        toolbar->setMinimumHeight(fontMetrics().height()*3.5);
    }
    #endif

    GtkStyle::applyTheme(this);

    UNITY_MENU_ICON_CHECK

    toolbar->ensurePolished();
    toolbar->adjustSize();
    coverWidget->setSize(toolbar->height());
    nowPlaying->ensurePolished();
    nowPlaying->adjustSize();
    nowPlaying->setFixedHeight(nowPlaying->height());
    volumeSlider->setColor(nowPlaying->textColor());
    Icons::self()->initToolbarIcons(nowPlaying->textColor());
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
    QColor iconCol=Utils::monoIconColor();

    prefAction=ActionCollection::get()->createAction("configure", Utils::KDE==Utils::currentDe() ? i18n("Configure Cantata...") : i18n("Preferences"),
                                                     HIDE_MENU_ICON(Icons::self()->configureIcon));
    connect(prefAction, SIGNAL(triggered()),this, SLOT(showPreferencesDialog()));
    quitAction = ActionCollection::get()->createAction("quit", i18n("Quit"), HIDE_MENU_ICON(MonoIcon::icon(FontAwesome::poweroff, MonoIcon::constRed, MonoIcon::constRed)));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(quit()));
    quitAction->setShortcut(QKeySequence::Quit);
    Action *aboutAction=ActionCollection::get()->createAction("about", i18nc("Qt-only", "About Cantata..."), HIDE_MENU_ICON(Icons::self()->appIcon));
    connect(aboutAction, SIGNAL(triggered()),this, SLOT(showAboutDialog()));
    #ifdef Q_OS_MAC
    prefAction->setMenuRole(QAction::PreferencesRole);
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction->setMenuRole(QAction::AboutRole);
    #endif
    #endif // ENABLE_KDE_SUPPORT
    restoreAction = new Action(i18n("Show Window"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(restoreWindow()));

    serverInfoAction=ActionCollection::get()->createAction("mpdinfo", i18n("Server information..."), HIDE_MENU_ICON(MonoIcon::icon(FontAwesome::server, iconCol)));
    connect(serverInfoAction, SIGNAL(triggered()),this, SLOT(showServerInfo()));
    serverInfoAction->setEnabled(Settings::self()->firstRun());
    refreshDbAction = ActionCollection::get()->createAction("refresh", i18n("Refresh Database"), HIDE_MENU_ICON(Icons::self()->refreshIcon));
    doDbRefreshAction = new Action(refreshDbAction->icon(), i18n("Refresh"), this);
    refreshDbAction->setEnabled(false);
    connectAction = new Action(Icons::self()->connectIcon, i18n("Connect"), this);
    connectionsAction = new Action(HIDE_MENU_ICON(MonoIcon::icon(FontAwesome::server, iconCol)), i18n("Collection"), this);
    outputsAction = new Action(HIDE_MENU_ICON(MonoIcon::icon(FontAwesome::volumeup, iconCol)), i18n("Outputs"), this);
    stopAfterTrackAction = ActionCollection::get()->createAction("stopaftertrack", i18n("Stop After Track"), Icons::self()->toolbarStopIcon);
    fwdAction = new Action(this);
    revAction = new Action(this);
    fwdAction->setShortcut((Qt::RightToLeft==layoutDirection() ? Qt::Key_Left : Qt::Key_Right)+Qt::ControlModifier);
    revAction->setShortcut((Qt::RightToLeft==layoutDirection() ? Qt::Key_Right : Qt::Key_Left)+Qt::ControlModifier);
    connect(fwdAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(forward()));
    connect(revAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(reverse()));
    addAction(fwdAction);
    addAction(revAction);

    addPlayQueueToStoredPlaylistAction = new Action(HIDE_MENU_ICON(Icons::self()->playlistListIcon), i18n("Add To Stored Playlist"), this);
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = new Action(HIDE_MENU_ICON(StdActions::self()->copyToDeviceAction->icon()), Utils::strippedText(StdActions::self()->copyToDeviceAction->text()), this);
    copyToDeviceAction->setMenu(DevicesModel::self()->menu()->duplicate(0));
    #endif
    cropPlayQueueAction = ActionCollection::get()->createAction("cropplaylist", i18n("Crop Others"));
    addStreamToPlayQueueAction = ActionCollection::get()->createAction("addstreamtoplayqueue", i18n("Add Stream URL"));
    clearPlayQueueAction = ActionCollection::get()->createAction("clearplaylist", i18n("Clear"), HIDE_MENU_ICON(Icons::self()->removeIcon));
    clearPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_K);
    centerPlayQueueAction = ActionCollection::get()->createAction("centerplaylist", i18n("Center On Current Track"), Icons::self()->centrePlayQueueOnTrackIcon);
    expandInterfaceAction = ActionCollection::get()->createAction("expandinterface", i18n("Expanded Interface"), HIDE_MENU_ICON(MonoIcon::icon(FontAwesome::expand, iconCol)));
    expandInterfaceAction->setCheckable(true);
    songInfoAction = ActionCollection::get()->createAction("showsonginfo", i18n("Show Current Song Information"), Icons::self()->infoIcon);
    songInfoAction->setShortcut(Qt::Key_F12);
    songInfoAction->setCheckable(true);
    fullScreenAction = ActionCollection::get()->createAction("fullScreen", i18n("Full Screen"), HIDE_MENU_ICON(MonoIcon::icon(FontAwesome::arrowsalt, iconCol)));
    #ifndef Q_OS_MAC
    fullScreenAction->setShortcut(Qt::Key_F11);
    #endif
    randomPlayQueueAction = ActionCollection::get()->createAction("randomplaylist", i18n("Random"), Icons::self()->shuffleIcon);
    repeatPlayQueueAction = ActionCollection::get()->createAction("repeatplaylist", i18n("Repeat"), Icons::self()->repeatIcon);
    singlePlayQueueAction = ActionCollection::get()->createAction("singleplaylist", i18n("Single"), Icons::self()->singleIcon, i18n("When 'Single' is activated, playback is stopped after current song, or song is repeated if 'Repeat' is enabled."));
    consumePlayQueueAction = ActionCollection::get()->createAction("consumeplaylist", i18n("Consume"), Icons::self()->consumeIcon, i18n("When consume is activated, a song is removed from the play queue after it has been played."));
    searchPlayQueueAction = ActionCollection::get()->createAction("searchplaylist", i18n("Find in Play Queue"), HIDE_MENU_ICON(Icons::self()->searchIcon));
    addAction(searchPlayQueueAction);
    searchPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_F);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction = ActionCollection::get()->createAction("streamplay", i18n("Play Stream"), HIDE_MENU_ICON(Icons::self()->httpStreamIcon));
    streamPlayAction->setCheckable(true);
    streamPlayAction->setChecked(false);
    streamPlayAction->setVisible(false);
    streamPlayButton->setDefaultAction(streamPlayAction);
    #endif
    streamPlayButton->setVisible(false);

    locateTrackAction = ActionCollection::get()->createAction("locatetrack", i18n("Locate In Library"), Icons::self()->searchIcon);
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction = ActionCollection::get()->createAction("editpqtags", Utils::strippedText(StdActions::self()->editTagsAction->text()), StdActions::self()->editTagsAction->icon());
    #endif
    addAction(expandAllAction = ActionCollection::get()->createAction("expandall", i18n("Expand All")));
    expandAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Plus);
    addAction(collapseAllAction = ActionCollection::get()->createAction("collapseall", i18n("Collapse All")));
    collapseAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Minus);
    cancelAction = ActionCollection::get()->createAction("cancel", i18n("Cancel"), Icons::self()->cancelIcon);
    cancelAction->setShortcut(Qt::AltModifier+Qt::Key_Escape);
    connect(cancelAction, SIGNAL(triggered()), messageWidget, SLOT(animatedHide()));

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
    addPlayQueueToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu()->duplicate(0));

    playPauseTrackButton->setDefaultAction(StdActions::self()->playPauseTrackAction);
    stopTrackButton->setDefaultAction(StdActions::self()->stopPlaybackAction);
    nextTrackButton->setDefaultAction(StdActions::self()->nextTrackAction);
    prevTrackButton->setDefaultAction(StdActions::self()->prevTrackAction);

    QMenu *stopMenu=new QMenu(this);
    stopMenu->addAction(StdActions::self()->stopPlaybackAction);
    stopMenu->addAction(StdActions::self()->stopAfterCurrentTrackAction);
    stopTrackButton->setMenu(stopMenu);
    stopTrackButton->setPopupMode(QToolButton::DelayedPopup);

    clearPlayQueueAction->setEnabled(false);
    centerPlayQueueAction->setEnabled(false);
    StdActions::self()->savePlayQueueAction->setEnabled(false);
    addStreamToPlayQueueAction->setEnabled(false);
    clearPlayQueueButton->setDefaultAction(clearPlayQueueAction);
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
    bool contextInSidebar=!hiddenPages.contains(contextPage->metaObject()->className());
    layout=new QBoxLayout(QBoxLayout::TopToBottom, contextPage);
    layout->setContentsMargins(0, 0, 0, 0);

    // Build sidebar...
    #define TAB_ACTION(A) A->icon(), A->text(), A->text()
    int sidebarPageShortcutKey=Qt::Key_1;
    addAction(showPlayQueueAction = ActionCollection::get()->createAction("showplayqueue", i18n("Play Queue"), Icons::self()->playqueueIcon));
    showPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_Q);
    tabWidget->addTab(playQueuePage, TAB_ACTION(showPlayQueueAction), playQueueInSidebar);
    connect(showPlayQueueAction, SIGNAL(triggered()), this, SLOT(showPlayQueue()));
    libraryPage = new LibraryPage(this);
    addAction(libraryTabAction = ActionCollection::get()->createAction("showlibrarytab", i18n("Library"), Icons::self()->libraryIcon));
    libraryTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(libraryPage, TAB_ACTION(libraryTabAction), !hiddenPages.contains(libraryPage->metaObject()->className()));
    connect(libraryTabAction, SIGNAL(triggered()), this, SLOT(showLibraryTab()));
    folderPage = new FolderPage(this);
    addAction(foldersTabAction = ActionCollection::get()->createAction("showfolderstab", i18n("Folders"), Icons::self()->foldersIcon));
    foldersTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(folderPage, TAB_ACTION(foldersTabAction), !hiddenPages.contains(folderPage->metaObject()->className()));
    connect(foldersTabAction, SIGNAL(triggered()), this, SLOT(showFoldersTab()));
    folderPage->setEnabled(!hiddenPages.contains(folderPage->metaObject()->className()));
    playlistsPage = new PlaylistsPage(this);
    addAction(playlistsTabAction = ActionCollection::get()->createAction("showplayliststab", i18n("Playlists"), Icons::self()->playlistsIcon));
    playlistsTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(playlistsPage, TAB_ACTION(playlistsTabAction), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    connect(playlistsTabAction, SIGNAL(triggered()), this, SLOT(showPlaylistsTab()));
    connect(Dynamic::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(Dynamic::self(), SIGNAL(running(bool)), dynamicLabel, SLOT(setVisible(bool)));
    connect(Dynamic::self(), SIGNAL(running(bool)), this, SLOT(controlDynamicButton()));
    stopDynamicButton->setDefaultAction(Dynamic::self()->stopAct());
    onlinePage = new OnlineServicesPage(this);
    addAction(onlineTabAction = ActionCollection::get()->createAction("showonlinetab", i18n("Internet"), Icons::self()->onlineIcon));
    onlineTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(onlinePage, TAB_ACTION(onlineTabAction), !hiddenPages.contains(onlinePage->metaObject()->className()));
    onlinePage->setEnabled(!hiddenPages.contains(onlinePage->metaObject()->className()));
    connect(onlineTabAction, SIGNAL(triggered()), this, SLOT(showOnlineTab()));
//    connect(onlinePage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(onlinePage, SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage = new DevicesPage(this);
    addAction(devicesTabAction = ActionCollection::get()->createAction("showdevicestab", i18n("Devices"), Icons::self()->devicesIcon));
    devicesTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(devicesPage, TAB_ACTION(devicesTabAction), !hiddenPages.contains(devicesPage->metaObject()->className()));
    DevicesModel::self()->setEnabled(!hiddenPages.contains(devicesPage->metaObject()->className()));
    connect(devicesTabAction, SIGNAL(triggered()), this, SLOT(showDevicesTab()));
    #endif
    searchPage = new SearchPage(this);
    addAction(searchTabAction = ActionCollection::get()->createAction("showsearchtab", i18n("Search"), Icons::self()->searchTabIcon));
    searchTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    connect(searchTabAction, SIGNAL(triggered()), this, SLOT(showSearchTab()));
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

    expandInterfaceAction->setChecked(Settings::self()->showPlaylist());
    fullScreenAction->setEnabled(expandInterfaceAction->isChecked());
    if (fullScreenAction->isEnabled()) {
        fullScreenAction->setChecked(Settings::self()->showFullScreen());
    }
    randomPlayQueueAction->setCheckable(true);
    repeatPlayQueueAction->setCheckable(true);
    singlePlayQueueAction->setCheckable(true);
    consumePlayQueueAction->setCheckable(true);

    songInfoButton->setDefaultAction(songInfoAction);
    fullScreenLabel->setVisible(false);
    connect(fullScreenLabel, SIGNAL(leftClickedUrl()), fullScreenAction, SIGNAL(triggered()));
    connect(playQueueSearchWidget, SIGNAL(active(bool)), playQueue, SLOT(searchActive(bool)));
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
    } else {
        playbackIconSize=24==Icons::self()->toolbarPlayIcon.actualSize(QSize(24, 24)).width() ? 24 : 28;
    }
    #ifdef USE_SYSTEM_MENU_ICON
    controlIconSize=22==controlIconSize ? 16 : 32==controlIconSize ? 22 : 32;
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

    #ifdef UNITY_MENU_HACK
    if (!menuIcons) {
        // These buttons have actions that are in the menubar, as wall as in the main window.
        // Under unity we dont want any icons in menus - so the actions themselves have no icons.
        // But the toolbuttuns need icons!
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        streamPlayButton->setIcon(Icons::self()->httpStreamIcon);
        #endif
        savePlayQueueButton->setIcon(Icons::self()->savePlayQueueIcon);
        clearPlayQueueButton->setIcon(Icons::self()->removeIcon);
    }
    #endif
    expandedSize=Settings::self()->mainWindowSize();
    collapsedSize=Settings::self()->mainWindowCollapsedSize();

    #ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys);
    #endif

    if (Settings::self()->firstRun() || (expandInterfaceAction->isChecked() && expandedSize.isEmpty())) {
        int width=playPauseTrackButton->width()*25;
        resize(playPauseTrackButton->width()*25, playPauseTrackButton->height()*18);
        splitter->setSizes(QList<int>() << width*0.4 << width*0.6);
    } else {
        if (expandInterfaceAction->isChecked()) {
            if (!expandedSize.isEmpty()) {
                if (expandedSize.width()>1) {
                    resize(expandedSize);
                    expandOrCollapse(false);
                } else {
                    showMaximized();
                    expandedSize=size();
                }
            }
        } else {
            if (!collapsedSize.isEmpty() && collapsedSize.width()>1) {
                resize(collapsedSize);
                expandOrCollapse(false);
            }
        }

        if (!playQueueInSidebar) {
            QByteArray state=Settings::self()->splitterState();
            if (state.isEmpty()) {
                int width=playPauseTrackButton->width()*25;
                splitter->setSizes(QList<int>() << width*0.4 << width*0.6);
            } else {
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
        mainMenu->addAction(expandInterfaceAction);
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
        mainMenu->addAction(searchPlayQueueAction);
        mainMenu->addSeparator();
        mainMenu->addAction(serverInfoAction);
        mainMenu->addMenu(helpMenu());
        #else
        mainMenu->addAction(prefAction);
        mainMenu->addAction(refreshDbAction);
        mainMenu->addSeparator();
        mainMenu->addAction(StdActions::self()->searchAction);
        mainMenu->addAction(searchPlayQueueAction);
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
        addMenuAction(menu, PlayQueueModel::self()->undoAct());
        addMenuAction(menu, PlayQueueModel::self()->redoAct());
        menu->addSeparator();
        addMenuAction(menu, StdActions::self()->searchAction);
        addMenuAction(menu, searchPlayQueueAction);
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
            addMenuAction(menu, expandInterfaceAction);
            addMenuAction(menu, fullScreenAction);
            menuBar()->addMenu(menu);
        }
        #endif
        menu=new QMenu(i18n("&Queue"), this);
        addMenuAction(menu, clearPlayQueueAction);
        addMenuAction(menu, StdActions::self()->savePlayQueueAction);
        addMenuAction(menu, addStreamToPlayQueueAction);
        menu->addSeparator();
        addMenuAction(menu, PlayQueueModel::self()->shuffleAct());
        addMenuAction(menu, PlayQueueModel::self()->sortAct());
        menuBar()->addMenu(menu);
        #ifndef ENABLE_KDE_SUPPORT
        if (Utils::KDE==Utils::currentDe())
        #endif
        {
            menu=new QMenu(i18n("&Settings"), this);
            if (showMenuAction) {
                addMenuAction(menu, showMenuAction);
            }
            addMenuAction(menu, expandInterfaceAction);
            addMenuAction(menu, fullScreenAction);
            addMenuAction(menu, songInfoAction);
            menu->addSeparator();
            #ifdef ENABLE_KDE_SUPPORT
            addMenuAction(menu, shortcutsAction);
            #endif
            addMenuAction(menu, prefAction);
            menuBar()->addMenu(menu);
        }
        #ifdef Q_OS_MAC
        OSXStyle::self()->initWindowMenu(this);
        #endif
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
    }

    if (showMenuAction) {
        showMenuAction->setChecked(Settings::self()->showMenubar());
        toggleMenubar();
    }

    dynamicLabel->setVisible(false);
    stopDynamicButton->setVisible(false);
    StdActions::self()->addWithPriorityAction->setVisible(false);
    StdActions::self()->setPriorityAction->setVisible(false);

    playQueueProxyModel.setSourceModel(PlayQueueModel::self());
    playQueue->setModel(&playQueueProxyModel);
    playQueue->addAction(playQueue->removeFromAct());
    ratingAction=new Action(i18n("Set Rating"), this);
    ratingAction->setMenu(new QMenu(0));
    for (int i=0; i<((Song::Rating_Max/Song::Rating_Step)+1); ++i) {
        QString text;
        if (0==i) {
            text=i18n("No Rating");
        } else {
            for (int s=0; s<i; ++s) {
                text+=QChar(0x2605);
            }
        }
        Action *action=ActionCollection::get()->createAction(QLatin1String("rating")+QString::number(i), text);
        action->setProperty(constRatingKey, i*Song::Rating_Step);
        action->setShortcut(Qt::AltModifier+Qt::Key_0+i);
        ratingAction->menu()->addAction(action);
        connect(action, SIGNAL(triggered()), SLOT(setRating()));
    }
    playQueue->addAction(ratingAction);
    playQueue->addAction(StdActions::self()->setPriorityAction);
    playQueue->addAction(stopAfterTrackAction);
    playQueue->addAction(locateTrackAction);
    #ifdef TAGLIB_FOUND
    playQueue->addAction(editPlayQueueTagsAction);
    #endif
    Action *sep=new Action(this);
    sep->setSeparator(true);
    playQueue->addAction(sep);
    playQueue->addAction(PlayQueueModel::self()->removeDuplicatesAct());
    playQueue->addAction(clearPlayQueueAction);
    playQueue->addAction(cropPlayQueueAction);
    playQueue->addAction(StdActions::self()->savePlayQueueAction);
    playQueue->addAction(addStreamToPlayQueueAction);
    playQueue->addAction(addPlayQueueToStoredPlaylistAction);
    #ifdef ENABLE_DEVICES_SUPPORT
    playQueue->addAction(copyToDeviceAction);
    #endif
    playQueue->addAction(PlayQueueModel::self()->shuffleAct());
    playQueue->addAction(PlayQueueModel::self()->sortAct());
    playQueue->addAction(PlayQueueModel::self()->undoAct());
    playQueue->addAction(PlayQueueModel::self()->redoAct());
    playQueue->readConfig();

    #ifdef ENABLE_DEVICES_SUPPORT
    connect(DevicesModel::self(), SIGNAL(addToDevice(const QString &)), this, SLOT(addToDevice(const QString &)));
    connect(DevicesModel::self(), SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    connect(libraryPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(playlistsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(searchPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(StdActions::self()->deleteSongsAction, SIGNAL(triggered()), SLOT(deleteSongs()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    #endif
    connect(StdActions::self()->prioHighAction, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    connect(StdActions::self()->prioMediumAction, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    connect(StdActions::self()->prioLowAction, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    connect(StdActions::self()->prioDefaultAction, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    connect(StdActions::self()->prioCustomAction, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    connect(StdActions::self()->appendToPlayQueueAndPlayAction, SIGNAL(triggered()), this, SLOT(appendToPlayQueueAndPlay()));
    connect(StdActions::self()->addToPlayQueueAndPlayAction, SIGNAL(triggered()), this, SLOT(addToPlayQueueAndPlay()));
    connect(StdActions::self()->insertAfterCurrentAction, SIGNAL(triggered()), this, SLOT(insertIntoPlayQueue()));
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
    connect(PlayQueueModel::self(), SIGNAL(statsUpdated(int, quint32)), this, SLOT(updatePlayQueueStats(int, quint32)));
    connect(PlayQueueModel::self(), SIGNAL(fetchingStreams()), playQueue, SLOT(showSpinner()));
    connect(PlayQueueModel::self(), SIGNAL(streamsFetched()), playQueue, SLOT(hideSpinner()));
    connect(PlayQueueModel::self(), SIGNAL(updateCurrent(Song)), SLOT(updateCurrentSong(Song)));
    connect(PlayQueueModel::self(), SIGNAL(streamFetchStatus(QString)), playQueue, SLOT(streamFetchStatus(QString)));
    connect(playQueue, SIGNAL(cancelStreamFetch()), PlayQueueModel::self(), SLOT(cancelStreamFetch()));
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playQueueItemsSelected(bool)));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &, bool)), this, SLOT(updatePlayQueue(const QList<Song> &, bool)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(Song)), this, SLOT(updateCurrentSong(Song)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
    connect(MPDConnection::self(), SIGNAL(info(const QString &)), SLOT(showInformation(const QString &)));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(checkMpdDir()));
    connect(MPDConnection::self(), SIGNAL(connectionNotChanged(QString)), SLOT(mpdConnectionName(QString)));
    connect(MpdLibraryModel::self(), SIGNAL(error(QString)), SLOT(showError(QString)));
    connect(refreshDbAction, SIGNAL(triggered()), this, SLOT(refreshDbPromp()));
    connect(doDbRefreshAction, SIGNAL(triggered()), MpdLibraryModel::self(), SLOT(clearDb()));
    connect(doDbRefreshAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(update()));
    connect(doDbRefreshAction, SIGNAL(triggered()), messageWidget, SLOT(animatedHide()));
    connect(connectAction, SIGNAL(triggered()), this, SLOT(connectToMpd()));
    connect(StdActions::self()->prevTrackAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(goToPrevious()));
    connect(StdActions::self()->nextTrackAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(goToNext()));
    connect(StdActions::self()->playPauseTrackAction, SIGNAL(triggered()), this, SLOT(playPauseTrack()));
    connect(StdActions::self()->stopPlaybackAction, SIGNAL(triggered()), this, SLOT(stopPlayback()));
    connect(StdActions::self()->stopAfterCurrentTrackAction, SIGNAL(triggered()), this, SLOT(stopAfterCurrentTrack()));
    connect(stopAfterTrackAction, SIGNAL(triggered()), this, SLOT(stopAfterTrack()));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(nowPlaying, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(PlayQueueModel::self(), SIGNAL(currentSongRating(QString,quint8)), nowPlaying, SLOT(rating(QString,quint8)));
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
    connect(StdActions::self()->removeAction, SIGNAL(triggered()), this, SLOT(removeItems()));
    connect(StdActions::self()->appendToPlayQueueAction, SIGNAL(triggered()), this, SLOT(appendToPlayQueue()));
    connect(StdActions::self()->replacePlayQueueAction, SIGNAL(triggered()), this, SLOT(replacePlayQueue()));
    connect(playQueue->removeFromAct(), SIGNAL(triggered()), this, SLOT(removeFromPlayQueue()));
    connect(clearPlayQueueAction, SIGNAL(triggered()), playQueueSearchWidget, SLOT(clear()));
    connect(clearPlayQueueAction, SIGNAL(triggered()), this, SLOT(clearPlayQueue()));
    connect(centerPlayQueueAction, SIGNAL(triggered()), this, SLOT(centerPlayQueue()));
    connect(cropPlayQueueAction, SIGNAL(triggered()), this, SLOT(cropPlayQueue()));
    connect(songInfoAction, SIGNAL(triggered()), this, SLOT(showSongInfo()));
    connect(expandInterfaceAction, SIGNAL(triggered()), this, SLOT(expandOrCollapse()));
    connect(fullScreenAction, SIGNAL(triggered()), this, SLOT(fullScreen()));
    #ifdef TAGLIB_FOUND
    connect(StdActions::self()->editTagsAction, SIGNAL(triggered()), this, SLOT(editTags()));
    connect(editPlayQueueTagsAction, SIGNAL(triggered()), this, SLOT(editTags()));
    connect(StdActions::self()->organiseFilesAction, SIGNAL(triggered()), SLOT(organiseFiles()));
    #endif
    connect(context, SIGNAL(findArtist(QString)), this, SLOT(locateArtist(QString)));
    connect(context, SIGNAL(findAlbum(QString,QString)), this, SLOT(locateAlbum(QString,QString)));
    connect(context, SIGNAL(playSong(QString)), PlayQueueModel::self(), SLOT(playSong(QString)));
    connect(locateTrackAction, SIGNAL(triggered()), this, SLOT(locateTrack()));
    connect(StdActions::self()->searchAction, SIGNAL(triggered()), SLOT(showSearch()));
    connect(searchPlayQueueAction, SIGNAL(triggered()), this, SLOT(showPlayQueueSearch()));
    connect(playQueue, SIGNAL(focusSearch(QString)), playQueueSearchWidget, SLOT(activate(QString)));
    connect(expandAllAction, SIGNAL(triggered()), this, SLOT(expandAll()));
    connect(collapseAllAction, SIGNAL(triggered()), this, SLOT(collapseAll()));
    connect(addStreamToPlayQueueAction, SIGNAL(triggered()), this, SLOT(addStreamToPlayQueue()));
    connect(StdActions::self()->setCoverAction, SIGNAL(triggered()), SLOT(setCover()));
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    connect(StdActions::self()->replaygainAction, SIGNAL(triggered()), SLOT(replayGain()));
    #endif
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
// TODO: Why is this here???
//    connect(playlistsPage, SIGNAL(add(const QStringList &, bool, quint8)), PlayQueueModel::self(), SLOT(addItems(const QStringList &, bool, quint8)));
    connect(coverWidget, SIGNAL(clicked()), expandInterfaceAction, SLOT(trigger()));
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
    connect(tabWidget, SIGNAL(styleChanged(int)), this, SLOT(sidebarModeChanged()));
    connect(tabWidget, SIGNAL(configRequested()), this, SLOT(showSidebarPreferencesPage()));

    readSettings();
    updateConnectionsMenu();
    ActionCollection::get()->readSettings();

    if (testAttribute(Qt::WA_TranslucentBackground)) {
        // BUG: 146 - Work-around non-showing main window on start-up with transparent QtCurve windows.
        move(p.isNull() ? QPoint(96, 96) : p);
    }

    currentTabChanged(tabWidget->currentIndex());

    if (Settings::self()->firstRun() && MPDConnection::self()->isConnected()) {
        mpdConnectionStateChanged(true);
    }
    #ifndef ENABLE_KDE_SUPPORT
    MediaKeys::self()->start();
    #endif
    #ifdef Q_OS_MAC
    dockMenu=new DockMenu(this);
    #endif
    updateActionToolTips();
    CustomActions::self()->setMainWindow(this);

    if (Settings::self()->startHidden()) {
        hide();
    } else {
        show();
    }
}

MainWindow::~MainWindow()
{
    if (showMenuAction) {
        Settings::self()->saveShowMenubar(showMenuAction->isChecked());
    }
    bool hadCantataStreams=PlayQueueModel::self()->removeCantataStreams();
    Settings::self()->saveShowFullScreen(fullScreenAction->isChecked());
    if (!fullScreenAction->isChecked()) {
        if (expandInterfaceAction->isChecked()) {
            Settings::self()->saveMainWindowSize(isMaximized() ? QSize(1, 1) : size());
        } else {
            Settings::self()->saveMainWindowSize(expandedSize);
        }
        Settings::self()->saveMainWindowCollapsedSize(expandInterfaceAction->isChecked() ? collapsedSize : size());
        Settings::self()->saveShowPlaylist(expandInterfaceAction->isChecked());
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    Settings::self()->savePlayStream(streamPlayAction->isVisible() && streamPlayAction->isChecked());
    #endif
    if (!fullScreenAction->isChecked()) {
        if (!tabWidget->isEnabled(PAGE_PLAYQUEUE)) {
            Settings::self()->saveSplitterState(splitter->saveState());
        }
    }
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    playQueue->saveConfig();
//    playlistsPage->saveConfig();
    context->saveConfig();
    StreamsModel::self()->save();
    nowPlaying->saveConfig();
    Settings::self()->saveForceSingleClick(TreeView::getForceSingleClick());
    Settings::StartupState startupState=Settings::self()->startupState();
    Settings::self()->saveStartHidden(trayItem->isActive() && Settings::self()->minimiseOnClose() &&
                                      ((isHidden() && Settings::SS_ShowMainWindow!=startupState) || (Settings::SS_HideMainWindow==startupState)));
    Settings::self()->save();
    disconnect(MPDConnection::self(), 0, 0, 0);
    if (Settings::self()->stopOnExit()) {
        Dynamic::self()->stop();
    }
    if (Settings::self()->stopOnExit()) {
        emit stop();
        Utils::sleep(); // Allow time for stop to be sent...
    } else if (hadCantataStreams) {
        Utils::sleep(); // Allow time for removal of cantata streams...
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->stop();
    #if defined CDDB_FOUND || defined MUSICBRAINZ5_FOUND
    Covers::self()->cleanCdda();
    #endif
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

QList<Song> MainWindow::selectedSongs() const
{
    return currentPage ? currentPage->selectedSongs() : QList<Song>();
}

void MainWindow::addMenuAction(QMenu *menu, QAction *action)
{
    menu->addAction(action);
    addAction(action); // Bind action to window, so that it works when fullscreen!
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
    if (!message.isEmpty()) {
        expand();
    }
    QApplication::alert(this);
}

void MainWindow::showInformation(const QString &message)
{
    if (!messageWidget->showingError()) {
        messageWidget->setInformation(message);
        messageWidget->removeAllActions();
    }
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    serverInfoAction->setEnabled(connected && !MPDConnection::self()->isMopidy());
    refreshDbAction->setEnabled(connected);
    addStreamToPlayQueueAction->setEnabled(connected);
    if (connected) {
        if (!messageWidget->showingError()) {
            messageWidget->hide();
        }
        if (CS_Connected!=connectedState) {
            emit playListInfo();
            emit outputs();
            if (CS_Init!=connectedState) {
                currentTabChanged(tabWidget->currentIndex());
            }
            connectedState=CS_Connected;
            StdActions::self()->addWithPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
            StdActions::self()->setPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
        }
    } else {
        libraryPage->clear();
        folderPage->clear();
        PlaylistsModel::self()->clear();
        PlayQueueModel::self()->clear();
        searchPage->clear();
        connectedState=CS_Disconnected;
        outputsAction->setVisible(false);
        MPDStatus dummyStatus;
        updateStatus(&dummyStatus);
    }
    updateWindowTitle();
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

#if defined Q_OS_WIN && QT_VERSION >= 0x050000
void MainWindow::showEvent(QShowEvent *event)
{
    if (!thumbnailTooolbar) {
        thumbnailTooolbar=new ThumbnailToolBar(this);
    }
    MAIN_WINDOW_BASE_CLASS::showEvent(event);
}
#endif

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayItem->isActive() && Settings::self()->minimiseOnClose()) {
        lastPos=pos();
        hide();
        if (event->spontaneous()) {
            event->ignore();
        }
    } else if (canClose()) {
        MAIN_WINDOW_BASE_CLASS::closeEvent(event);
    }
}

void MainWindow::playQueueItemsSelected(bool s)
{
    int rc=playQueue->model() ? playQueue->model()->rowCount() : 0;
    bool haveItems=rc>0;
    bool singleSelection=1==playQueue->selectedIndexes(false).count(); // Dont need sorted selection here...
    playQueue->removeFromAct()->setEnabled(s && haveItems);
    StdActions::self()->setPriorityAction->setEnabled(s && haveItems);
    locateTrackAction->setEnabled(singleSelection);
    cropPlayQueueAction->setEnabled(playQueue->haveUnSelectedItems() && haveItems);
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction->setEnabled(s && haveItems && MPDConnection::self()->getDetails().dirReadable);
    #endif
    addPlayQueueToStoredPlaylistAction->setEnabled(haveItems);
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction->setEnabled(editPlayQueueTagsAction->isEnabled());
    #endif
    stopAfterTrackAction->setEnabled(singleSelection);
    ratingAction->setEnabled(s && haveItems);
}

void MainWindow::connectToMpd(const MPDConnectionDetails &details)
{
    if (!MPDConnection::self()->isConnected() || details!=MPDConnection::self()->getDetails()) {
        libraryPage->clear();
        folderPage->clear();
        PlaylistsModel::self()->clear();
        PlayQueueModel::self()->clear();
        searchPage->clear();
        if (!MPDConnection::self()->getDetails().isEmpty() && details!=MPDConnection::self()->getDetails()) {
            Dynamic::self()->stop();
        }
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
    messageWidget->hide();
    connectToMpd(Settings::self()->connectionDetails());
}

void MainWindow::streamUrl(const QString &u)
{
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction->setVisible(!u.isEmpty());
    streamPlayAction->setChecked(streamPlayAction->isVisible() && Settings::self()->playStream());
    streamPlayButton->setVisible(!u.isEmpty());
    httpStream->setEnabled(streamPlayAction->isChecked());
    leftSpacer->setVisible(loveTrack->isVisible() || scrobblingStatus->isVisible() || streamPlayButton->isVisible());
    #else
    Q_UNUSED(u)
    #endif
}

void MainWindow::refreshDbPromp()
{
    int btnLayout=style()->styleHint(QStyle::SH_DialogButtonLayout);
    if (QDialogButtonBox::GnomeLayout==btnLayout || QDialogButtonBox::MacLayout==btnLayout) {
        messageWidget->setActions(QList<QAction*>() << cancelAction << doDbRefreshAction);
    } else {
        messageWidget->setActions(QList<QAction*>() << doDbRefreshAction << cancelAction);
    }
    messageWidget->setWarning(i18n("Refresh MPD Database?"), false);
    expand();
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
                             "&copy; 2011-2016 Craig Drummond<br/>Released under the <a href=\"http://www.gnu.org/licenses/gpl.html\">GPLv3</a>", PACKAGE_VERSION_STRING)+
                       QLatin1String("<br/><br/><i><small>")+i18n("Based upon <a href=\"http://lowblog.nl\">QtMPC</a> - &copy; 2007-2010 The QtMPC Authors<br/>")+
                       i18nc("Qt-only", "Context view backdrops courtesy of <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+QLatin1String("<br/>")+
                       i18nc("Qt-only", "Context view metadata courtesy of <a href=\"http://www.wikipedia.org\">Wikipedia</a> and <a href=\"http://www.last.fm\">Last.fm</a>")+
                       QLatin1String("<br/><br/>")+i18n("Please consider uploading your own music fan-art to <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+
                       QLatin1String("</small></i>"));
}
#endif

bool MainWindow::canClose()
{
    if (onlinePage->isDownloading() &&
            MessageBox::No==MessageBox::warningYesNo(this, i18n("A Podcast is currently being downloaded\n\nQuiting now will abort the download."),
                                                     QString(), GuiItem(i18n("Abort download and quit")), GuiItem("Do not quit just yet"))) {
        return false;
    }
    onlinePage->cancelAll();
    return true;
}

void MainWindow::expand()
{
    if (!expandInterfaceAction->isChecked()) {
        expandInterfaceAction->setChecked(true);
        expandOrCollapse();
    }
}

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
    if (!page.isEmpty()) {
        pref->showPage(page);
    }
    pref->show();
}

void MainWindow::quit()
{
    if (!canClose()) {
        return;
    }
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
    if (ActionDialog::instanceCount() || SyncDialog::instanceCount()) {
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
    if (StdActions::self()->editTagsAction->isEnabled()) {
        StdActions::self()->editTagsAction->setEnabled(MPDConnection::self()->getDetails().dirReadable);
    }
    #endif
    if (currentPage) {
        currentPage->controlActions();
    }
}

void MainWindow::outputsUpdated(const QList<Output> &outputs)
{
    const char *constMpdConName="mpd-name";
    const char *constMpdEnabledOuptuts="mpd-outputs";
    QString lastConn=property(constMpdConName).toString();
    QString newConn=MPDConnection::self()->getDetails().name;
    setProperty(constMpdConName, newConn);
    outputsAction->setVisible(true);
    QSet<QString> enabledMpd;
    QSet<QString> lastEnabledMpd=QSet<QString>::fromList(property(constMpdEnabledOuptuts).toStringList());
    QSet<QString> mpd;
    QSet<QString> menuItems;
    QMenu *menu=outputsAction->menu();
    foreach (const Output &o, outputs) {
        if (o.enabled) {
            enabledMpd.insert(o.name);
        }
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
            act->setShortcut(Qt::ControlModifier+Qt::AltModifier+nextKey(i));
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

    if (newConn==lastConn && enabledMpd!=lastEnabledMpd && !menuItems.isEmpty()) {
        QSet<QString> switchedOn=enabledMpd-lastEnabledMpd;
        QSet<QString> switchedOff=lastEnabledMpd-enabledMpd;

        if (!switchedOn.isEmpty() && switchedOff.isEmpty()) {
            QStringList names=switchedOn.toList();
            qSort(names);
            trayItem->showMessage(i18n("Outputs"), i18n("Enabled: %1", names.join(QLatin1String(", "))), Icons::self()->speakerIcon.pixmap(64, 64).toImage());
        } else if (!switchedOff.isEmpty() && switchedOn.isEmpty()) {
            QStringList names=switchedOff.toList();
            qSort(names);
            trayItem->showMessage(i18n("Outputs"), i18n("Disabled: %1", names.join(QLatin1String(", "))), Icons::self()->speakerIcon.pixmap(64, 64).toImage());
        } else if (!switchedOn.isEmpty() && !switchedOff.isEmpty()) {
            QStringList on=switchedOn.toList();
            qSort(on);
            QStringList off=switchedOff.toList();
            qSort(off);
            trayItem->showMessage(i18n("Outputs"),
                                  i18n("Enabled: %1", on.join(QLatin1String(", ")))+QLatin1Char('\n')+
                                  i18n("Disabled: %1", off.join(QLatin1String(", "))), Icons::self()->speakerIcon.pixmap(64, 64).toImage());
        }
    }
    setProperty(constMpdEnabledOuptuts, QStringList() << enabledMpd.toList());
    outputsAction->setVisible(outputs.count()>1);
    trayItem->updateOutputs();
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
    trayItem->updateConnections();
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
    stopDynamicButton->setVisible(Dynamic::self()->isRunning());
    PlayQueueModel::self()->enableUndo(!Dynamic::self()->isRunning());
}

void MainWindow::setRating()
{
    Action *act=qobject_cast<Action *>(sender());
    if (act) {
        PlayQueueModel::self()->setRating(playQueueProxyModel.mapToSourceRows(playQueue->selectedIndexes()), act->property(constRatingKey).toUInt());
    }
}

void MainWindow::initMpris()
{
    #ifdef QT_QTDBUS_FOUND
    if (Settings::self()->mpris()) {
        if (!mpris) {
            mpris=new Mpris(this);
            connect(mpris, SIGNAL(showMainWindow()), this, SLOT(restoreWindow()));
        }
    } else if (mpris) {
        disconnect(mpris, SIGNAL(showMainWindow()), this, SLOT(restoreWindow()));
        mpris->deleteLater();
        mpris=0;
    }
    CurrentCover::self()->setEnabled(mpris || Settings::self()->showPopups() || 0!=Settings::self()->playQueueBackground() || Settings::self()->showCoverWidget());
    #endif
}

void MainWindow::readSettings()
{    
    #ifdef QT_QTDBUS_FOUND
    // It appears as if the KDE MPRIS code does not like the MPRIS interface to be setup before the window is visible.
    // to work-around this, initMpris in the next event loop iteration.
    // See #863
    QTimer::singleShot(0, this, SLOT(initMpris()));
    #else
    CurrentCover::self()->setEnabled(Settings::self()->showPopups() || 0!=Settings::self()->playQueueBackground() || Settings::self()->showCoverWidget());
    #endif

    checkMpdDir();
    Song::setIgnorePrefixes(Settings::self()->ignorePrefixes());
    Covers::self()->readConfig();
    HttpServer::self()->readConfig();
    #ifdef ENABLE_DEVICES_SUPPORT
    StdActions::self()->deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    Song::setComposerGenres(Settings::self()->composerGenres());
    trayItem->setup();
    autoScrollPlayQueue=Settings::self()->playQueueScroll();
    TreeView::setForceSingleClick(Settings::self()->forceSingleClick());
    #if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
    PowerManagement::self()->setInhibitSuspend(Settings::self()->inhibitSuspend());
    #endif
    context->readConfig();
    tabWidget->setHiddenPages(Settings::self()->hiddenPages());
    tabWidget->setStyle(Settings::self()->sidebar());
    coverWidget->setEnabled(Settings::self()->showCoverWidget());
    stopTrackButton->setVisible(Settings::self()->showStopButton());
    #if defined Q_OS_WIN && QT_VERSION >= 0x050000
    if (thumbnailTooolbar) {
        thumbnailTooolbar->readSettings();
    }
    #endif
    searchPlayQueueAction->setEnabled(Settings::self()->playQueueSearch());
    searchPlayQueueAction->setVisible(Settings::self()->playQueueSearch());
    nowPlaying->readConfig();
    setCollapsedSize();
    toggleSplitterAutoHide();
    if (contextSwitchTime!=Settings::self()->contextSwitchTime()) {
        contextSwitchTime=Settings::self()->contextSwitchTime();
        if (0==contextSwitchTime && contextTimer) {
            contextTimer->stop();
            contextTimer->deleteLater();
            contextTimer=0;
        }
    }
    MPDConnection::self()->setVolumeFadeDuration(Settings::self()->stopFadeDuration());
    MPDParseUtils::setSingleTracksFolders(Settings::self()->singleTracksFolders());
    MPDParseUtils::setCueFileSupport(Settings::self()->cueSupport());
    leftSpacer->setVisible(loveTrack->isVisible() || scrobblingStatus->isVisible() || streamPlayButton->isVisible());
}

void MainWindow::updateSettings()
{
    connectToMpd();
    Settings::self()->save();
    readSettings();

    bool wasAutoExpand=playQueue->isAutoExpand();
    bool wasStartClosed=playQueue->isStartClosed();
    bool wasGrouped=playQueue->isGrouped();
    playQueue->readConfig();

    if (wasGrouped!=playQueue->isGrouped() ||
        (playQueue->isGrouped() && (wasAutoExpand!=playQueue->isAutoExpand() || wasStartClosed!=playQueue->isStartClosed())) ) {
        QModelIndex idx=playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(PlayQueueModel::self()->currentSongRow(), 0));
        playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && MPDState_Playing==MPDStatus::self()->state());
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
    QDateTime dbUpdate;
    dbUpdate.setTime_t(MPDStats::self()->dbUpdate());
    MessageBox::information(this, 
                                  #ifdef Q_OS_MAC
                                  i18n("Server Information")+QLatin1String("<br/><br/>")+
                                  #endif
                                  QLatin1String("<p><table>")+
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
                                       Utils::formatDuration(MPDStats::self()->dbPlaytime()), dbUpdate.toString(Qt::SystemLocaleShortDate))+
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
    emit stop();
    enableStopActions(false);
    StdActions::self()->nextTrackAction->setEnabled(false);
    StdActions::self()->prevTrackAction->setEnabled(false);
}

void MainWindow::stopAfterCurrentTrack()
{
    PlayQueueModel::self()->clearStopAfterTrack();
    emit stop(true);
}

void MainWindow::stopAfterTrack()
{
    QModelIndexList selected=playQueue->selectedIndexes(false); // Dont need sorted selection here...
    if (1==selected.count()) {
        QModelIndex idx=playQueueProxyModel.mapToSource(selected.first());
        PlayQueueModel::self()->setStopAfterTrack(PlayQueueModel::self()->getIdByRow(idx.row()));
    }
}

void MainWindow::playPauseTrack()
{
    switch (MPDStatus::self()->state()) {
    case MPDState_Playing:
        emit pause(true);
        break;
    case MPDState_Paused:
        emit pause(false);
        break;
    default:
        if (PlayQueueModel::self()->rowCount()>0) {
            if (-1!=PlayQueueModel::self()->currentSong() && -1!=PlayQueueModel::self()->currentSongRow()) {
                emit startPlayingSongId(PlayQueueModel::self()->currentSong());
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
        QModelIndex idx=playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(PlayQueueModel::self()->currentSongRow(), 0));
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

void MainWindow::updatePlayQueue(const QList<Song> &songs, bool isComplete)
{
    StdActions::self()->playPauseTrackAction->setEnabled(!songs.isEmpty());
    StdActions::self()->nextTrackAction->setEnabled(StdActions::self()->stopPlaybackAction->isEnabled() && songs.count()>1);
    StdActions::self()->prevTrackAction->setEnabled(StdActions::self()->stopPlaybackAction->isEnabled() && songs.count()>1);
    StdActions::self()->savePlayQueueAction->setEnabled(!songs.isEmpty());
    clearPlayQueueAction->setEnabled(!songs.isEmpty());

    int topRow=-1;
    QModelIndex topIndex=PlayQueueModel::self()->lastCommandWasUnodOrRedo() ? playQueue->indexAt(QPoint(0, 0)) : QModelIndex();
    if (topIndex.isValid()) {
        topRow=playQueueProxyModel.mapToSource(topIndex).row();
    }
    bool wasEmpty=0==PlayQueueModel::self()->rowCount();
    bool songChanged=false;

    PlayQueueModel::self()->update(songs, isComplete);

    if (songs.isEmpty()) {
        updateCurrentSong(Song(), wasEmpty);
    } else if (wasEmpty || current.isStandardStream()) {
        // Check to see if it has been updated...
        Song pqSong=PlayQueueModel::self()->getSongByRow(PlayQueueModel::self()->currentSongRow());
        if (wasEmpty || pqSong.isDifferent(current) ) {
            updateCurrentSong(pqSong, wasEmpty);
            songChanged=true;
        }
    }

    QModelIndex idx=playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(PlayQueueModel::self()->currentSongRow(), 0));
    bool scroll=songChanged && autoScrollPlayQueue && playQueueProxyModel.isEmpty() && (wasEmpty || MPDState_Playing==MPDStatus::self()->state());
    playQueue->updateRows(idx.row(), current.key, scroll, wasEmpty);
    if (!scroll && topRow>0 && topRow<PlayQueueModel::self()->rowCount()) {
        playQueue->scrollTo(playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(topRow, PlayQueueModel::COL_TITLE)), QAbstractItemView::PositionAtTop);
    }

    playQueueItemsSelected(playQueue->haveSelectedItems());
}

void MainWindow::updateWindowTitle()
{
    bool multipleConnections=connectionsAction->isVisible();
    QString connection=MPDConnection::self()->getDetails().getName();
    setWindowTitle(multipleConnections ? i18n("Cantata (%1)", connection) : "Cantata");
}

void MainWindow::updateCurrentSong(Song song, bool wasEmpty)
{
    if (song.isCdda()) {
        emit getStatus();
        if (song.isUnknownAlbum()) {
            Song pqSong=PlayQueueModel::self()->getSongById(song    .id);
            if (!pqSong.isEmpty()) {
                song=pqSong;
            }
        }
    }

//    if (song.isCantataStream()) {
//        Song mod=HttpServer::self()->decodeUrl(song.file);
//        if (!mod.title.isEmpty()) {
//            mod.id=song.id;
//            song=mod;
//        }
//    }

    bool diffSong=song.isDifferent(current);
    current=song;

    CurrentCover::self()->update(current);
    #ifdef QT_QTDBUS_FOUND
    if (mpris) {
        mpris->updateCurrentSong(current);
    }
    #endif
    if (current.time<5 && MPDStatus::self()->songId()==current.id && MPDStatus::self()->timeTotal()>5) {
        current.time=MPDStatus::self()->timeTotal();
    }
    nowPlaying->setEnabled(-1!=current.id && !current.isCdda() && (!currentIsStream() || current.time>5));
    nowPlaying->update(current);
    bool isPlaying=MPDState_Playing==MPDStatus::self()->state();
    PlayQueueModel::self()->updateCurrentSong(current.id);
    QModelIndex idx=playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(PlayQueueModel::self()->currentSongRow(), 0));
    playQueue->updateRows(idx.row(), current.key, autoScrollPlayQueue && playQueueProxyModel.isEmpty() && isPlaying, wasEmpty);
    scrollPlayQueue(wasEmpty);
    if (diffSong) {
        context->update(current);
        trayItem->songChanged(song, isPlaying);
    }
    centerPlayQueueAction->setEnabled(!song.isEmpty());
}

void MainWindow::scrollPlayQueue(bool wasEmpty)
{
    if (autoScrollPlayQueue && (wasEmpty || MPDState_Playing==MPDStatus::self()->state()) && !playQueue->isGrouped()) {
        qint32 row=PlayQueueModel::self()->currentSongRow();
        if (row>=0) {
            playQueue->scrollTo(playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(row, 0)), QAbstractItemView::PositionAtCenter);
        }
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
        PlayQueueModel::self()->clearStopAfterTrack();
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

    PlayQueueModel::self()->setState(status->state());
    StdActions::self()->playPauseTrackAction->setEnabled(status->playlistLength()>0);
    switch (status->state()) {
    case MPDState_Playing:
        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPauseIcon);
        enableStopActions(true);
        StdActions::self()->nextTrackAction->setEnabled(status->playlistLength()>1);
        StdActions::self()->prevTrackAction->setEnabled(status->playlistLength()>1);
        nowPlaying->startTimer();
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
        enableStopActions(false);
        StdActions::self()->nextTrackAction->setEnabled(false);
        StdActions::self()->prevTrackAction->setEnabled(false);
        if (!StdActions::self()->playPauseTrackAction->isEnabled()) {
            current=Song();
            nowPlaying->update(current);
            CurrentCover::self()->update(current);
            context->update(current);
        }
        current.id=0;
        trayItem->setToolTip("cantata", i18n("Cantata"), QLatin1String("<i>")+i18n("Playback stopped")+QLatin1String("</i>"));
        nowPlaying->stopTimer();
        break;
    case MPDState_Paused:
        StdActions::self()->playPauseTrackAction->setIcon(Icons::self()->toolbarPlayIcon);
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
    #if defined Q_OS_WIN && QT_VERSION>=0x050000
    if (thumbnailTooolbar) {
        thumbnailTooolbar->update(status);
    }
    #endif
    #ifdef Q_OS_MAC
    dockMenu->update(status);
    #endif
}

void MainWindow::playQueueItemActivated(const QModelIndex &index)
{
    emit startPlayingSongId(PlayQueueModel::self()->getIdByRow(playQueueProxyModel.mapToSource(index).row()));
}

void MainWindow::clearPlayQueue()
{
    if (!Settings::self()->playQueueConfirmClear() ||
        MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Remove all songs from play queue?"))) {
        if (dynamicLabel->isVisible()) {
            Dynamic::self()->stop(true);
        } else {
            PlayQueueModel::self()->removeAll();
        }
    }
}

void MainWindow::centerPlayQueue()
{
    QModelIndex idx=playQueueProxyModel.mapFromSource(PlayQueueModel::self()->index(PlayQueueModel::self()->currentSongRow(),
                                                                           playQueue->isGrouped() ? 0 : PlayQueueModel::COL_TITLE));
    if (idx.isValid()) {
        if (playQueue->isGrouped()) {
            playQueue->updateRows(idx.row(), current.key, true, true);
        } else {
            playQueue->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }
    }
}

void MainWindow::appendToPlayQueue(int action, quint8 priority)
{
    playQueueSearchWidget->clear();
    if (currentPage) {
        currentPage->addSelectionToPlaylist(QString(), action, priority);
    }
}

void MainWindow::addWithPriority()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (!act || !MPDConnection::self()->canUsePriority()) {
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
                ids.append(PlayQueueModel::self()->getIdByRow(playQueueProxyModel.mapToSource(idx).row()));
            }
            emit setPriority(ids, prio);
        } else {
            appendToPlayQueue(false, prio);
        }
    }
}

void MainWindow::addToNewStoredPlaylist()
{
    bool pq=playQueue->hasFocus();
    for(;;) {
        QString name = InputDialog::getText(i18n("Playlist Name"), i18n("Enter a name for the playlist:"), QString(), 0, this);

        if (name==MPDConnection::constStreamsPlayListName) {
            MessageBox::error(this, i18n("'%1' is used to store favorite streams, please choose another name.", name));
            continue;
        }
        if (PlaylistsModel::self()->exists(name)) {
            switch(MessageBox::warningYesNoCancel(this, i18n("A playlist named '%1' already exists!\n\nAdd to that playlist?", name),
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
            files = PlayQueueModel::self()->filenames();
        } else {
            foreach (const QModelIndex &idx, items) {
                Song s = PlayQueueModel::self()->getSongByRow(playQueueProxyModel.mapToSource(idx).row());
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
    StreamDialog dlg(this, true);

    if (QDialog::Accepted==dlg.exec()) {
        QString url=dlg.url();

        if (dlg.save()) {
            StreamsModel::self()->addToFavourites(url, dlg.name());
        }
        PlayQueueModel::self()->addItems(QStringList() << StreamsModel::modifyUrl(url, true, dlg.name()), false, 0);
    }
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
    if (expandInterfaceAction->isChecked()) {
        #ifndef Q_OS_MAC
        fullScreenLabel->setVisible(!isFullScreen());
        #endif
        expandInterfaceAction->setEnabled(isFullScreen());
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
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

void MainWindow::currentTabChanged(int index)
{
    prevPage=index;
    controlDynamicButton();
    switch(index) {
    case PAGE_LIBRARY:   currentPage=libraryPage;   break;
    case PAGE_FOLDERS:   folderPage->load();  currentPage=folderPage; break;
    case PAGE_PLAYLISTS: currentPage=playlistsPage; break;
    case PAGE_ONLINE:    currentPage=onlinePage;    break;
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
    case PAGE_FOLDERS:
        folderPage->setEnabled(!folderPage->isEnabled());
        break;
    case PAGE_PLAYLISTS:
        break;
    case PAGE_ONLINE:
        onlinePage->setEnabled(onlinePage->isEnabled());
        break;
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
    bool ah=Settings::self()->splitterAutoHide();
    if (splitter->isAutoHideEnabled()!=ah) {
        splitter->setAutoHideEnabled(ah);
        splitter->setAutohidable(0, ah);
    }
}

void MainWindow::locateTracks(const QList<Song> &songs)
{
    if (!songs.isEmpty() && tabWidget->isEnabled(PAGE_LIBRARY)) {
        showLibraryTab();
        libraryPage->showSongs(songs);
    }
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
    Dynamic::self()->helperMessage(message);
}

void MainWindow::setCollection(const QString &collection)
{
    if (!connectionsAction->isVisible()) {
        return;
    }
    foreach (QAction *act, connectionsAction->menu()->actions()) {
        if (Utils::strippedText(act->text())==collection) {
            if (!act->isChecked()) {
                act->trigger();
            }
            break;
        }
    }
}

void MainWindow::mpdConnectionName(const QString &name)
{
    foreach (QAction *act, connectionsAction->menu()->actions()) {
        if (Utils::strippedText(act->text())==name) {
            if (!act->isChecked()) {
                act->setChecked(true);
            }
            break;
        }
    }
}

void MainWindow::showPlayQueueSearch()
{
    playQueueSearchWidget->activate();
}

void MainWindow::showSearch()
{
    if (!searchPlayQueueAction->isEnabled() && playQueue->hasFocus()) {
        playQueueSearchWidget->activate();
    } else if (context->isVisible()) {
        context->search();
    } else if (currentPage && splitter->sizes().at(0)>0) {
        if (currentPage==folderPage) {
            showTab(PAGE_SEARCH);
            if (PAGE_SEARCH==tabWidget->currentIndex()) {
                searchPage->setSearchCategory("file");
            }
        } else {
            currentPage->focusSearch();
        }
    } else if (!searchPlayQueueAction->isEnabled() && (playQueuePage->isVisible() || playQueue->isVisible())) {
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
    if (TagEditor::instanceCount() || !canShowDialog()) {
        return;
    }
    QList<Song> songs;
    bool isPlayQueue=playQueue->hasFocus();
    if (isPlayQueue) {
        songs=playQueue->selectedSongs();
    } else if (currentPage) {
        songs=currentPage->selectedSongs();
    }
    if (songs.isEmpty()) {
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
    }
    #endif
    MpdLibraryModel::self()->getDetails(artists, albumArtists, composers, albums, genres);
    TagEditor *dlg=new TagEditor(this, songs, artists, albumArtists, composers, albums, genres, udi);
    dlg->show();
    #endif
}

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
    if (playQueue->hasFocus()) {
        copyToDevice(QString(), udi, playQueue->selectedSongs());
    } else if (currentPage) {
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
        Song s=PlayQueueModel::self()->getSongByRow(PlayQueueModel::self()->getRowById(nextTrackId));
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
    tabWidget->setToolTip(PAGE_FOLDERS, foldersTabAction->toolTip());
    tabWidget->setToolTip(PAGE_PLAYLISTS, playlistsTabAction->toolTip());
    tabWidget->setToolTip(PAGE_ONLINE, onlineTabAction->toolTip());
    #ifdef ENABLE_DEVICES_SUPPORT
    tabWidget->setToolTip(PAGE_DEVICES, devicesTabAction->toolTip());
    #endif
    tabWidget->setToolTip(PAGE_SEARCH, searchTabAction->toolTip());
    tabWidget->setToolTip(PAGE_CONTEXT, songInfoAction->toolTip());
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

int MainWindow::calcMinHeight()
{
    return tabWidget->style()&FancyTabWidget::Side && tabWidget->style()&FancyTabWidget::Large
            ? calcCollapsedSize()+(tabWidget->visibleCount()*tabWidget->tabSize().height())
            : Utils::scaleForDpi(256);
}

int MainWindow::calcCollapsedSize()
{
    return toolbar->height()+(menuBar() && menuBar()->isVisible() ? menuBar()->height() : 0);
}

void MainWindow::setCollapsedSize()
{
    if (!expandInterfaceAction->isChecked()) {
        int w=width();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        adjustSize();
        collapsedSize=QSize(w, calcCollapsedSize());
        resize(collapsedSize);
        setFixedHeight(collapsedSize.height());
    }
}

void MainWindow::expandOrCollapse(bool saveCurrentSize)
{
    if (!expandInterfaceAction->isChecked() && (isFullScreen() || isMaximized() || messageWidget->isVisible())) {
        expandInterfaceAction->setChecked(true);
        return;
    }

    static bool lastMax=false;

    bool showing=expandInterfaceAction->isChecked();
    QPoint p(isVisible() ? pos() : QPoint());

    if (!showing) {
        setMinimumHeight(0);
        lastMax=isMaximized();
        if (saveCurrentSize) {
            expandedSize=size();
        }
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
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
        // Width also sometimes expands, so make sure this is no larger than it was before...
        collapsedSize=QSize(collapsedSize.isValid() ? collapsedSize.width() : (size().width()>prevWidth ? prevWidth : size().width()), calcCollapsedSize());
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
        setCollapsedSize();
    }
}

void MainWindow::hideWindow()
{
    lastPos=pos();
    hide();
}

void MainWindow::restoreWindow()
{
    bool wasHidden=isHidden();
    Utils::raiseWindow(this);
    if (wasHidden && !lastPos.isNull()) {
        move(lastPos);
    }
}
