/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtCore/QTimer>
#include <QtCore/QProcess>
#include <QtCore/QPropertyAnimation>
#include <QtGui/QResizeEvent>
#include <QtGui/QMoveEvent>
#include <QtGui/QClipboard>
#include <QtGui/QProxyStyle>
#include <cstdlib>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KApplication>
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KNotification>
#include <KDE/KStandardAction>
#include <KDE/KStandardDirs>
#include <KDE/KAboutApplicationDialog>
#include <KDE/KLineEdit>
#include <KDE/KXMLGUIFactory>
#include <KDE/KMessageBox>
#include <KDE/KIcon>
#include <KDE/KMenuBar>
#include <KDE/KMenu>
#include <KDE/KStatusNotifierItem>
#include <KDE/KInputDialog>
#else
#include <QtGui/QInputDialog>
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
#include "config.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemalbum.h"
#include "librarypage.h"
#include "albumspage.h"
#include "folderpage.h"
#include "streamspage.h"
#include "lyricspage.h"
#include "infopage.h"
#include "serverinfopage.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicespage.h"
#include "devicesmodel.h"
#include "actiondialog.h"
#include "trackorganiser.h"
#endif
#ifdef TAGLIB_FOUND
#include "tageditor.h"
#endif
#include "streamsmodel.h"
#include "playlistspage.h"
#include "fancytabwidget.h"
#include "timeslider.h"
#include "mpris.h"
#include "dockmanager.h"
#include "messagewidget.h"
#include "debugtimer.h"

enum Tabs
{
    TAB_LIBRARY = 0x01,
    TAB_FOLDERS = 0x02,
    TAB_STREAMS = 0x04
};

class ProxyStyle : public QProxyStyle
{
public:
    ProxyStyle()
        : QProxyStyle()
    {
    }

    int styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *returnData) const
    {
        if(stylehint==QStyle::SH_Slider_AbsoluteSetButtons){
            return Qt::LeftButton|QProxyStyle::styleHint(stylehint,opt,widget,returnData);
        }else{
            return QProxyStyle::styleHint(stylehint,opt,widget,returnData);
        }
    }
};

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
            window->expandInterfaceAction->trigger();
        }
        pressed=false;
        break;
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

MainWindow::MainWindow(QWidget *parent)
#ifdef ENABLE_KDE_SUPPORT
    : KXmlGuiWindow(parent)
#else
    : QMainWindow(parent)
#endif
    , lastState(MPDStatus::State_Inactive)
    , lastSongId(-1)
    , fetchStatsFactor(0)
    , nowPlayingFactor(0)
    , draggingPositionSlider(false)
    , dock(0)
    , mpris(0)
    , playlistSearchTimer(0)
    , usingProxy(false)
    , isConnected(false)
    , volumeFade(0)
    , origVolume(0)
    , stopState(StopState_None)
{
    loaded=0;
    trayItem = 0;
    lyricsNeedUpdating=false;
    #ifdef ENABLE_WEBKIT
    infoNeedsUpdating=false;
    #endif

    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
    QMenu *mainMenu=new QMenu(this);

    messageWidget->hide();
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

    smallPlaybackButtonsAction = actionCollection()->addAction("smallplaybackbuttons");
    smallPlaybackButtonsAction->setText(i18n("Small Playback Buttons"));

    smallControlButtonsAction = actionCollection()->addAction("smallcontrolbuttons");
    smallControlButtonsAction->setText(i18n("Small Control Buttons"));

    refreshAction = actionCollection()->addAction("refresh");
    refreshAction->setText(i18n("Refresh Database"));

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
    addToStoredPlaylistAction->setText(i18n("Add To Playlist"));

    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = actionCollection()->addAction("copytodevice");
    copyToDeviceAction->setText(i18n("Copy To Device"));
    copyToDeviceAction->setIcon(QIcon::fromTheme("multimedia-player"));
    deleteSongsAction = actionCollection()->addAction("deletesongs");
    organiseFilesAction = actionCollection()->addAction("organizefiles");
    #endif

    removeAction = actionCollection()->addAction("removeitems");
    removeAction->setText(i18n("Remove"));

    replacePlaylistAction = actionCollection()->addAction("replaceplaylist");
    replacePlaylistAction->setText(i18n("Replace Play Queue"));

    removeFromPlaylistAction = actionCollection()->addAction("removefromplaylist");
    removeFromPlaylistAction->setText(i18n("Remove From Play Queue"));

    copyTrackInfoAction = actionCollection()->addAction("copytrackinfo");
    copyTrackInfoAction->setText(i18n("Copy Track Info"));
    copyTrackInfoAction->setShortcut(QKeySequence::Copy);

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

//     burnAction = actionCollection()->addAction("burn");
//     burnAction->setText(i18n("Burn To CD/DVD"));
//
//     createAudioCdAction = actionCollection()->addAction("createaudiocd");
//     createAudioCdAction->setText(i18n("Create Audio CD"));
//
//     createDataCdAction = actionCollection()->addAction("createdatacd");
//     createDataCdAction->setText(i18n("Create Data CD/DVD"));

    #ifdef TAGLIB_FOUND
    editTagsAction = actionCollection()->addAction("edittags");
    editTagsAction->setText(i18n("Edit Tags"));
    #endif

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

    #ifdef ENABLE_WEBKIT
    infoTabAction = actionCollection()->addAction("showinfotab");
    infoTabAction->setText(i18n("Info"));
    #endif // ENABLE_WEBKIT

    serverInfoTabAction = actionCollection()->addAction("showserverinfotab");
    serverInfoTabAction->setText(i18n("Server Info"));

    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction = actionCollection()->addAction("showdevicestab");
    devicesTabAction->setText(i18n("Devices"));
    #endif // ENABLE_DEVICES_SUPPORT

    #else
    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    quitAction->setIcon(QIcon::fromTheme("application-exit"));
    quitAction->setShortcut(QKeySequence::Quit);
    smallPlaybackButtonsAction = new QAction(tr("Small Playback Buttons"), this);
    smallControlButtonsAction = new QAction(tr("Small Control Buttons"), this);
    refreshAction = new QAction(tr("Refresh"), this);
    prevTrackAction = new QAction(tr("Previous Track"), this);
    nextTrackAction = new QAction(tr("Next Track"), this);
    playPauseTrackAction = new QAction(tr("Play/Pause"), this);
    stopTrackAction = new QAction(tr("Stop"), this);
    increaseVolumeAction = new QAction(tr("Increase Volume"), this);
    decreaseVolumeAction = new QAction(tr("Decrease Volume"), this);
    addToPlaylistAction = new QAction(tr("Add To Play Queue"), this);
    addToStoredPlaylistAction = new QAction(tr("Add To Playlist"), this);
    removeAction = new QAction(tr("Remove"), this);
    replacePlaylistAction = new QAction(tr("Replace Play Queue"), this);
    removeFromPlaylistAction = new QAction(tr("Remove From Play Queue"), this);
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
//     burnAction = new QAction(tr("Burn To CD/DVD"), this);
//     createAudioCdAction = new QAction(tr("Create Audio CD"), this);
//     createDataCdAction = new QAction(tr("Create Data CD"), this);
    #ifdef TAGLIB_FOUND
    editTagsAction = new QAction(tr("Edit Tags"), this);
    #endif
    libraryTabAction = new QAction(tr("Library"), this);
    albumsTabAction = new QAction(tr("Albums"), this);
    foldersTabAction = new QAction(tr("Folders"), this);
    playlistsTabAction = new QAction(tr("Playlists"), this);
    lyricsTabAction = new QAction(tr("Lyrics"), this);
    streamsTabAction = new QAction(tr("Streams"), this);
    #ifdef ENABLE_WEBKIT
    infoTabAction = new QAction(tr("Info"), this);
    #endif // ENABLE_WEBKIT
    serverInfoTabAction = new QAction(tr("Server Info"), this);
    #endif // ENABLE_KDE_SUPPORT
    libraryTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F1);
    albumsTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F2);
    foldersTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F3);
    playlistsTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F4);
    streamsTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F5);
    lyricsTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F6);
    #ifdef ENABLE_WEBKIT
    infoTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F7);
    serverInfoTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F8);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F9);
    #endif // ENABLE_DEVICES_SUPPORT
    #else // ENABLE_WEBKIT
    serverInfoTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F7);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F8);
    #endif // ENABLE_DEVICES_SUPPORT
    #endif // ENABLE_WEBKIT

    // Setup event handler for volume adjustment
    volumeSliderEventHandler = new VolumeSliderEventHandler(this);

    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(volumeSliderEventHandler);

    positionSlider->setStyle(new ProxyStyle());

    noCover = QIcon::fromTheme("media-optical-audio").pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    noStreamCover = QIcon::fromTheme("applications-internet").pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverWidget->setPixmap(noCover);

    playbackPlay = QIcon::fromTheme("media-playback-start");
    playbackPause = QIcon::fromTheme("media-playback-pause");
    repeatPlaylistAction->setIcon(QIcon::fromTheme("cantata-view-media-repeat"));
    randomPlaylistAction->setIcon(QIcon::fromTheme("media-playlist-shuffle"));
    consumePlaylistAction->setIcon(QIcon::fromTheme("cantata-view-media-consume"));
    removeAction->setIcon(QIcon::fromTheme("list-remove"));
    addToPlaylistAction->setIcon(QIcon::fromTheme("list-add"));
    replacePlaylistAction->setIcon(QIcon::fromTheme("media-playback-start"));

