/*
 * Cantata
 *
 * Copyright (c) 2011-2019 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "application.h"
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
#include "models/musiclibraryitemartist.h"
#include "models/musiclibraryitemalbum.h"
#include "models/mpdlibrarymodel.h"
#include "librarypage.h"
#include "folderpage.h"
#include "streams/streamdialog.h"
#include "searchpage.h"
#include "customactions.h"
#include "apikeys.h"
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
#include "playlists/playlistspage.h"
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
#include "playlists/dynamicplaylists.h"
#include "support/messagewidget.h"
#include "widgets/groupedview.h"
#include "widgets/actionitemdelegate.h"
#include "widgets/icons.h"
#include "widgets/volumecontrol.h"
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
#include <QProcess>
#ifdef Q_OS_WIN
#include "windows/thumbnailtoolbar.h"
#endif
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QSpacerItem>
#include <QMenuBar>
#include <QFileDialog>
#include "mediakeys.h"
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

static const char * constRatingKey="rating";
static const char * constUserSettingProp = "user-setting-1";
static const char * constUserSetting2Prop = "user-setting-2";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , prevPage(-1)
    , lastState(MPDState_Inactive)
    , lastSongId(-1)
    , autoScrollPlayQueue(true)
    , singlePane(false)
    , shown(false)
    , currentPage(nullptr)
    #ifdef QT_QTDBUS_FOUND
    , mpris(nullptr)
    #endif
    , statusTimer(nullptr)
    , playQueueSearchTimer(nullptr)
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    , mpdAccessibilityTimer(nullptr)
    , showMenubarAction(nullptr)
    #endif
    , contextTimer(nullptr)
    , contextSwitchTime(0)
    , connectedState(CS_Init)
    , stopAfterCurrent(false)
    , responsiveSidebar(false)
    #if defined Q_OS_WIN
    , thumbnailTooolbar(0)
    #endif
{
    stopTrackButton=nullptr;
    coverWidget=nullptr;
    tabWidget=nullptr;
    savePlayQueueButton=nullptr;
    centerPlayQueueButton=nullptr;
    midSpacer=nullptr;

    QPoint p=pos();
    ActionCollection::setMainWidget(this);
    trayItem=new TrayItem(this);
    #ifdef QT_QTDBUS_FOUND
    new CantataAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/cantata", this);
    #endif
    setMinimumHeight(Utils::scaleForDpi(480));
    setMinimumWidth(Utils::scaleForDpi(400));
    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
    messageWidget->hide();

    // Need to set these values here, as used in library/device loading...
    Song::setComposerGenres(Settings::self()->composerGenres());
    Song::setUseOriginalYear(Settings::self()->useOriginalYear());

    int hSpace=Utils::layoutSpacing(this);
    int vSpace=Utils::scaleForDpi(2);
    toolbarLayout->setContentsMargins(hSpace, vSpace, hSpace, vSpace);
    toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setWindowTitle("Cantata");
    QWidget *tb=toolbar;
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

    topToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    topToolBar->setContentsMargins(0, 0, 0, 0);
    QLayout *l=topToolBar->layout();
    if (l) {
        l->setMargin(0);
        l->setSpacing(0);
    }
    topToolBar->ensurePolished();
    toolbar=topToolBar;
    #elif !defined Q_OS_WIN
    QProxyStyle *proxy=qobject_cast<QProxyStyle *>(style());
    QStyle *check=proxy && proxy->baseStyle() ? proxy->baseStyle() : style();
    if (check->inherits("Kvantum::Style")) {
        QToolBar *topToolBar = addToolBar("ToolBar");
        topToolBar->setObjectName("MainToolBar");
        topToolBar->addWidget(toolbar);
        topToolBar->setMovable(false);
        topToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
        topToolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        topToolBar->setContentsMargins(0, 0, 0, 0);
        QLayout *l=topToolBar->layout();
        if (l) {
            l->setMargin(0);
            l->setSpacing(0);
        }
        topToolBar->ensurePolished();
        toolbar=topToolBar;
    } else {
        toolbar->setFixedHeight(qMax(54, (int)(fontMetrics().height()*3.25)+(toolbarLayout->spacing()*3)+(vSpace*2)));
    }
    #endif

    toolbar->ensurePolished();
    toolbar->adjustSize();
    coverWidget->setSize(toolbar->height()-(vSpace*2));
    tb->setMinimumHeight(toolbar->height());
    nowPlaying->initColors();
    nowPlaying->adjustSize();
    nowPlaying->setFixedHeight(nowPlaying->height());
    volumeSlider->setColor(nowPlaying->textColor());
    Icons::self()->initToolbarIcons(nowPlaying->textColor());
    Icons::self()->initSidebarIcons();
    QColor iconCol=Utils::monoIconColor();

    setWindowIcon(Icons::self()->appIcon);

    prefAction=ActionCollection::get()->createAction("configure", Utils::KDE==Utils::currentDe() ? tr("Configure Cantata...") : tr("Preferences..."),
                                                     Icons::self()->configureIcon);
    connect(prefAction, SIGNAL(triggered()),this, SLOT(showPreferencesDialog()));
    quitAction = ActionCollection::get()->createAction("quit", tr("Quit"), MonoIcon::icon(FontAwesome::poweroff, MonoIcon::constRed, MonoIcon::constRed));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(quit()));
    quitAction->setShortcut(QKeySequence::Quit);
    Action *aboutAction=ActionCollection::get()->createAction("about", tr("About Cantata..."), Icons::self()->appIcon);
    connect(aboutAction, SIGNAL(triggered()),this, SLOT(showAboutDialog()));
    #ifdef Q_OS_MAC
    prefAction->setMenuRole(QAction::PreferencesRole);
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction->setMenuRole(QAction::AboutRole);
    #endif
    restoreAction = new Action(tr("Show Window"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(restoreWindow()));

    serverInfoAction=ActionCollection::get()->createAction("mpdinfo", tr("Server information..."), MonoIcon::icon(FontAwesome::server, iconCol));
    connect(serverInfoAction, SIGNAL(triggered()),this, SLOT(showServerInfo()));
    serverInfoAction->setEnabled(Settings::self()->firstRun());
    refreshDbAction = ActionCollection::get()->createAction("refresh", tr("Refresh Database"), Icons::self()->refreshIcon);
    doDbRefreshAction = new Action(refreshDbAction->icon(), tr("Refresh"), this);
    refreshDbAction->setEnabled(false);
    connectAction = new Action(Icons::self()->connectIcon, tr("Connect"), this);
    connectionsAction = new Action(MonoIcon::icon(FontAwesome::server, iconCol), tr("Collection"), this);
    outputsAction = new Action(MonoIcon::icon(FontAwesome::volumeup, iconCol), tr("Outputs"), this);
    stopAfterTrackAction = ActionCollection::get()->createAction("stopaftertrack", tr("Stop After Track"), Icons::self()->toolbarStopIcon);

    QList<int> seeks=QList<int>() << 5 << 30 << 60;
    QList<int> seekShortcuts=QList<int>() << (int)Qt::ControlModifier << (int)Qt::ShiftModifier << (int)(Qt::ControlModifier|Qt::ShiftModifier);
    for (int i=0; i<seeks.length(); ++i) {
        int seek=seeks.at(i);
        Action *fwdAction = ActionCollection::get()->createAction("seekfwd"+QString::number(seek), tr("Seek forward (%1 seconds)").arg(seek));
        Action *revAction = ActionCollection::get()->createAction("seekrev"+QString::number(seek), tr("Seek backward (%1 seconds)").arg(seek));
        fwdAction->setProperty("offset", seek);
        revAction->setProperty("offset", -1*seek);
        connect(fwdAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(seek()));
        connect(revAction, SIGNAL(triggered()), MPDConnection::self(), SLOT(seek()));
        addAction(fwdAction);
        addAction(revAction);
        if (i<seekShortcuts.length()) {
            fwdAction->setShortcut((Qt::RightToLeft==layoutDirection() ? Qt::Key_Left : Qt::Key_Right)+seekShortcuts.at(i));
            revAction->setShortcut((Qt::RightToLeft==layoutDirection() ? Qt::Key_Right : Qt::Key_Left)+seekShortcuts.at(i));
        }
    }

    addPlayQueueToStoredPlaylistAction = new Action(Icons::self()->playlistListIcon, tr("Add To Stored Playlist"), this);
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = new Action(StdActions::self()->copyToDeviceAction->icon(), Utils::strippedText(StdActions::self()->copyToDeviceAction->text()), this);
    copyToDeviceAction->setMenu(DevicesModel::self()->menu()->duplicate(nullptr));
    #endif
    cropPlayQueueAction = ActionCollection::get()->createAction("cropplaylist", tr("Crop Others"));
    addStreamToPlayQueueAction = ActionCollection::get()->createAction("addstreamtoplayqueue", tr("Add Stream URL"));
    addLocalFilesToPlayQueueAction = ActionCollection::get()->createAction("addlocalfiles", tr("Add Local Files"));
    QIcon clearIcon = MonoIcon::icon(FontAwesome::times, MonoIcon::constRed, MonoIcon::constRed);
    clearPlayQueueAction = ActionCollection::get()->createAction("clearplaylist", tr("Clear"), clearIcon);
    clearPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::Key_K);
    centerPlayQueueAction = ActionCollection::get()->createAction("centerplaylist", tr("Center On Current Track"), Icons::self()->centrePlayQueueOnTrackIcon);
    expandInterfaceAction = ActionCollection::get()->createAction("expandinterface", tr("Expanded Interface"), MonoIcon::icon(FontAwesome::expand, iconCol));
    expandInterfaceAction->setCheckable(true);
    songInfoAction = ActionCollection::get()->createAction("showsonginfo", tr("Show Current Song Information"), Icons::self()->infoIcon);
    songInfoAction->setShortcut(Qt::Key_F12);
    songInfoAction->setCheckable(true);
    fullScreenAction = ActionCollection::get()->createAction("fullScreen", tr("Full Screen"), MonoIcon::icon(FontAwesome::arrowsalt, iconCol));
    #ifndef Q_OS_MAC
    fullScreenAction->setShortcut(Qt::Key_F11);
    #endif
    randomPlayQueueAction = ActionCollection::get()->createAction("randomplaylist", tr("Random"), Icons::self()->shuffleIcon);
    repeatPlayQueueAction = ActionCollection::get()->createAction("repeatplaylist", tr("Repeat"), Icons::self()->repeatIcon);
    singlePlayQueueAction = ActionCollection::get()->createAction("singleplaylist", tr("Single"), Icons::self()->singleIcon, tr("When 'Single' is activated, playback is stopped after current song, or song is repeated if 'Repeat' is enabled."));
    consumePlayQueueAction = ActionCollection::get()->createAction("consumeplaylist", tr("Consume"), Icons::self()->consumeIcon, tr("When consume is activated, a song is removed from the play queue after it has been played."));
    searchPlayQueueAction = ActionCollection::get()->createAction("searchplaylist", tr("Find in Play Queue"), Icons::self()->searchIcon);
    addAction(searchPlayQueueAction);
    searchPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_F);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamPlayAction = ActionCollection::get()->createAction("streamplay", tr("Play HTTP Output Stream"), Icons::self()->httpStreamIcon);
    streamPlayAction->setCheckable(true);
    streamPlayAction->setChecked(false);
    streamPlayAction->setVisible(false);
    #endif

    locateAction = new Action(Icons::self()->searchIcon, tr("Locate In Library"), this);
    locateArtistAction = ActionCollection::get()->createAction("locateartist", tr("Artist"));
    locateAlbumAction = ActionCollection::get()->createAction("locatealbum", tr("Album"));
    locateTrackAction = ActionCollection::get()->createAction("locatetrack", tr("Track"));
    locateArtistAction->setSettingsText(locateAction);
    locateAlbumAction->setSettingsText(locateAction);
    locateTrackAction->setSettingsText(locateAction);
    addAction(locateAction);

    QMenu *locateMenu=new QMenu();
    locateMenu->addAction(locateArtistAction);
    locateMenu->addAction(locateAlbumAction);
    locateMenu->addAction(locateTrackAction);
    locateAction->setMenu(locateMenu);

    playNextAction = ActionCollection::get()->createAction("playnext", tr("Play Next"));
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction = ActionCollection::get()->createAction("editpqtags", Utils::strippedText(StdActions::self()->editTagsAction->text()), StdActions::self()->editTagsAction->icon());
    editPlayQueueTagsAction->setSettingsText(tr("Edit Track Information (Play Queue)"));
    #endif
    addAction(expandAllAction = ActionCollection::get()->createAction("expandall", tr("Expand All")));
    expandAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Down);
    addAction(collapseAllAction = ActionCollection::get()->createAction("collapseall", tr("Collapse All")));
    collapseAllAction->setShortcut(Qt::ControlModifier+Qt::Key_Up);
    cancelAction = ActionCollection::get()->createAction("cancel", tr("Cancel"), Icons::self()->cancelIcon);
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
    addPlayQueueToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu()->duplicate(nullptr));

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
    addLocalFilesToPlayQueueAction->setEnabled(false);
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
    addAction(showPlayQueueAction = ActionCollection::get()->createAction("showplayqueue", tr("Play Queue"), Icons::self()->playqueueIcon));
    showPlayQueueAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+Qt::Key_Q);
    tabWidget->addTab(playQueuePage, TAB_ACTION(showPlayQueueAction), playQueueInSidebar);
    connect(showPlayQueueAction, SIGNAL(triggered()), this, SLOT(showPlayQueue()));
    libraryPage = new LibraryPage(this);
    addAction(libraryTabAction = ActionCollection::get()->createAction("showlibrarytab", tr("Library"), Icons::self()->libraryIcon));
    libraryTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(libraryPage, TAB_ACTION(libraryTabAction), !hiddenPages.contains(libraryPage->metaObject()->className()));
    connect(libraryTabAction, SIGNAL(triggered()), this, SLOT(showLibraryTab()));
    folderPage = new FolderPage(this);
    addAction(foldersTabAction = ActionCollection::get()->createAction("showfolderstab", tr("Folders"), Icons::self()->foldersIcon));
    foldersTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(folderPage, TAB_ACTION(foldersTabAction), !hiddenPages.contains(folderPage->metaObject()->className()));
    connect(foldersTabAction, SIGNAL(triggered()), this, SLOT(showFoldersTab()));
    folderPage->setEnabled(!hiddenPages.contains(folderPage->metaObject()->className()));
    playlistsPage = new PlaylistsPage(this);
    addAction(playlistsTabAction = ActionCollection::get()->createAction("showplayliststab", tr("Playlists"), Icons::self()->playlistsIcon));
    playlistsTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(playlistsPage, TAB_ACTION(playlistsTabAction), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    connect(playlistsTabAction, SIGNAL(triggered()), this, SLOT(showPlaylistsTab()));
    connect(playlistsPage, SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(DynamicPlaylists::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(DynamicPlaylists::self(), SIGNAL(running(bool)), dynamicLabel, SLOT(setVisible(bool)));
    connect(DynamicPlaylists::self(), SIGNAL(running(bool)), this, SLOT(controlDynamicButton()));
    stopDynamicButton->setDefaultAction(DynamicPlaylists::self()->stopAct());
    onlinePage = new OnlineServicesPage(this);
    addAction(onlineTabAction = ActionCollection::get()->createAction("showonlinetab", tr("Internet"), Icons::self()->onlineIcon));
    onlineTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(onlinePage, TAB_ACTION(onlineTabAction), !hiddenPages.contains(onlinePage->metaObject()->className()));
    onlinePage->setEnabled(!hiddenPages.contains(onlinePage->metaObject()->className()));
    connect(onlineTabAction, SIGNAL(triggered()), this, SLOT(showOnlineTab()));
//    connect(onlinePage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(onlinePage, SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage = new DevicesPage(this);
    addAction(devicesTabAction = ActionCollection::get()->createAction("showdevicestab", tr("Devices"), Icons::self()->devicesIcon));
    devicesTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    tabWidget->addTab(devicesPage, TAB_ACTION(devicesTabAction), !hiddenPages.contains(devicesPage->metaObject()->className()));
    DevicesModel::self()->setEnabled(!hiddenPages.contains(devicesPage->metaObject()->className()));
    connect(devicesTabAction, SIGNAL(triggered()), this, SLOT(showDevicesTab()));
    #endif
    searchPage = new SearchPage(this);
    addAction(searchTabAction = ActionCollection::get()->createAction("showsearchtab", tr("Search"), Icons::self()->searchTabIcon));
    searchTabAction->setShortcut(Qt::ControlModifier+Qt::ShiftModifier+nextKey(sidebarPageShortcutKey));
    connect(searchTabAction, SIGNAL(triggered()), this, SLOT(showSearchTab()));
    connect(searchPage, SIGNAL(locate(QList<Song>)), this, SLOT(locateTracks(QList<Song>)));
    tabWidget->addTab(searchPage, TAB_ACTION(searchTabAction), !hiddenPages.contains(searchPage->metaObject()->className()));
    tabWidget->addTab(contextPage, Icons::self()->infoSidebarIcon, tr("Info"), songInfoAction->text(),
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
    playQueueSearchWidget->setToolTip(tr("<p>Enter a string to search artist, album, title, etc. To filter based on year, add <i>#year-range</i> to search string - e.g.</p><ul>"
                              "<li><b><i>#2000</i></b> return tracks from 2000</li>"
                              "<li><b><i>#1980-1989</i></b> return tracks from the 80's</li>"
                              "<li><b><i>Blah #2000</i></b> to search for string <i>Blah</i> and only return tracks from 2000</li>"
                              "</ul></p>"));
    QList<QToolButton *> playbackBtns=QList<QToolButton *>() << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    QList<QToolButton *> controlBtns=QList<QToolButton *>() << menuButton << songInfoButton;
    int playbackIconSizeNonScaled=24==Icons::self()->toolbarPlayIcon.actualSize(QSize(24, 24)).width() ? 24 : 28;
    int playbackIconSize=Utils::scaleForDpi(playbackIconSizeNonScaled);
    int playPauseIconSize=Utils::scaleForDpi(32);
    int controlIconSize=Utils::scaleForDpi(22);
    int controlButtonSize=Utils::scaleForDpi(32);
    int playbackButtonSize=28==playbackIconSizeNonScaled ? Utils::scaleForDpi(34) : controlButtonSize;

    for (QToolButton *b: controlBtns) {
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setFixedSize(QSize(controlButtonSize, controlButtonSize));
        b->setIconSize(QSize(controlIconSize, controlIconSize));
    }
    for (QToolButton *b: playbackBtns) {
        b->setAutoRaise(true);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setFixedSize(QSize(playbackButtonSize, playbackButtonSize));
        b->setIconSize(QSize(playbackIconSize, playbackIconSize));
    }

    playPauseTrackButton->setIconSize(QSize(playPauseIconSize, playPauseIconSize));
    playPauseTrackButton->setFixedSize(QSize(playPauseIconSize+6, playPauseIconSize+6));

    QList<QWidget *> pqWidgets = QList<QWidget *>() << stopDynamicButton << dynamicLabel << playQueueStatsLabel << fullScreenLabel
                                                    << repeatButton << singleButton << randomButton << consumeButton << midSpacer
                                                    << centerPlayQueueButton << savePlayQueueButton << clearPlayQueueButton << sizeGrip;
    for (const auto &item: pqWidgets) {
        Application::fixSize(item);
    }

    if (fullScreenAction->isEnabled()) {
        fullScreenAction->setChecked(Settings::self()->showFullScreen());
    }

    randomPlayQueueAction->setChecked(false);
    repeatPlayQueueAction->setChecked(false);
    singlePlayQueueAction->setChecked(false);
    consumePlayQueueAction->setChecked(false);

    expandedSize=Settings::self()->mainWindowSize();
    collapsedSize=Settings::self()->mainWindowCollapsedSize();

    int width=playPauseTrackButton->width()*25;
    QSize defaultSize(playPauseTrackButton->width()*25, playPauseTrackButton->height()*18);

    if (Settings::self()->firstRun() || (expandInterfaceAction->isChecked() && expandedSize.isEmpty())) {
        resize(defaultSize);
        splitter->setSizes(QList<int>() << width*0.4 << width*0.6);
    } else {
        if (expandInterfaceAction->isChecked()) {
            if (!expandedSize.isEmpty()) {
                if (!Settings::self()->maximized() && expandedSize.width()>1) {
                    resize(expandedSize);
                    expandOrCollapse(false);
                } else {
                    resize(expandedSize.width()>1 ? expandedSize : defaultSize);
                    // Issue #1137 Under Windows, Cantata does not restore maximized correctly if context is in sidebar
                    // ...so, set maximized after shown
                    QTimer::singleShot(0, this, SLOT(showMaximized()));
                    if (expandedSize.width()<=1) {
                        expandedSize=defaultSize;
                    }
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

    #ifndef Q_OS_WIN
    #ifdef Q_OS_MAC
    bool showMenubar = true;
    #else
    bool showMenubar = Utils::Gnome!=Utils::currentDe();
    #endif
    if (showMenubar) {
        #ifdef Q_OS_MAC
        menuButton->setVisible(false);
        #else
        showMenubarAction = ActionCollection::get()->createAction("showmenubar", tr("Show Menubar"));
        showMenubarAction->setShortcut(Qt::ControlModifier+Qt::Key_M);
        showMenubarAction->setCheckable(true);
        connect(showMenubarAction, SIGNAL(toggled(bool)), this, SLOT(toggleMenubar()));
        #endif

        QMenu *menu=new QMenu(tr("&Music"), this);
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
        menu=new QMenu(tr("&Edit"), this);
        addMenuAction(menu, PlayQueueModel::self()->undoAct());
        addMenuAction(menu, PlayQueueModel::self()->redoAct());
        menu->addSeparator();
        addMenuAction(menu, StdActions::self()->searchAction);
        addMenuAction(menu, searchPlayQueueAction);
        if (Utils::KDE!=Utils::currentDe()) {
            menu->addSeparator();
            addMenuAction(menu, prefAction);
        }
        menuBar()->addMenu(menu);
        if (Utils::KDE!=Utils::currentDe()) {
            menu=new QMenu(tr("&View"), this);
            #ifndef Q_OS_MAC
            if (showMenubarAction) {
                addMenuAction(menu, showMenubarAction);
                menu->addSeparator();
            }
            #endif
            addMenuAction(menu, expandInterfaceAction);
            addMenuAction(menu, fullScreenAction);
            //addMenuAction(menu, songInfoAction);
            menuBar()->addMenu(menu);
        }
        menu=new QMenu(tr("&Queue"), this);
        addMenuAction(menu, clearPlayQueueAction);
        addMenuAction(menu, StdActions::self()->savePlayQueueAction);
        addMenuAction(menu, addStreamToPlayQueueAction);
        addMenuAction(menu, addLocalFilesToPlayQueueAction);
        menu->addSeparator();
        addMenuAction(menu, PlayQueueModel::self()->shuffleAct());
        addMenuAction(menu, PlayQueueModel::self()->sortAct());
        menuBar()->addMenu(menu);
        if (Utils::KDE==Utils::currentDe()) {
            menu=new QMenu(tr("&Settings"), this);
            #ifndef Q_OS_MAC
            if (showMenubarAction) {
                addMenuAction(menu, showMenubarAction);
            }
            #endif
            addMenuAction(menu, expandInterfaceAction);
            addMenuAction(menu, fullScreenAction);
            //addMenuAction(menu, songInfoAction);
            menu->addSeparator();
            addMenuAction(menu, prefAction);
            menuBar()->addMenu(menu);
        }
        #ifdef Q_OS_MAC
        OSXStyle::self()->initWindowMenu(this);
        #endif
        menu=new QMenu(tr("&Help"), this);
        addMenuAction(menu, serverInfoAction);
        addMenuAction(menu, aboutAction);
        menuBar()->addMenu(menu);
    }
    #endif // infdef Q_OS_WIN

    #ifndef Q_OS_MAC
    QMenu *mainMenu=new QMenu(this);
    mainMenu->addAction(expandInterfaceAction);
    #ifndef Q_OS_WIN
    if (showMenubarAction) {
        mainMenu->addAction(showMenubarAction);
        showMenubarAction->setChecked(Settings::self()->showMenubar());
        toggleMenubar();
    }
    #endif
    mainMenu->addAction(fullScreenAction);
    mainMenu->addAction(connectionsAction);
    mainMenu->addAction(outputsAction);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    mainMenu->addAction(streamPlayAction);
    #endif
    mainMenu->addAction(prefAction);
    mainMenu->addAction(refreshDbAction);
    mainMenu->addSeparator();
    mainMenu->addAction(StdActions::self()->searchAction);
    mainMenu->addAction(searchPlayQueueAction);
    mainMenu->addSeparator();
    mainMenu->addAction(serverInfoAction);
    mainMenu->addAction(aboutAction);
    mainMenu->addSeparator();
    mainMenu->addAction(quitAction);
    menuButton->setIcon(Icons::self()->toolbarMenuIcon);
    menuButton->setAlignedMenu(mainMenu);
    #endif // ifndef Q_OS_MAC

    dynamicLabel->setVisible(false);
    stopDynamicButton->setVisible(false);
    StdActions::self()->addWithPriorityAction->setVisible(false);
    StdActions::self()->setPriorityAction->setVisible(false);

    playQueueProxyModel.setSourceModel(PlayQueueModel::self());
    playQueue->setModel(&playQueueProxyModel);
    playQueue->addAction(playQueue->removeFromAct());
    ratingAction=new Action(tr("Set Rating"), this);
    ratingAction->setMenu(new QMenu(nullptr));
    for (int i=0; i<((Song::Rating_Max/Song::Rating_Step)+1); ++i) {
        QString text;
        if (0==i) {
            text=tr("No Rating");
        } else {
            for (int s=0; s<i; ++s) {
                text+=QChar(0x2605);
            }
        }
        Action *action=ActionCollection::get()->createAction(QLatin1String("rating")+QString::number(i), text);
        action->setProperty(constRatingKey, i*Song::Rating_Step);
        action->setShortcut(Qt::AltModifier+Qt::Key_0+i);
        action->setSettingsText(ratingAction);
        ratingAction->menu()->addAction(action);
        connect(action, SIGNAL(triggered()), SLOT(setRating()));
    }
    playQueue->addAction(ratingAction);
    playQueue->addAction(StdActions::self()->setPriorityAction);
    playQueue->addAction(stopAfterTrackAction);
    playQueue->addAction(locateAction);
    #ifdef TAGLIB_FOUND
    playQueue->addAction(editPlayQueueTagsAction);
    #endif
    playQueue->addAction(playNextAction);
    Action *sep=new Action(this);
    sep->setSeparator(true);
    playQueue->addAction(sep);
    playQueue->addAction(PlayQueueModel::self()->removeDuplicatesAct());
    playQueue->addAction(clearPlayQueueAction);
    playQueue->addAction(cropPlayQueueAction);
    playQueue->addAction(StdActions::self()->savePlayQueueAction);
    playQueue->addAction(addStreamToPlayQueueAction);
    playQueue->addAction(addLocalFilesToPlayQueueAction);
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
    connect(folderPage->mpd(), SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(playlistsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(searchPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(StdActions::self()->deleteSongsAction, SIGNAL(triggered()), SLOT(deleteSongs()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(folderPage->mpd(), SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(searchPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    #endif
    for (QAction *act: StdActions::self()->setPriorityAction->menu()->actions()) {
        connect(act, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    }
    for (QAction *act: StdActions::self()->addWithPriorityAction->menu()->actions()) {
        connect(act, SIGNAL(triggered()), this, SLOT(addWithPriority()));
    }
    connect(StdActions::self()->appendToPlayQueueAndPlayAction, SIGNAL(triggered()), this, SLOT(appendToPlayQueueAndPlay()));
    connect(StdActions::self()->addToPlayQueueAndPlayAction, SIGNAL(triggered()), this, SLOT(addToPlayQueueAndPlay()));
    connect(StdActions::self()->insertAfterCurrentAction, SIGNAL(triggered()), this, SLOT(insertIntoPlayQueue()));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(outputsUpdated(const QList<Output> &)));
    connect(this, SIGNAL(enableOutput(quint32, bool)), MPDConnection::self(), SLOT(enableOutput(quint32, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(pause(bool)), MPDConnection::self(), SLOT(setPause(bool)));
    connect(this, SIGNAL(play()), MPDConnection::self(), SLOT(play()));
    connect(this, SIGNAL(stop(bool)), MPDConnection::self(), SLOT(stopPlaying(bool)));
    connect(this, SIGNAL(terminating()), MPDConnection::self(), SLOT(stop()));
    connect(this, SIGNAL(getStatus()), MPDConnection::self(), SLOT(getStatus()));
    connect(this, SIGNAL(playListInfo()), MPDConnection::self(), SLOT(playListInfo()));
    connect(this, SIGNAL(currentSong()), MPDConnection::self(), SLOT(currentSong()));
    connect(this, SIGNAL(setSeekId(qint32, quint32)), MPDConnection::self(), SLOT(setSeekId(qint32, quint32)));
    connect(this, SIGNAL(startPlayingSongId(qint32)), MPDConnection::self(), SLOT(startPlayingSongId(qint32)));
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(this, SIGNAL(setPriority(const QList<qint32> &, quint8, bool)), MPDConnection::self(), SLOT(setPriority(const QList<qint32> &, quint8, bool)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(PlayQueueModel::self(), SIGNAL(statsUpdated(int, quint32)), this, SLOT(updatePlayQueueStats(int, quint32)));
    connect(PlayQueueModel::self(), SIGNAL(fetchingStreams()), playQueue, SLOT(showSpinner()));
    connect(PlayQueueModel::self(), SIGNAL(streamsFetched()), playQueue, SLOT(hideSpinner()));
    connect(PlayQueueModel::self(), SIGNAL(updateCurrent(Song)), SLOT(updateCurrentSong(Song)));
    connect(PlayQueueModel::self(), SIGNAL(streamFetchStatus(QString)), playQueue, SLOT(streamFetchStatus(QString)));
    connect(PlayQueueModel::self(), SIGNAL(error(QString)), SLOT(showError(QString)));
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
    connect(ApiKeys::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
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
    connect(streamPlayAction, SIGNAL(triggered(bool)), HttpStream::self(), SLOT(setEnabled(bool)));
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
    connect(locateAlbumAction, SIGNAL(triggered()), this, SLOT(locateTrack()));
    connect(locateArtistAction, SIGNAL(triggered()), this, SLOT(locateTrack()));
    connect(this, SIGNAL(playNext(QList<quint32>,quint32,quint32)), MPDConnection::self(), SLOT(move(QList<quint32>,quint32,quint32)));
    connect(playNextAction, SIGNAL(triggered()), this, SLOT(moveSelectionAfterCurrentSong()));
    connect(qApp, SIGNAL(paletteChanged(const QPalette &)), this, SLOT(paletteChanged()));

    connect(StdActions::self()->searchAction, SIGNAL(triggered()), SLOT(showSearch()));
    connect(searchPlayQueueAction, SIGNAL(triggered()), this, SLOT(showPlayQueueSearch()));
    connect(playQueue, SIGNAL(focusSearch(QString)), playQueueSearchWidget, SLOT(activate(QString)));
    connect(expandAllAction, SIGNAL(triggered()), this, SLOT(expandAll()));
    connect(collapseAllAction, SIGNAL(triggered()), this, SLOT(collapseAll()));
    connect(addStreamToPlayQueueAction, SIGNAL(triggered()), this, SLOT(addStreamToPlayQueue()));
    connect(addLocalFilesToPlayQueueAction, SIGNAL(triggered()), this, SLOT(addLocalFilesToPlayQueue()));
    connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(controlPlayQueueButtons()));
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
    connect(qApp, SIGNAL(commitDataRequest(QSessionManager&)), SLOT(commitDataRequest(QSessionManager&)));
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
    MediaKeys::self()->start();
    #ifdef Q_OS_MAC
    dockMenu=new DockMenu(this);
    #endif
    updateActionToolTips();
    CustomActions::self()->setMainWindow(this);

    if (Utils::useSystemTray() && Settings::self()->startHidden()) {
        hide();
    } else {
        show();
    }
    QTimer::singleShot(0, this, SLOT(controlPlayQueueButtons()));
}

MainWindow::~MainWindow()
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (showMenubarAction) {
        Settings::self()->saveShowMenubar(showMenubarAction->isChecked());
    }
    #endif
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    HttpStream::self()->save();
    #endif
    bool hadCantataStreams=PlayQueueModel::self()->removeCantataStreams();
    Settings::self()->saveShowFullScreen(fullScreenAction->isChecked());
    if (!fullScreenAction->isChecked()) {
        if (expandInterfaceAction->isChecked()) {
            Settings::self()->saveMaximized(isMaximized());
            Settings::self()->saveMainWindowSize(isMaximized() ? previousSize : size());
        } else {
            Settings::self()->saveMaximized(false);
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
    if (Utils::useSystemTray()) {
        Settings::StartupState startupState=Settings::self()->startupState();
        Settings::self()->saveStartHidden(trayItem->isActive() && Settings::self()->minimiseOnClose() &&
                                          ((isHidden() && Settings::SS_ShowMainWindow!=startupState) || (Settings::SS_HideMainWindow==startupState)));
    }
    Settings::self()->save();
    disconnect(MPDConnection::self(), nullptr, nullptr, nullptr);
    if (Settings::self()->stopOnExit()) {
        DynamicPlaylists::self()->stop();
    }
    if (Settings::self()->stopOnExit()) {
        emit terminating();
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
    MediaKeys::self()->stop();
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

QStringList MainWindow::listActions() const
{
    QStringList allActions;
    for(int i = 0; i < ActionCollection::get()->actions().count(); i++) {
        Action *action = qobject_cast<Action *>(ActionCollection::get()->actions().at(i));
        if (!action) {
            continue;
        }
        allActions.append(action->objectName()+" - "+Action::settingsText(action));
    }
    return allActions;
}

void MainWindow::triggerAction(const QString &name)
{
    QAction *act=ActionCollection::get()->action(name);
    if (act) {
        act->trigger();
    }
}

#if !defined Q_OS_WIN
void MainWindow::addMenuAction(QMenu *menu, QAction *action)
{
    menu->addAction(action);
    addAction(action); // Bind action to window, so that it works when fullscreen!
}
#endif

void MainWindow::showError(const QString &message, bool showActions)
{
    if (!message.isEmpty()) {
        if (!isVisible()) {
            show();
        }
        expand();
    }

    if (QLatin1String("NO_SONGS")==message) {
        messageWidget->setError(tr("Failed to locate any songs matching the dynamic playlist rules."));
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
    addLocalFilesToPlayQueueAction->setEnabled(connected && (HttpServer::self()->isAlive() || MPDConnection::self()->localFilePlaybackSupported()));
    if (connected) {
        //if (!messageWidget->showingError()) {
            messageWidget->hide();
        //}
        if (CS_Connected!=connectedState) {
            emit playListInfo();
            emit outputs();
            MpdLibraryModel::self()->reload();
            if (CS_Init!=connectedState) {
                currentTabChanged(tabWidget->currentIndex());
            }
            connectedState=CS_Connected;
            StdActions::self()->addWithPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
            StdActions::self()->setPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
        }
    } else {
        libraryPage->clear();
        folderPage->mpd()->clear();
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

void MainWindow::showEvent(QShowEvent *event)
{
    #if defined Q_OS_WIN
    if (!thumbnailTooolbar) {
        thumbnailTooolbar=new ThumbnailToolBar(this);
    }
    #endif
    QMainWindow::showEvent(event);
    controlView();
    if (!shown) {
        shown=true;
        // Work-around for qt5ct palette issues...
        QTimer::singleShot(0, this, SLOT(paletteChanged()));
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayItem->isActive() && Settings::self()->minimiseOnClose() && event->spontaneous()) {
        lastPos=pos();
        hide();
        event->ignore();
    } else if (canClose()) {
        QMainWindow::closeEvent(event);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    previousSize=event->oldSize();
    QMainWindow::resizeEvent(event);
    controlView();
}

void MainWindow::playQueueItemsSelected(bool s)
{
    int rc=playQueue->model() ? playQueue->model()->rowCount() : 0;
    bool haveItems=rc>0;
    bool singleSelection=1==playQueue->selectedIndexes(false).count(); // Dont need sorted selection here...
    playQueue->removeFromAct()->setEnabled(s && haveItems);
    StdActions::self()->setPriorityAction->setEnabled(s && haveItems);
    locateAction->setEnabled(singleSelection);
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
        folderPage->mpd()->clear();
        PlaylistsModel::self()->clear();
        PlayQueueModel::self()->clear();
        searchPage->clear();
        if (!MPDConnection::self()->getDetails().isEmpty() && details!=MPDConnection::self()->getDetails()) {
            DynamicPlaylists::self()->stop();
        }
        showInformation(tr("Connecting to %1").arg(details.description()));
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
    HttpStream::self()->setEnabled(streamPlayAction->isChecked());
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
    messageWidget->setWarning(tr("Refresh MPD Database?"), false);
    expand();
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("About Cantata"),
                       tr("<b>Cantata %1</b><br/><br/>MPD client.<br/><br/>"
                           "&copy; 2011-2019 Craig Drummond<br/>Released under the <a href=\"http://www.gnu.org/licenses/gpl.html\">GPLv3</a>").arg(PACKAGE_VERSION_STRING)+
                       QLatin1String("<br/><br/>")+
                       tr("Please refer to <a href=\"https://github.com/CDrummond/cantata/issues\">Cantata's issue tracker</a> for a list of known issues, and to report new issues.")+
                       QLatin1String("<br/><br/><i><small>")+
                       tr("Based upon <a href=\"http://lowblog.nl\">QtMPC</a> - &copy; 2007-2010 The QtMPC Authors<br/>")+
                       tr("Context view backdrops courtesy of <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+QLatin1String("<br/>")+
                       tr("Context view metadata courtesy of <a href=\"http://www.wikipedia.org\">Wikipedia</a> and <a href=\"http://www.last.fm\">Last.fm</a>")+
                       QLatin1String("<br/><br/>")+tr("Please consider uploading your own music fan-art to <a href=\"http://www.fanart.tv\">FanArt.tv</a>")+
                       QLatin1String("</small></i>"));
}

bool MainWindow::canClose()
{
    if (onlinePage->isDownloading() &&
            MessageBox::No==MessageBox::warningYesNo(this, tr("A Podcast is currently being downloaded\n\nQuitting now will abort the download."),
                                                     QString(), GuiItem(tr("Abort download and quit")), GuiItem("Do not quit just yet"))) {
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
        MessageBox::error(this, tr("Please close other dialogs first."));
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
    qApp->quit();
}

void MainWindow::commitDataRequest(QSessionManager &mgr)
{
    Q_UNUSED(mgr)
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (isVisible() && trayItem->isActive() && Settings::self()->minimiseOnClose()) {
        // Issue 1183 If We are using a tray item, and have window open - then cantata intercepts close events
        // and hides the window. This seems to prevent the logout dialog appwaring under GNOME Shell.
        // To work-around this if qApp emits commitDataRequest and our window is open, them close.
        QMainWindow::close();
    }
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
    for (const Output &o: outputs) {
        if (o.enabled) {
            enabledMpd.insert(o.name);
        }
        mpd.insert(o.name);
    }

    for (QAction *act: menu->actions()) {
        menuItems.insert(act->data().toString());
    }

    if (menuItems!=mpd) {
        menu->clear();
        QList<Output> out=outputs;
        qSort(out);
        int i=Qt::Key_1;
        for (const Output &o: out) {
            QAction *act=menu->addAction(o.name, this, SLOT(toggleOutput()));
            act->setData(o.id);
            act->setCheckable(true);
            act->setChecked(o.enabled);
            act->setShortcut(Qt::ControlModifier+Qt::AltModifier+nextKey(i));
        }
    } else {
        for (const Output &o: outputs) {
            for (QAction *act: menu->actions()) {
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
            trayItem->showMessage(tr("Outputs"), tr("Enabled: %1").arg(names.join(QLatin1String(", "))));
        } else if (!switchedOff.isEmpty() && switchedOn.isEmpty()) {
            QStringList names=switchedOff.toList();
            qSort(names);
            trayItem->showMessage(tr("Outputs"), tr("Disabled: %1").arg(names.join(QLatin1String(", "))));
        } else if (!switchedOn.isEmpty() && !switchedOff.isEmpty()) {
            QStringList on=switchedOn.toList();
            qSort(on);
            QStringList off=switchedOff.toList();
            qSort(off);
            trayItem->showMessage(tr("Outputs"),
                                  tr("Enabled: %1").arg(on.join(QLatin1String(", ")))+QLatin1Char('\n')+
                                  tr("Disabled: %1").arg(off.join(QLatin1String(", "))));
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
        for (const MPDConnectionDetails &d: connections) {
            cfg.insert(d.name);
        }

        for (QAction *act: menu->actions()) {
            menuItems.insert(act->data().toString());
            act->setChecked(act->data().toString()==current);
        }

        if (menuItems!=cfg) {
            menu->clear();
            qSort(connections);
            int i=Qt::Key_1;
            for (const MPDConnectionDetails &d: connections) {
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
    for (QAction *act: connectionsAction->menu()->actions()) {
        act->setEnabled(enable);
    }
}

void MainWindow::controlDynamicButton()
{
    stopDynamicButton->setVisible(DynamicPlaylists::self()->isRunning());
    PlayQueueModel::self()->enableUndo(!DynamicPlaylists::self()->isRunning());
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
        mpris=nullptr;
    }
    CurrentCover::self()->setEnabled(mpris || Settings::self()->showPopups() || 0!=Settings::self()->playQueueBackground() || Settings::self()->showCoverWidget());
    #endif
}

void MainWindow::toggleMenubar()
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (showMenubarAction) {
        menuButton->setVisible(!showMenubarAction->isChecked());
        menuBar()->setVisible(showMenubarAction->isChecked());
        setCollapsedSize();
    }
    #endif
}

void MainWindow::paletteChanged()
{
    QColor before = nowPlaying->textColor();
    nowPlaying->initColors();
    if (before == nowPlaying->textColor()) {
        return;
    }

    volumeSlider->setColor(nowPlaying->textColor());

    Icons::self()->initToolbarIcons(nowPlaying->textColor());
    StdActions::self()->prevTrackAction->setIcon(Icons::self()->toolbarPrevIcon);
    StdActions::self()->nextTrackAction->setIcon(Icons::self()->toolbarNextIcon);
    StdActions::self()->playPauseTrackAction->setIcon(MPDState_Playing==MPDStatus::self()->state() ? Icons::self()->toolbarPauseIcon : Icons::self()->toolbarPlayIcon);
    StdActions::self()->stopPlaybackAction->setIcon(Icons::self()->toolbarStopIcon);
    StdActions::self()->stopAfterCurrentTrackAction->setIcon(Icons::self()->toolbarStopIcon);
    StdActions::self()->stopAfterTrackAction->setIcon(Icons::self()->toolbarStopIcon);
    songInfoAction->setIcon(Icons::self()->infoIcon);
    menuButton->setIcon(Icons::self()->toolbarMenuIcon);
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
    tabWidget->setProperty(constUserSettingProp, Settings::self()->hiddenPages());
    tabWidget->setProperty(constUserSetting2Prop, Settings::self()->sidebar());
    coverWidget->setProperty(constUserSettingProp, Settings::self()->showCoverWidget());
    stopTrackButton->setProperty(constUserSettingProp, Settings::self()->showStopButton());
    responsiveSidebar=Settings::self()->responsiveSidebar();
    controlView(true);
    #if defined Q_OS_WIN
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
            contextTimer=nullptr;
        }
    }
    MPDConnection::self()->setVolumeFadeDuration(Settings::self()->stopFadeDuration());
    MPDParseUtils::setSingleTracksFolders(Settings::self()->singleTracksFolders());
    MPDParseUtils::setCueFileSupport(Settings::self()->cueSupport());
    volumeSlider->setPageStep(Settings::self()->volumeStep());
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
        emit enableOutput(act->data().toUInt(), act->isChecked());
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
                                  tr("Server Information")+QLatin1String("<br/><br/>")+
                                  #endif
                                  QLatin1String("<p><table>")+
                                  tr("<tr><td colspan=\"2\"><b>Server</b></td></tr>"
                                       "<tr><td align=\"right\">Protocol:&nbsp;</td><td>%1.%2.%3</td></tr>"
                                       "<tr><td align=\"right\">Uptime:&nbsp;</td><td>%4</td></tr>"
                                       "<tr><td align=\"right\">Playing:&nbsp;</td><td>%5</td></tr>"
                                       "<tr><td align=\"right\">Handlers:&nbsp;</td><td>%6</td></tr>"
                                       "<tr><td align=\"right\">Tags:&nbsp;</td><td>%7</td></tr>")
                                       .arg((version>>16)&0xFF).arg((version>>8)&0xFF).arg(version&0xFF)
                                       .arg(Utils::formatDuration(MPDStats::self()->uptime()))
                                       .arg(Utils::formatDuration(MPDStats::self()->playtime()))
                                       .arg(handlers.join(", ")).arg(tags.join(", "))+
                                  QLatin1String("<tr/>")+
                                  tr("<tr><td colspan=\"2\"><b>Database</b></td></tr>"
                                       "<tr><td align=\"right\">Artists:&nbsp;</td><td>%1</td></tr>"
                                       "<tr><td align=\"right\">Albums:&nbsp;</td><td>%2</td></tr>"
                                       "<tr><td align=\"right\">Songs:&nbsp;</td><td>%3</td></tr>"
                                       "<tr><td align=\"right\">Duration:&nbsp;</td><td>%4</td></tr>"
                                       "<tr><td align=\"right\">Updated:&nbsp;</td><td>%5</td></tr>")
                                       .arg(MPDStats::self()->artists()).arg(MPDStats::self()->albums()).arg(MPDStats::self()->songs())
                                       .arg(Utils::formatDuration(MPDStats::self()->dbPlaytime())).arg(dbUpdate.toString(Qt::SystemLocaleShortDate))+
                                  QLatin1String("</table></p>"),
                            tr("Server Information"));
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
    setWindowTitle(multipleConnections ? tr("Cantata (%1)").arg(connection) : "Cantata");
}

void MainWindow::updateCurrentSong(Song song, bool wasEmpty)
{
    if (song.isCdda()) {
        emit getStatus();
        if (song.isUnknownAlbum()) {
            Song pqSong=PlayQueueModel::self()->getSongById(song.id);
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
        showError(tr("MPD reported the following error: %1").arg(status->error()));
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

    if (status->timeElapsed()<64800 && (!currentIsStream() || (status->timeTotal()>0 && status->timeElapsed()<=status->timeTotal()))) {
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
        trayItem->setToolTip("cantata", tr("Cantata"), QLatin1String("<i>")+tr("Playback stopped")+QLatin1String("</i>"));
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
    #if defined Q_OS_WIN
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
    if (index.isValid()) {
        emit startPlayingSongId(PlayQueueModel::self()->getIdByRow(playQueueProxyModel.mapToSource(index).row()));
    }
}

void MainWindow::clearPlayQueue()
{
    if (!Settings::self()->playQueueConfirmClear() ||
        MessageBox::Yes==MessageBox::questionYesNo(this, tr("Remove all songs from play queue?"))) {
        if (dynamicLabel->isVisible()) {
            DynamicPlaylists::self()->stop(true);
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

void MainWindow::appendToPlayQueue(int action, quint8 priority, bool decreasePriority)
{
    playQueueSearchWidget->clear();
    if (currentPage) {
        currentPage->addSelectionToPlaylist(QString(), action, priority, decreasePriority);
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
    bool decreasePriority=false;
    QModelIndexList pqItems;

    if (isPlayQueue) {
        pqItems=playQueue->selectedIndexes();
        if (pqItems.isEmpty()) {
            return;
        }
    }

    if (-1==prio) {
        InputDialog dlg(tr("Priority"), tr("Enter priority (0..255):"), 150, 0, 255, 5, this);
        QCheckBox *dec = new QCheckBox(tr("Decrease priority for each subsequent track"), this);
        dec->setChecked(false);
        dlg.addExtraWidget(dec);
        if (QDialog::Accepted!=dlg.exec()) {
            return;
        }
        prio=dlg.spinBox()->value();
        decreasePriority=dec->isChecked();
    }

    if (prio>=0 && prio<=255) {
        if (isPlayQueue) {
            QList<qint32> ids;
            for (const QModelIndex &idx: pqItems) {
                ids.append(PlayQueueModel::self()->getIdByRow(playQueueProxyModel.mapToSource(idx).row()));
            }
            emit setPriority(ids, prio, decreasePriority);
        } else {
            appendToPlayQueue(false, prio, decreasePriority);
        }
    }
}

void MainWindow::addToNewStoredPlaylist()
{
    bool pq=playQueue->hasFocus();
    for(;;) {
        QString name = InputDialog::getText(tr("Playlist Name"), tr("Enter a name for the playlist:"), QString(), nullptr, this);

        if (name==MPDConnection::constStreamsPlayListName) {
            MessageBox::error(this, tr("'%1' is used to store favorite streams, please choose another name.").arg(name));
            continue;
        }
        if (PlaylistsModel::self()->exists(name)) {
            switch(MessageBox::warningYesNoCancel(this, tr("A playlist named '%1' already exists!\n\nAdd to that playlist?").arg(name),
                                                  tr("Existing Playlist"))) {
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
            for (const QModelIndex &idx: items) {
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
        PlayQueueModel::self()->addItems(QStringList() << StreamsModel::modifyUrl(url, true, dlg.name()), MPDConnection::Append, 0, false);
    }
}

void MainWindow::addLocalFilesToPlayQueue()
{
    QString extensions;
    for (const auto &ext: PlayQueueModel::constFileExtensions) {
        if (!extensions.isEmpty()) {
            extensions+=" ";
        }
        extensions+="*"+ext;
    }
    QStringList files=QFileDialog::getOpenFileNames(this, tr("Select Music Files"), QString(), tr("Music Files ")+"("+extensions+")");
    if (!files.isEmpty()) {
        PlayQueueModel::self()->load(files);
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
        playQueueStatsLabel->setText(tr("%n Track(s)", "", songs));
    } else {
        playQueueStatsLabel->setText(tr("%n Tracks (%1)", "", songs).arg(Utils::formatDuration(time)));
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
    case PAGE_FOLDERS:   currentPage=folderPage;    break;
    case PAGE_PLAYLISTS: currentPage=playlistsPage; break;
    case PAGE_ONLINE:    currentPage=onlinePage;    break;
    #ifdef ENABLE_DEVICES_SUPPORT
    case PAGE_DEVICES:   currentPage=devicesPage;   break;
    #endif
    case PAGE_SEARCH:    currentPage=searchPage;    break;
    default:             currentPage=nullptr;       break;
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
        locateAction->setVisible(tabWidget->isEnabled(index));
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

void MainWindow::locateTrack()
{
    if (!locateAction->isVisible() || !locateAction->isEnabled()) {
        return;
    }
    Action *act = qobject_cast<Action *>(sender());
    if (!act) {
        return;
    }

    QList<Song> songs = playQueue->selectedSongs();
    if (1!=songs.count() || !tabWidget->isEnabled(PAGE_LIBRARY)) {
        return;
    }
    showLibraryTab();
    if (locateTrackAction==act) {
        libraryPage->showSongs(songs);
    }
    Song s = songs.first();
    if (locateAlbumAction==act) {
        libraryPage->showAlbum(s.albumArtist(), s.albumId());
    }
    if (locateArtistAction==act) {
        libraryPage->showArtist(s.albumArtist());
    }
}

void MainWindow::moveSelectionAfterCurrentSong()
{
    QList<int> selectedIdexes = playQueueProxyModel.mapToSourceRows(playQueue->selectedIndexes());

    if( !selectedIdexes.empty() ){
        QList<quint32> selectedSongIds;
        for (int row: selectedIdexes){
            selectedSongIds.append( (quint32) row);
        }

        int currentSongIdx = PlayQueueModel::self()->currentSongRow();

        emit playNext(selectedSongIds, currentSongIdx+1, PlayQueueModel::self()->rowCount());
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
    DynamicPlaylists::self()->helperMessage(message);
}

void MainWindow::setCollection(const QString &collection)
{
    if (!connectionsAction->isVisible()) {
        return;
    }
    for (QAction *act: connectionsAction->menu()->actions()) {
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
    for (QAction *act: connectionsAction->menu()->actions()) {
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
        if (currentPage==folderPage && folderPage->mpd()->isVisible()) {
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
            tt+=QLatin1String("<br/><i><small>")+s.artistSong()+QLatin1String("</small></i>");
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
    #if !defined Q_OS_MAC && !defined Q_OS_WIN
    return toolbar->height()+(showMenubarAction && menuBar() && menuBar()->isVisible() ? menuBar()->height() : 0);
    #else
    return toolbar->height();
    #endif
}

void MainWindow::setCollapsedSize()
{
    if (!expandInterfaceAction->isChecked()) {
        int w=width();
        adjustSize();
        collapsedSize=QSize(w, calcCollapsedSize());
        resize(collapsedSize);
        setFixedHeight(collapsedSize.height());
    }
}

void MainWindow::controlView(bool forceUpdate)
{
    if (!stopTrackButton || !coverWidget || !tabWidget) {
        controlPlayQueueButtons();
        return;
    }

    static int lastWidth=-1;

    if (forceUpdate || -1==lastWidth || qAbs(lastWidth-width())>20) {
        bool stopEnabled = stopTrackButton->property(constUserSettingProp).toBool();
        bool coverWidgetEnabled = coverWidget->property(constUserSettingProp).toBool();
        int tabWidgetStyle = tabWidget->property(constUserSetting2Prop).toInt();
        QStringList tabWidgetPages = tabWidget->property(constUserSettingProp).toStringList();

        if ((!stopEnabled && !coverWidgetEnabled) || width()<Utils::scaleForDpi(450)) {
            stopTrackButton->setVisible(false);
            coverWidget->setEnabled(false);
        } else if (stopEnabled && coverWidgetEnabled) {
            if (width()>Utils::scaleForDpi(550)) {
                stopTrackButton->setVisible(true);
                coverWidget->setEnabled(true);
            } else if (width()>Utils::scaleForDpi(450)) {
                stopTrackButton->setVisible(false);
                coverWidget->setEnabled(true);
            }
        } else {
            coverWidget->setEnabled(coverWidgetEnabled);
            stopTrackButton->setVisible(stopEnabled);
        }

        if (expandInterfaceAction->isChecked() && (responsiveSidebar || forceUpdate)) {
            if (!responsiveSidebar || width()>Utils::scaleForDpi(450)) {
                if (forceUpdate || singlePane) {
                    int index=tabWidget->currentIndex();
                    if (forceUpdate || tabWidget->style()!=tabWidgetStyle) {
                        tabWidget->setStyle(tabWidgetStyle);
                    }
                    tabWidget->setHiddenPages(tabWidgetPages);
                    tabWidget->setCurrentIndex(index);
                }
                singlePane=false;
            } else if (responsiveSidebar) {
                if (forceUpdate || !singlePane) {
                    int index=tabWidget->currentIndex();
                    int smallStyle=FancyTabWidget::Small|FancyTabWidget::Top|FancyTabWidget::IconOnly;
                    if (forceUpdate || tabWidget->style()!=smallStyle) {
                        tabWidget->setStyle(smallStyle);
                    }
                    tabWidgetPages.removeAll(QLatin1String("PlayQueuePage"));
                    tabWidgetPages.removeAll(QLatin1String("ContextPage"));
                    tabWidget->setHiddenPages(tabWidgetPages);

                    if (forceUpdate && !isVisible() && PAGE_PLAYQUEUE!=index && PAGE_CONTEXT!=index) {
                        QString page=Settings::self()->page();
                        for (int i=0; i<tabWidget->count(); ++i) {
                            if (tabWidget->widget(i)->metaObject()->className()==page) {
                                index=i;
                                break;
                            }
                        }
                    }

                    tabWidget->setCurrentIndex(index);
                }
                singlePane=true;
            }
        }
    }
    controlPlayQueueButtons();
}

void MainWindow::controlPlayQueueButtons()
{
    if (!savePlayQueueButton || !centerPlayQueueButton || !midSpacer) {
        return;
    }

    savePlayQueueButton->setVisible(singlePane || playQueueWidget->width()>Utils::scaleForDpi(320));
    centerPlayQueueButton->setVisible(singlePane || playQueueWidget->width()>(Utils::scaleForDpi(320)+centerPlayQueueButton->width()));
    midSpacer->setVisible(singlePane || centerPlayQueueButton->isVisible());
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
    adjustSize();

    if (showing) {
        resize(qMax(prevWidth, expandedSize.width()), expandedSize.height());
        if (lastMax) {
            showMaximized();
        }
        controlView();
    } else {
        // Width also sometimes expands, so make sure this is no larger than it was before...
        collapsedSize=QSize(collapsedSize.isValid() ? qMin(collapsedSize.width(), prevWidth) : prevWidth, calcCollapsedSize());
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

#include "moc_mainwindow.cpp"