//     burnAction->setIcon(QIcon::fromTheme("tools-media-optical-burn"));
//     createDataCdAction->setIcon(QIcon::fromTheme("media-optical"));
//     createAudioCdAction->setIcon(QIcon::fromTheme("media-optical-audio"));
    #ifdef TAGLIB_FOUND
    editTagsAction->setIcon(QIcon::fromTheme("document-edit"));
    #endif
//     QMenu *cdMenu=new QMenu(this);
//     cdMenu->addAction(createAudioCdAction);
//     cdMenu->addAction(createDataCdAction);
//     burnAction->setMenu(cdMenu);
//     #ifdef ENABLE_KDE_SUPPORT
//     if (KStandardDirs::findExe("k3b").isEmpty()) {
//         burnAction->setVisible(false);
//     }
//     #endif

    prevTrackAction->setIcon(QIcon::fromTheme("media-skip-backward"));
    nextTrackAction->setIcon(QIcon::fromTheme("media-skip-forward"));
    playPauseTrackAction->setIcon(playbackPlay);
    stopTrackAction->setIcon(QIcon::fromTheme("media-playback-stop"));
    removeFromPlaylistAction->setIcon(QIcon::fromTheme("list-remove"));
    clearPlaylistAction->setIcon(QIcon::fromTheme("edit-clear-list"));
    savePlaylistAction->setIcon(QIcon::fromTheme("document-save-as"));
    clearPlaylistAction->setIcon(QIcon::fromTheme("edit-clear-list"));
    expandInterfaceAction->setIcon(QIcon::fromTheme("view-media-playlist"));
    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    libraryTabAction->setIcon(QIcon::fromTheme("audio-ac3"));
    albumsTabAction->setIcon(QIcon::fromTheme("media-optical-audio"));
    foldersTabAction->setIcon(QIcon::fromTheme("inode-directory"));
    playlistsTabAction->setIcon(QIcon::fromTheme("view-media-playlist"));
    lyricsTabAction->setIcon(QIcon::fromTheme("view-media-lyrics"));
    streamsTabAction->setIcon(QIcon::fromTheme("applications-internet"));
    #ifdef ENABLE_WEBKIT
    infoTabAction->setIcon(QIcon::fromTheme("dialog-information"));
    #endif
    serverInfoTabAction->setIcon(QIcon::fromTheme("server-database"));
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setIcon(QIcon::fromTheme("multimedia-player"));
    copyToDeviceAction->setMenu(DevicesModel::self()->menu());
    deleteSongsAction->setIcon(QIcon::fromTheme("edit-delete"));
    deleteSongsAction->setText(i18n("Delete Songs"));
    organiseFilesAction->setIcon(QIcon::fromTheme("inode-directory"));
    organiseFilesAction->setText(i18n("Organize Files"));
    #endif
    addToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());
    addToStoredPlaylistAction->setIcon(playlistsTabAction->icon());

    menuButton->setIcon(QIcon::fromTheme("configure"));
    volumeButton->setIcon(QIcon::fromTheme("player-volume"));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &, const QString &)),
            SLOT(cover(const QString &, const QString &, const QImage &, const QString &)));

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
    #ifdef ENABLE_WEBKIT
    infoPage = new InfoPage(this);
    #endif
    serverInfoPage = new ServerInfoPage(this);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage = new DevicesPage(this);
    #endif

    connect(MusicLibraryModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), albumsPage, SLOT(updateGenres(const QSet<QString> &)));

    setVisible(true);

    savePlaylistPushButton->setDefaultAction(savePlaylistAction);
    removeAllFromPlaylistPushButton->setDefaultAction(clearPlaylistAction);
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
    #ifdef ENABLE_WEBKIT
    tabWidget->AddTab(infoPage, infoTabAction->icon(), infoTabAction->text(), !hiddenPages.contains(infoPage->metaObject()->className()));
    #endif
    tabWidget->AddTab(serverInfoPage, serverInfoTabAction->icon(), serverInfoTabAction->text(), !hiddenPages.contains(serverInfoPage->metaObject()->className()));
    #ifdef ENABLE_DEVICES_SUPPORT
    tabWidget->AddTab(devicesPage, devicesTabAction->icon(), devicesTabAction->text(), !hiddenPages.contains(devicesPage->metaObject()->className()));
    DevicesModel::self()->setEnabled(!hiddenPages.contains(devicesPage->metaObject()->className()));
    copyToDeviceAction->setVisible(DevicesModel::self()->isEnabled());
    #endif
    AlbumsModel::self()->setEnabled(!hiddenPages.contains(albumsPage->metaObject()->className()));
    folderPage->setEnabled(!hiddenPages.contains(folderPage->metaObject()->className()));
    streamsPage->setEnabled(!hiddenPages.contains(streamsPage->metaObject()->className()));

    tabWidget->SetMode(FancyTabWidget::Mode_LargeSidebar);

    expandInterfaceAction->setCheckable(true);
    randomPlaylistAction->setCheckable(true);
    repeatPlaylistAction->setCheckable(true);
    consumePlaylistAction->setCheckable(true);

    #ifdef ENABLE_KDE_SUPPORT
    searchPlaylistLineEdit->setPlaceholderText(i18n("Search Play Queue..."));
    #else
    searchPlaylistLineEdit->setPlaceholderText(tr("Search Play Queue..."));
    #endif
    QList<QToolButton *> playbackBtns;
    QList<QToolButton *> controlBtns;
    QList<QToolButton *> btns;
    playbackBtns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    controlBtns << volumeButton << menuButton;
    btns << repeatPushButton << randomPushButton << savePlaylistPushButton << removeAllFromPlaylistPushButton << consumePushButton;

    foreach (QToolButton *b, btns) {
        b->setAutoRaise(true);
    }

    smallControlButtonsAction->setCheckable(true);
    smallControlButtonsAction->setChecked(Settings::self()->smallControlButtons());
    controlBtnsMenu = new QMenu(this);
    controlBtnsMenu->addAction(smallControlButtonsAction);
    connect(smallControlButtonsAction, SIGNAL(triggered(bool)), SLOT(setControlButtonsSize(bool)));
    foreach (QToolButton *b, controlBtns) {
        b->setAutoRaise(true);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(controlButtonsMenu()));
    }

    smallPlaybackButtonsAction->setCheckable(true);
    smallPlaybackButtonsAction->setChecked(Settings::self()->smallPlaybackButtons());
    playbackBtnsMenu = new QMenu(this);
    playbackBtnsMenu->addAction(smallPlaybackButtonsAction);
    connect(smallPlaybackButtonsAction, SIGNAL(triggered(bool)), SLOT(setPlaybackButtonsSize(bool)));
    foreach (QToolButton *b, playbackBtns) {
        b->setAutoRaise(true);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(playbackButtonsMenu()));
    }

    setPlaybackButtonsSize(Settings::self()->smallPlaybackButtons());
    setControlButtonsSize(Settings::self()->smallControlButtons());

    trackLabel->setText(QString());
    artistLabel->setText(QString());

    expandInterfaceAction->setChecked(Settings::self()->showPlaylist());
    randomPlaylistAction->setChecked(false);
    repeatPlaylistAction->setChecked(false);
    consumePlaylistAction->setChecked(false);
//     burnAction->setEnabled(QDir(Settings::self()->mpdDir()).isReadable());
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction->setEnabled(QDir(Settings::self()->mpdDir()).isReadable());
    deleteSongsAction->setEnabled(copyToDeviceAction->isEnabled());
    deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    MusicLibraryItemAlbum::setShowDate(Settings::self()->libraryYear());
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

    coverWidget->installEventFilter(new CoverEventHandler(this));

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
    playQueue->setModel(&playQueueModel);
    playQueue->setAcceptDrops(true);
    playQueue->setDropIndicatorShown(true);
    playQueue->addAction(removeFromPlaylistAction);
    playQueue->addAction(clearPlaylistAction);
    playQueue->addAction(savePlaylistAction);
    playQueue->addAction(cropPlaylistAction);
    playQueue->addAction(shufflePlaylistAction);
    //playQueue->addAction(copyTrackInfoAction);
    playQueue->installEventFilter(new DeleteKeyEventHandler(playQueue, removeFromPlaylistAction));
    playQueue->setRootIsDecorated(false);
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playlistItemsSelected(bool)));
    setupPlaylistView();

    connect(MPDConnection::self(), SIGNAL(statsUpdated()), this, SLOT(updateStats()));
    connect(MPDConnection::self(), SIGNAL(statusUpdated()), this, SLOT(updateStatus())/*, Qt::DirectConnection*/);
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlaylist(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(refreshAction, SIGNAL(triggered(bool)), this, SLOT(refresh()));
    connect(refreshAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(update()));
    connect(prevTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToPrevious()));
    connect(nextTrackAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(goToNext()));
    connect(playPauseTrackAction, SIGNAL(triggered(bool)), this, SLOT(playPauseTrack()));
    connect(stopTrackAction, SIGNAL(triggered(bool)), this, SLOT(stopTrack()));
    connect(volumeControl, SIGNAL(valueChanged(int)), MPDConnection::self(), SLOT(setVolume(int)));
    connect(this, SIGNAL(setVolume(int)), MPDConnection::self(), SLOT(setVolume(int)));
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
    connect(removeAction, SIGNAL(activated()), this, SLOT(removeItems()));
    connect(addToPlaylistAction, SIGNAL(activated()), this, SLOT(addToPlaylist()));
    connect(replacePlaylistAction, SIGNAL(activated()), this, SLOT(replacePlaylist()));
    connect(removeFromPlaylistAction, SIGNAL(activated()), this, SLOT(removeFromPlaylist()));
    connect(clearPlaylistAction, SIGNAL(activated()), searchPlaylistLineEdit, SLOT(clear()));
    connect(clearPlaylistAction, SIGNAL(activated()), MPDConnection::self(), SLOT(clear()));
    connect(copyTrackInfoAction, SIGNAL(activated()), this, SLOT(copyTrackInfo()));
    connect(cropPlaylistAction, SIGNAL(activated()), this, SLOT(cropPlaylist()));
    connect(shufflePlaylistAction, SIGNAL(activated()), MPDConnection::self(), SLOT(shuffle()));
    connect(expandInterfaceAction, SIGNAL(activated()), this, SLOT(togglePlaylist()));
    connect(positionSlider, SIGNAL(valueChanged(int)), this, SLOT(updatePosition()));
    connect(volumeButton, SIGNAL(clicked()), SLOT(showVolumeControl()));
//     connect(createDataCdAction, SIGNAL(activated()), this, SLOT(createDataCd()));
//     connect(createAudioCdAction, SIGNAL(activated()), this, SLOT(createAudioCd()));
    #ifdef TAGLIB_FOUND
    connect(editTagsAction, SIGNAL(activated()), this, SLOT(editTags()));
    #endif
    connect(libraryTabAction, SIGNAL(activated()), this, SLOT(showLibraryTab()));
    connect(albumsTabAction, SIGNAL(activated()), this, SLOT(showAlbumsTab()));
    connect(foldersTabAction, SIGNAL(activated()), this, SLOT(showFoldersTab()));
    connect(playlistsTabAction, SIGNAL(activated()), this, SLOT(showPlaylistsTab()));
    connect(lyricsTabAction, SIGNAL(activated()), this, SLOT(showLyricsTab()));
    connect(streamsTabAction, SIGNAL(activated()), this, SLOT(showStreamsTab()));
    #ifdef ENABLE_WEBKIT
    connect(infoTabAction, SIGNAL(activated()), this, SLOT(showInfoTab()));
    #endif
    connect(serverInfoTabAction, SIGNAL(activated()), this, SLOT(showServerInfoTab()));
    #ifdef ENABLE_DEVICES_SUPPORT
    connect(devicesTabAction, SIGNAL(activated()), this, SLOT(showDevicesTab()));
    connect(DevicesModel::self(), SIGNAL(addToDevice(const QString &)), this, SLOT(addToDevice(const QString &)));
    connect(libraryPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(deleteSongsAction, SIGNAL(triggered()), SLOT(deleteSongs()));
    connect(organiseFilesAction, SIGNAL(triggered()), SLOT(organiseFiles()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    #endif
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
    connect(playlistsPage, SIGNAL(add(const QStringList &)), &playQueueModel, SLOT(addItems(const QStringList &)));

    QByteArray state=Settings::self()->splitterState();

    if (state.isEmpty()) {
        QList<int> sizes;
        sizes << 250 << 500;
        splitter->setSizes(sizes);
        resize(800, 600);
    } else {
        splitter->restoreState(Settings::self()->splitterState());
    }

    playlistItemsSelected(false);
    playQueue->setFocus();

    mpdThread=new QThread(this);
    MPDConnection::self()->moveToThread(mpdThread);
    mpdThread->start();
    emit setDetails(Settings::self()->connectionHost(), Settings::self()->connectionPort(), Settings::self()->connectionPasswd());

    QString page=Settings::self()->page();

    for (int i=0; i<tabWidget->count(); ++i) {
        if (tabWidget->widget(i)->metaObject()->className()==page) {
            tabWidget->SetCurrentIndex(i);
            break;
        }
    }

    connect(tabWidget, SIGNAL(CurrentChanged(int)), this, SLOT(currentTabChanged(int)));
    connect(tabWidget, SIGNAL(TabToggled(int)), this, SLOT(tabToggled(int)));

    libraryPage->setView(0==Settings::self()->libraryView());
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    albumsPage->setView(Settings::self()->albumsView());
    AlbumsModel::setUseLibrarySizes(Settings::self()->albumsView()!=ItemView::Mode_IconTop);
    playlistsPage->setView(0==Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(0==Settings::self()->devicesView());
    #endif

    currentTabChanged(tabWidget->current_index());
    fadeStop=Settings::self()->stopFadeDuration()>Settings::MinFade;
    playlistsPage->refresh();
    toggleMpris();
    toggleDockManager();
}

struct Thread : public QThread
{
    static void sleep() { QThread::msleep(100); }
};

MainWindow::~MainWindow()
{
    if (dock) {
        dock->setIcon(QString());
    }
    #ifndef ENABLE_KDE_SUPPORT
    Settings::self()->saveMainWindowSize(size());
    #endif
    Settings::self()->saveShowPlaylist(expandInterfaceAction->isChecked());
    Settings::self()->saveSplitterState(splitter->saveState());
    Settings::self()->savePlayQueueHeaderState(playQueueHeader->saveState());
    Settings::self()->saveSidebar((int)(tabWidget->mode()));
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    Settings::self()->saveSmallPlaybackButtons(smallPlaybackButtonsAction->isChecked());
    Settings::self()->saveSmallControlButtons(smallControlButtonsAction->isChecked());
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

void MainWindow::playbackButtonsMenu()
{
    playbackBtnsMenu->exec(QCursor::pos());
}

void MainWindow::controlButtonsMenu()
{
    controlBtnsMenu->exec(QCursor::pos());
}

void MainWindow::setPlaybackButtonsSize(bool small)
{
    QList<QToolButton *> playbackBtns;
    playbackBtns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    foreach (QToolButton *b, playbackBtns) {
        b->setIconSize(small ? QSize(22, 22) : QSize(28, 28));
    }
}

void MainWindow::setControlButtonsSize(bool small)
{
    QList<QToolButton *> controlBtns;
    controlBtns << volumeButton << menuButton;

    foreach (QToolButton *b, controlBtns) {
        b->setIconSize(small ? QSize(18, 18) : QSize(22, 22));
    }
}

void MainWindow::songLoaded()
{
    if (MPDStatus::State_Stopped==MPDStatus::self()->state()) {
        stopVolumeFade();
        emit play();
    }
}

void MainWindow::showError(const QString &message)
{
    messageWidget->setError(message);
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    if (connected==isConnected) {
        return;
    }
    isConnected=connected;
    if (connected) {
        emit getStatus();
        emit getStats();
        emit playListInfo();
        loaded=0;
//         currentTabChanged(tabWidget->current_index());
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
    if (trayItem) {
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

void MainWindow::refresh()
{
    MusicLibraryModel::self()->removeCache();
    lastDbUpdate=QDateTime();
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
    int stopFadeDuration=Settings::self()->stopFadeDuration();
    fadeStop=stopFadeDuration>Settings::MinFade;
    if (volumeFade) {
        volumeFade->setDuration(stopFadeDuration);
    }

    emit setDetails(Settings::self()->connectionHost(), Settings::self()->connectionPort(), Settings::self()->connectionPasswd());
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction->setEnabled(QDir(Settings::self()->mpdDir()).isReadable());
    deleteSongsAction->setEnabled(copyToDeviceAction->isEnabled());
    organiseFilesAction->setEnabled(copyToDeviceAction->isEnabled());
//     burnAction->setEnabled(copyToDeviceAction->isEnabled());
    deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    Settings::self()->save();
    bool useLibSizeForAl=Settings::self()->albumsView()!=ItemView::Mode_IconTop;
    bool diffLibCovers=((int)MusicLibraryItemAlbum::currentCoverSize())!=Settings::self()->libraryCoverSize();
    bool diffAlCovers=((int)AlbumsModel::currentCoverSize())!=Settings::self()->albumsCoverSize() ||
                      albumsPage->viewMode()!=Settings::self()->albumsView() ||
                      useLibSizeForAl!=AlbumsModel::useLibrarySizes();
    bool diffLibYear=MusicLibraryItemAlbum::showDate()!=Settings::self()->libraryYear();
    bool diffSingleGroup=MPDParseUtils::groupSingle()!=Settings::self()->groupSingle();

    if (diffLibCovers) {
        MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    }
    if (diffLibYear) {
        MusicLibraryItemAlbum::setShowDate(Settings::self()->libraryYear());
    }
    if (diffAlCovers) {
        AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());
    }
    if (diffSingleGroup) {
        MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    }

    AlbumsModel::setUseLibrarySizes(useLibSizeForAl);
    albumsPage->setView(Settings::self()->albumsView());
    if (diffAlCovers || diffSingleGroup) {
        albumsPage->clear();
    }
    if (diffLibCovers || diffAlCovers || diffLibYear || diffSingleGroup) {
        refresh();
    }

    libraryPage->setView(0==Settings::self()->libraryView());
    playlistsPage->setView(0==Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(0==Settings::self()->devicesView());
    #endif
    setupTrayIcon();
    toggleDockManager();
    toggleMpris();
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
    if (!fadeStop) {
        emit stop();
    }
    stopTrackAction->setEnabled(false);
    startVolumeFade(/*true*/);
}

void MainWindow::startVolumeFade(/*bool stop*/)
{
    if (!fadeStop) {
        return;
    }

    stopState=/*stop ? */StopState_Stopping/* : StopState_Pausing*/;
    if (!volumeFade) {
        volumeFade = new QPropertyAnimation(this, "volume");
        volumeFade->setDuration(Settings::self()->stopFadeDuration());
    }
    origVolume=volume;
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
        if (StopState_Stopping==stopState) {
            emit stop();
        } /*else if (StopState_Pausing==stopState) {
            emit pause(true);
        }*/
        stopState=StopState_None;
        volume=origVolume;
        emit setVolume(origVolume);
    } else {
        emit setVolume(v);
    }
}

void MainWindow::playPauseTrack()
{
    MPDStatus * const status = MPDStatus::self();

    if (status->state() == MPDStatus::State_Playing) {
        /*if (fadeStop) {
            startVolumeFade(false);
        } else*/ {
            emit pause(true);
        }
    } else if (status->state() == MPDStatus::State_Paused) {
        stopVolumeFade();
        emit pause(false);
    } else {
        stopVolumeFade();
        if (-1!=playQueueModel.currentSong()) {
            emit startPlayingSongId(playQueueModel.currentSong());
        } else {
            emit play();
        }
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
        if (playlistSearchTimer) {
            playlistSearchTimer->stop();
        }
        realSearchPlaylist();
    } else {
        if (!playlistSearchTimer) {
            playlistSearchTimer=new QTimer(this);
            connect(playlistSearchTimer, SIGNAL(timeout()), SLOT(realSearchPlaylist()));
        }
        playlistSearchTimer->start(250);
    }
}

void MainWindow::realSearchPlaylist()
{
    QList<qint32> selectedSongIds;
    if (playlistSearchTimer) {
        playlistSearchTimer->stop();
    }
    QString filter=searchPlaylistLineEdit->text().trimmed();
    if (filter.length()<2) {
        if (usingProxy) {
            if (playQueue->selectionModel()->hasSelection()) {
                QModelIndexList items = playQueue->selectionModel()->selectedRows();
                foreach (const QModelIndex &index, items) {
                    selectedSongIds.append(playQueueModel.getIdByRow(playQueueProxyModel.mapToSource(index).row()));
                }
            }

            playQueue->setModel(&playQueueModel);
            usingProxy=false;
        }
        playQueueProxyModel.setFilterEnabled(false);
        if (!playQueueProxyModel.filterRegExp().isEmpty()) {
            playQueueProxyModel.setFilterRegExp(QString());
        }
    } else if (filter!=playQueueProxyModel.filterRegExp().pattern()) {
        if (!usingProxy) {
            if (playQueue->selectionModel()->hasSelection()) {
                QModelIndexList items = playQueue->selectionModel()->selectedRows();
                foreach (const QModelIndex &index, items) {
                    selectedSongIds.append(playQueueModel.getIdByRow(index.row()));
                }
            }
            playQueue->setModel(&playQueueProxyModel);
            usingProxy=true;
        }
        playQueueProxyModel.setFilterEnabled(true);
        playQueueProxyModel.setFilterRegExp(filter);
    }

    if (selectedSongIds.size() > 0) {
        foreach (qint32 i, selectedSongIds) {
            qint32 row = playQueueModel.getRowById(i);
            playQueue->selectionModel()->select(usingProxy ? playQueueProxyModel.mapFromSource(playQueueModel.index(row, 0))
                                                           : playQueueModel.index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
}

void MainWindow::updatePlaylist(const QList<Song> &songs)
{
    TF_DEBUG

    playPauseTrackAction->setEnabled(0!=songs.count());
    nextTrackAction->setEnabled(songs.count()>1);
    prevTrackAction->setEnabled(songs.count()>1);
    if (0==songs.count()) {
        updateCurrentSong(Song());
    }

    QList<qint32> selectedSongIds;
    qint32 firstSelectedSongId = -1;
    qint32 firstSelectedRow = -1;

    // remember selected song ids and rownum of smallest selected row in proxymodel (because that represents the visible rows)
    if (playQueue->selectionModel()->hasSelection()) {
        QModelIndexList items = playQueue->selectionModel()->selectedRows();
        // find smallest selected rownum
        foreach (const QModelIndex &index, items) {
            QModelIndex sourceIndex = usingProxy ? playQueueProxyModel.mapToSource(index) : index;
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
    if (selectedSongIds.size() > 0) {
        bool found =  false;
        qint32 newCurrentRow = playQueueModel.getRowById(firstSelectedSongId);
        playQueue->setCurrentIndex(usingProxy ? playQueueProxyModel.mapFromSource(playQueueModel.index(newCurrentRow, 0)) : playQueueModel.index(newCurrentRow, 0));

        foreach (qint32 i, selectedSongIds) {
            qint32 row = playQueueModel.getRowById(i);
            if (row >= 0) {
                found = true;
            }

            playQueue->selectionModel()->select(usingProxy ? playQueueProxyModel.mapFromSource(playQueueModel.index(row, 0))
                                                           : playQueueModel.index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }

        if (!found) {
            // songids which were selected before the playlistupdate were not found anymore (they were removed) -> select firstSelectedRow again (should be the next song after the removed one)
            // firstSelectedRow contains the selected row of the proxymodel before we did the playlist refresh
            // check if rowcount of current proxymodel is smaller than that (last row as removed) and adjust firstSelectedRow when needed
            QModelIndex index;

            if (usingProxy) {
                index = playQueueProxyModel.index(firstSelectedRow, 0);
                if (firstSelectedRow > playQueueProxyModel.rowCount(index) - 1) {
                    firstSelectedRow = playQueueProxyModel.rowCount(index) - 1;
                }
                index = playQueueProxyModel.index(firstSelectedRow, 0);
            } else {
                index = playQueueModel.index(firstSelectedRow, 0);
                if (firstSelectedRow > playQueueModel.rowCount(index) - 1) {
                    firstSelectedRow = playQueueModel.rowCount(index) - 1;
                }
                index = playQueueProxyModel.index(firstSelectedRow, 0);
            }
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
    if (fadeStop && StopState_None!=stopState) {
        if (StopState_Stopping==stopState) {
            emit stop();
        } /*else if (StopState_Pausing==stopState) {
            emit pause(true);
        }*/
    }

    current=song;

    positionSlider->setEnabled(!currentIsStream());

    // Determine if album cover should be updated
    const QString &albumArtist=song.albumArtist();
    if (coverWidget->property("artist").toString() != albumArtist || coverWidget->property("album").toString() != song.album) {
        if (!albumArtist.isEmpty() && !song.album.isEmpty()) {
            Covers::self()->get(song, MPDParseUtils::groupSingle() && MusicLibraryModel::self()->isFromSingleTracks(song));
        } else if (!currentIsStream()) {
            coverWidget->setPixmap(noCover);
        }
        coverWidget->setProperty("artist", albumArtist);
        coverWidget->setProperty("album", song.album);
    }

    if (song.name.isEmpty()) {
        trackLabel->setText(song.title);
    } else {
        trackLabel->setText(QString("%1 (%2)").arg(song.title).arg(song.name));
    }
    if (song.album.isEmpty()) {
        artistLabel->setText(song.artist);
    } else {
        QString album=song.album;

        if (song.year>0) {
            album+=QString(" (%1)").arg(song.year);
        }
        #ifdef ENABLE_KDE_SUPPORT
        artistLabel->setText(i18nc("artist - album", "%1 - %2", song.artist, album));
        #else
        artistLabel->setText(tr("%1 - %2").arg(song.artist).arg(album));
        #endif
    }

    playQueueModel.updateCurrentSong(song.id);

    if (song.artist.isEmpty()) {
        if (trackLabel->text().isEmpty()) {
            setWindowTitle("Cantata");
        } else {
            #ifdef ENABLE_KDE_SUPPORT
            setWindowTitle(i18nc("track :: Cantata", "%1 :: Cantata", trackLabel->text()));
            #else
            setWindowTitle(tr("%1 :: Cantata").arg(trackLabel->text()));
            #endif
        }
    } else {
        #ifdef ENABLE_KDE_SUPPORT
        setWindowTitle(i18nc("track - artist :: Cantata", "%1 - %2 :: Cantata", trackLabel->text(), song.artist));
        #else
        setWindowTitle(tr("%1 - %2 :: Cantata").arg(trackLabel->text()).arg(song.artist));
        #endif
    }
    if (PAGE_LYRICS==tabWidget->current_index()) {
        lyricsPage->update(song);
    } else {
        lyricsNeedUpdating=true;
    }

    #ifdef ENABLE_WEBKIT
    if (PAGE_INFO==tabWidget->current_index()) {
        infoPage->update(song);
    } else {
        infoNeedsUpdating=true;
    }
    #endif

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
            if (!coverSong.album.isEmpty () && coverSong.albumArtist()==current.albumArtist() &&
                coverSong.album==current.album && coverWidget->pixmap()) {
                notification->setPixmap(*(coverWidget->pixmap()));
            }
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
    if (!lastDbUpdate.isValid() || MPDStats::self()->dbUpdate() > lastDbUpdate) {
        loaded|=TAB_LIBRARY|TAB_FOLDERS;
        if (!lastDbUpdate.isValid()) {
            libraryPage->clear();
            albumsPage->clear();
            folderPage->clear();
            playlistsPage->clear();
        }
        libraryPage->refresh();
        folderPage->refresh();
        playlistsPage->refresh();
    }

    lastDbUpdate = MPDStats::self()->dbUpdate();
}

void MainWindow::updateStatus()
{
    MPDStatus * const status = MPDStatus::self();

    if (!draggingPositionSlider) {
        if (MPDStatus::State_Stopped==status->state() || MPDStatus::State_Inactive==status->state()) {
            positionSlider->setValue(0);
        } else {
            positionSlider->setRange(0, status->timeTotal());
            positionSlider->setValue(status->timeElapsed());
        }
    }

    if (!stopState) {
        volume=status->volume();
        #ifdef ENABLE_KDE_SUPPORT
        volumeButton->setToolTip(i18n("Volume %1%").arg(volume));
        volumeControl->setToolTip(i18n("Volume %1%").arg(volume));
        #else
        volumeButton->setToolTip(tr("Volume %1%").arg(volume));
        volumeControl->setToolTip(tr("Volume %1%").arg(volume));
        #endif
        volumeButton->setIcon(QIcon::fromTheme("player-volume"));
        volumeControl->setValue(volume);

        if (0==volume) {
            volumeButton->setIcon(QIcon::fromTheme("audio-volume-muted"));
        } else if (volume<=33) {
            volumeButton->setIcon(QIcon::fromTheme("audio-volume-low"));
        } else if (volume<=67) {
            volumeButton->setIcon(QIcon::fromTheme("audio-volume-medium"));
        } else {
            volumeButton->setIcon(QIcon::fromTheme("audio-volume-high"));
        }
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
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        //playPauseTrackButton->setChecked(false);
        stopTrackAction->setEnabled(true);
        positionSlider->startTimer();

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
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        stopTrackAction->setEnabled(false);
        trackLabel->setText(QString());
        artistLabel->setText(QString());
        setWindowTitle("Cantata");
        current.id=0;

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("cantata");
            #else
            trayItem->setIcon(windowIcon());
            #endif
        }

        positionSlider->stopTimer();
        break;
    case MPDStatus::State_Paused:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        stopTrackAction->setEnabled(0!=playQueueModel.rowCount());

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("media-playback-pause");
            #else
            trayItem->setIcon(playbackPause);
            #endif
        }

        positionSlider->stopTimer();
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
    emit startPlayingSongId(playQueueModel.getIdByRow(usingProxy ? playQueueProxyModel.mapToSource(index).row() : index.row()));
}

void MainWindow::removeFromPlaylist()
{
    const QModelIndexList items = playQueue->selectionModel()->selectedRows();
    QModelIndex sourceIndex;
    QList<qint32> toBeRemoved;

    if (items.isEmpty()) {
        return;
    }

    foreach (const QModelIndex &idx, items) {
        toBeRemoved.append(playQueueModel.getIdByRow(usingProxy ? playQueueProxyModel.mapToSource(idx).row() : idx.row()));
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
        QString name = KInputDialog::getText(i18n("Playlist Name"), i18n("Enter a name for the playlist:"), QString(), 0, this);
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

void MainWindow::removeItems()
{
    if (playlistsPage->isVisible()) {
        playlistsPage->removeItems();
    } else if (streamsPage->isVisible()) {
        streamsPage->removeItems();
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
    status += QString::number(stats->playlistArtists())+QString(1==stats->playlistArtists() ? tr(" Artist,") : tr(" Artists, "));
    status += QString::number(stats->playlistAlbums())+QString(1==stats->playlistAlbums() ? tr(" Album,") : tr(" Albums, "));
    status += QString::number(stats->playlistSongs())+QString(1==stats->playlistSongs() ? tr(" Track") : tr(" Tracks"));
    #endif
    status += " (";
    status += MPDParseUtils::formatDuration(stats->playlistTime());
    status += ")";

    playListStatsLabel->setText(status);
}

void MainWindow::updatePosition()
{
    QString timeElapsedFormattedString;

    if (positionSlider->value() != positionSlider->maximum()) {
        timeElapsedFormattedString += Song::formattedTime(positionSlider->value());
        timeElapsedFormattedString += " / ";
        timeElapsedFormattedString += Song::formattedTime(songTime);
        songTimeElapsedLabel->setText(timeElapsedFormattedString);
    }
}

void MainWindow::copyTrackInfo()
{
    const QModelIndexList items = playQueue->selectionModel()->selectedRows();

    if (items.isEmpty()) {
        return;
    }

    QString txt;
    QTextStream str(&txt);

    foreach (const QModelIndex &idx, items) {
        Song s = playQueueModel.getSongByRow(usingProxy ? playQueueProxyModel.mapToSource(idx).row() : idx.row());
        if (!s.isEmpty()) {
            if (!txt.isEmpty()) {
                str << QChar('\n');
            }
            str << s.format();
        }
    }
    QApplication::clipboard()->setText(txt);
}

void MainWindow::togglePlaylist()
{
    if (splitter->isVisible()==expandInterfaceAction->isChecked()) {
        return;
    }
    static QSize lastSize;

    bool showing=expandInterfaceAction->isChecked();

    if (!showing) {
        lastSize=size();
    } else {
        setMinimumHeight(0);
        setMaximumHeight(65535);
    }
    splitter->setVisible(showing);
    if (!showing) {
        setWindowState(windowState()&~Qt::WindowMaximized);
    }
    QApplication::processEvents();
    adjustSize();

    bool adjustWidth=showing && size().width()<lastSize.width();
    bool adjustHeight=showing && size().height()<lastSize.height();
    if (adjustWidth || adjustHeight) {
        resize(adjustWidth ? lastSize.width() : size().width(), adjustHeight ? lastSize.height() : size().height());
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
    playQueueHeader->setResizeMode(QHeaderView::Interactive);
    playQueueHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    playQueueHeader->resizeSection(PlayQueueModel::COL_STATUS, 20);
    playQueueHeader->resizeSection(PlayQueueModel::COL_TRACK, QFontMetrics(playQueue->font()).width("999"));
    playQueueHeader->setResizeMode(PlayQueueModel::COL_STATUS, QHeaderView::Fixed);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_TITLE, QHeaderView::Interactive);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_ARTIST, QHeaderView::Interactive);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_ALBUM, QHeaderView::Stretch);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_TRACK, QHeaderView::Fixed);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_LENGTH, QHeaderView::ResizeToContents);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_DISC, QHeaderView::ResizeToContents);
    playQueueHeader->setResizeMode(PlayQueueModel::COL_YEAR, QHeaderView::ResizeToContents);
    playQueueHeader->setStretchLastSection(false);

    connect(playQueueHeader, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(playQueueContextMenuClicked()));
    connect(streamsPage, SIGNAL(add(const QStringList &)), &playQueueModel, SLOT(addItems(const QStringList &)));

    //Restore state
    QByteArray state;

    if (Settings::self()->version()>=CANTATA_MAKE_VERSION(0, 4, 0)) {
        state=Settings::self()->playQueueHeaderState();
    }

    QList<int> hideAble;
    hideAble << PlayQueueModel::COL_TRACK << PlayQueueModel::COL_ALBUM << PlayQueueModel::COL_LENGTH
             << PlayQueueModel::COL_DISC << PlayQueueModel::COL_YEAR << PlayQueueModel::COL_GENRE;

    //Restore
    if (state.isEmpty()) {
        playQueueHeader->setSectionHidden(PlayQueueModel::COL_YEAR, true);
        playQueueHeader->setSectionHidden(PlayQueueModel::COL_DISC, true);
        playQueueHeader->setSectionHidden(PlayQueueModel::COL_GENRE, true);
    } else {
        playQueueHeader->restoreState(state);

        foreach (int col, hideAble) {
            if (playQueueHeader->isSectionHidden(col) || 0==playQueueHeader->sectionSize(col)) {
                playQueueHeader->setSectionHidden(col, true);
            }
        }
    }

    playQueueMenu = new QMenu(this);

    foreach (int col, hideAble) {
        QString text=PlayQueueModel::COL_TRACK==col
                        ?
                            #ifdef ENABLE_KDE_SUPPORT
                            i18n("Track")
                            #else
                            tr("Track")
                            #endif
                        : playQueueModel.headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction *act=new QAction(text, playQueueMenu);
        act->setCheckable(true);
        act->setChecked(!playQueueHeader->isSectionHidden(col));
        playQueueMenu->addAction(act);
        act->setData(col);
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
        int index=act->data().toInt();
        if (-1!=index) {
            playQueueHeader->setSectionHidden(index, !visible);
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

    if (items.isEmpty()) {
        return;
    }

    foreach (const QModelIndex &idx, items) {
        selected << playQueueModel.getIdByRow(usingProxy ? playQueueProxyModel.mapToSource(idx).row() : idx.row());
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

    if (trayItem) {
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
    connect(trayItem, SIGNAL(secondaryActivateRequested(const QPoint &)), this, SLOT(playPauseTrack()));
    #else
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        trayItem = NULL;
        return;
    }

    trayItem = new QSystemTrayIcon(this);
    trayItem->installEventFilter(volumeSliderEventHandler);
    trayItemMenu = new QMenu(this);
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

void MainWindow::currentTabChanged(int index)
{
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
            libraryPage->refresh();
        }
        if (PAGE_LIBRARY==index) {
             libraryPage->controlActions();
        } else {
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
    case PAGE_STREAMS:
        if (!(loaded&TAB_STREAMS)) {
            loaded|=TAB_STREAMS;
            streamsPage->refresh();
        }
        streamsPage->controlActions();
        break;
    case PAGE_LYRICS:
        if (lyricsNeedUpdating) {
            lyricsPage->update(current);
            lyricsNeedUpdating=false;
        }
        break;
    #ifdef ENABLE_WEBKIT
    case PAGE_INFO:
        if (infoNeedsUpdating) {
            infoPage->update(current);
            infoNeedsUpdating=false;
        }
        break;
    #endif
    case PAGE_SERVER_INFO:
        break;
    default:
        break;
    }
}

void MainWindow::tabToggled(int index)
{
    switch (index) {
    case PAGE_ALBUMS:
        AlbumsModel::self()->setEnabled(!AlbumsModel::self()->isEnabled());
        break;
    case PAGE_FOLDERS:
        folderPage->setEnabled(!folderPage->isEnabled());
        break;
    case PAGE_STREAMS:
        streamsPage->setEnabled(!streamsPage->isEnabled());
        break;
    #ifdef ENABLE_DEVICES_SUPPORT
    case PAGE_DEVICES:
        DevicesModel::self()->setEnabled(!DevicesModel::self()->isEnabled());
        copyToDeviceAction->setVisible(DevicesModel::self()->isEnabled());
        break;
    #endif
    default:
        break;
    }
}

bool MainWindow::currentIsStream()
{
    return !current.title.isEmpty() && (current.file.isEmpty() || current.file.contains("://"));
}

void MainWindow::cover(const QString &artist, const QString &album, const QImage &img, const QString &file)
{
    if (artist==current.albumArtist() && album==current.album) {
        if (img.isNull()) {
            coverSong=Song();
            coverWidget->setPixmap(currentIsStream() ? noStreamCover : noCover);
            emit coverFile(QString());
        } else {
            coverSong=current;
            coverWidget->setPixmap(QPixmap::fromImage(img).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            emit coverFile(file);
        }
    }
}

void MainWindow::showTab(int page)
{
    tabWidget->SetCurrentIndex(page);
}

void MainWindow::toggleMpris()
{
    bool on=Settings::self()->mpris();
    if (on) {
        if (!mpris) {
            mpris=new Mpris(this);
        }
    } else {
        if (mpris) {
            mpris->deleteLater();
            mpris=0;
        }
    }
}

void MainWindow::toggleDockManager()
{
    if (!dock) {
        dock=new DockManager(this);
        connect(this, SIGNAL(coverFile(const QString &)), dock, SLOT(setIcon(const QString &)));
    }

    dock->setEnabled(Settings::self()->dockManager());
}

// void MainWindow::createDataCd()
// {
//     callK3b("data");
// }
//
// void MainWindow::createAudioCd()
// {
//     callK3b("audiocd");
// }
//
// void MainWindow::callK3b(const QString &type)
// {
//     QStringList files;
//     if (libraryPage->isVisible()) {
//         files=libraryPage->selectedFiles();
//     } else if (albumsPage->isVisible()) {
//         files=albumsPage->selectedFiles();
//     } else if (folderPage->isVisible()) {
//         files=folderPage->selectedFiles();
//     } else if (playlistsPage->isVisible()) {
//         files=playlistsPage->selectedFiles();
//     }
// #ifdef ENABLE_DEVICES_SUPPORT
//     else if (devicesPage->isVisible()) {
//         QList<Song> songs=devicesPage->selectedSongs();
//         foreach (const Song &s, songs) {
//             files.append(s.file);
//         }
//     }
// #endif
//
//     if (!files.isEmpty()) {
//         QStringList args;
//         args << QLatin1String("--")+type;
//         foreach (const QString &f, files) {
//             args << Settings::self()->mpdDir()+f;
//         }
//
//         QProcess *proc=new QProcess(this);
//         connect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
//         proc->start(QLatin1String("k3b"), args);
//     }
// }

#ifdef TAGLIB_FOUND
void MainWindow::editTags()
{
    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }
    #endif

    if (!songs.isEmpty()) {
        QSet<QString> artists;
        QSet<QString> albumArtists;
        QSet<QString> albums;
        QSet<QString> genres;
        #ifdef ENABLE_DEVICES_SUPPORT
        if (devicesPage->isVisible()) {
            DevicesModel::self()->getDetails(artists, albumArtists, albums, genres);
        } else
        #endif
        MusicLibraryModel::self()->getDetails(artists, albumArtists, albums, genres);
        TagEditor *dlg=new TagEditor(this, songs, artists, albumArtists, albums, genres);
        dlg->show();
    }
}
#endif

#ifdef ENABLE_DEVICES_SUPPORT
void MainWindow::organiseFiles()
{
    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }

    if (!songs.isEmpty()) {
        QString udi;
        if (devicesPage->isVisible()) {
            udi=devicesPage->activeUmsDeviceUdi();
            if (udi.isEmpty()) {
                return;
            }
        }

        TrackOrganiser *dlg=new TrackOrganiser(this);
        dlg->show(songs, udi);
    }
}

void MainWindow::addToDevice(const QString &udi)
{
    if (libraryPage->isVisible()) {
        libraryPage->addSelectionToDevice(udi);
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToDevice(udi);
    }
}

void MainWindow::deleteSongs()
{
    if (!deleteSongsAction->isVisible()) {
        return;
    }
    if (libraryPage->isVisible()) {
        libraryPage->deleteSongs();
    } else if (albumsPage->isVisible()) {
        albumsPage->deleteSongs();
    } else if (devicesPage->isVisible()) {
        devicesPage->deleteSongs();
    }
}

void MainWindow::copyToDevice(const QString &from, const QString &to, const QList<Song> &songs)
{
    ActionDialog *dlg=new ActionDialog(this);
    dlg->copy(from, to, songs);
}

void MainWindow::deleteSongs(const QString &from, const QList<Song> &songs)
{
    ActionDialog *dlg=new ActionDialog(this);
    dlg->remove(from, songs);
}
#endif
