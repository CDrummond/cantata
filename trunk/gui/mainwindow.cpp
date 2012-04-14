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
#include <QtGui/QPainter>
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
#include "utils.h"
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
#ifdef ENABLE_REPLAYGAIN_SUPPORT
#include "rgdialog.h"
#endif
#include "tageditor.h"
#include "streamsmodel.h"
#include "playlistspage.h"
#include "dynamicpage.h"
#include "fancytabwidget.h"
#include "timeslider.h"
#include "mpris.h"
#include "dockmanager.h"
#include "messagewidget.h"
#include "httpserver.h"
#include "dynamic.h"
#include "debugtimer.h"

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
#define Icon(X) KIcon(X)
#else
#include <QtGui/QIcon>
#define Icon(X) QIcon::fromTheme(X)
#endif

static QPixmap createSingleIconPixmap(int size, QColor &col, double opacity)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QFont font(QLatin1String("sans"));
    font.setBold(false);
    font.setItalic(false);
    font.setPixelSize(size*0.9);
    p.setFont(font);
    p.setPen(col);
    p.setOpacity(opacity);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.drawText(QRect(0, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.drawText(QRect(1, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.drawText(QRect(-1, 1, size, size), QLatin1String("1"), QTextOption(Qt::AlignHCenter|Qt::AlignVCenter));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.end();
    return pix;
}

static QIcon createSingleIcon()
{
    QIcon icon;
    QColor col=QColor(Qt::black);
    icon.addPixmap(createSingleIconPixmap(16, col, 100.0));
    icon.addPixmap(createSingleIconPixmap(22, col, 100.0));
    icon.addPixmap(createSingleIconPixmap(16, col, 50.0), QIcon::Disabled);
    icon.addPixmap(createSingleIconPixmap(22, col, 50.0), QIcon::Disabled);
    col=QColor(48, 48, 48);
    icon.addPixmap(createSingleIconPixmap(16, col, 100.0), QIcon::Active);
    icon.addPixmap(createSingleIconPixmap(22, col, 100.0), QIcon::Active);
    return icon;
}

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
        setBaseStyle(qApp->style());
    }

    int styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *returnData) const
    {
        if(stylehint==QStyle::SH_Slider_AbsoluteSetButtons){
            return Qt::LeftButton|QProxyStyle::styleHint(stylehint, opt, widget ,returnData);
        }else{
            return QProxyStyle::styleHint(stylehint, opt, widget, returnData);
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
    , loaded(0)
    , lastState(MPDState_Inactive)
    , songTime(0)
    , lastSongId(-1)
    , autoScrollPlayQueue(true)
    , draggingPositionSlider(false)
    , trayItem(0)
    , dynamicModeEnabled(false)
    , lyricsNeedUpdating(false)
    #ifdef ENABLE_WEBKIT
    , infoNeedsUpdating(false)
    #endif
    , dock(0)
    , mpris(0)
    , playlistSearchTimer(0)
    , usingProxy(false)
    , connectedState(CS_Init)
    , volumeFade(0)
    , origVolume(0)
    , lastVolume(0)
    , stopState(StopState_None)
{
    setMinimumHeight(256);
    QWidget *widget = new QWidget(this);
    setupUi(widget);
    setCentralWidget(widget);
    QMenu *mainMenu=new QMenu(this);

    messageWidget->hide();
    #ifndef ENABLE_KDE_SUPPORT
    setWindowIcon(QIcon(":/icons/cantata.svg"));
    setWindowTitle("Cantata");

//     quitAction=menu->addAction(tr("Quit"), qApp, SLOT(quit()));
//     menuAct->setIcon(Icon("application-exit"));
//     menuBar()->addMenu(menu);
//     menu=new QMenu(tr("Tools"), this);
//     menu=new QMenu(tr("Settings"), this);
//     menuAct=menu->addAction(tr("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
//     menuAct->setIcon(Icon("configure"));
//     menuBar()->addMenu(menu);
//     menu=new QMenu(tr("Help"), this);
//     menuAct=menu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
//     menuAct->setIcon(windowIcon());
//     menuBar()->addMenu(menu);

    QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::Instance());
    #endif

    #ifdef ENABLE_KDE_SUPPORT
    prefAction=KStandardAction::preferences(this, SLOT(showPreferencesDialog()), actionCollection());
    quitAction=KStandardAction::quit(kapp, SLOT(quit()), actionCollection());

    smallPlaybackButtonsAction = actionCollection()->addAction("smallplaybackbuttons");
    smallPlaybackButtonsAction->setText(i18n("Small Playback Buttons"));

    smallControlButtonsAction = actionCollection()->addAction("smallcontrolbuttons");
    smallControlButtonsAction->setText(i18n("Small Control Buttons"));

    connectAction = actionCollection()->addAction("connect");
    connectAction->setText(i18n("Connect"));

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
    copyToDeviceAction->setIcon(Icon("multimedia-player"));
    deleteSongsAction = actionCollection()->addAction("deletesongs");
    organiseFilesAction = actionCollection()->addAction("organizefiles");
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction = actionCollection()->addAction("replaygain");
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

    singlePlaylistAction = actionCollection()->addAction("singleplaylist");
    singlePlaylistAction->setText(i18n("Single"));
    singlePlaylistAction->setWhatsThis(i18n("When single is activated, playback is stopped after current song, or song is repeated if the 'repeat' mode is enabled."));

    consumePlaylistAction = actionCollection()->addAction("consumeplaylist");
    consumePlaylistAction->setText(i18n("Consume"));
    consumePlaylistAction->setWhatsThis(i18n("When consume is activated, a song is removed from the play queue after it has been played."));

    locateTrackAction = actionCollection()->addAction("locatetrack");
    locateTrackAction->setText(i18n("Locate In Library"));

//     burnAction = actionCollection()->addAction("burn");
//     burnAction->setText(i18n("Burn To CD/DVD"));
//
//     createAudioCdAction = actionCollection()->addAction("createaudiocd");
//     createAudioCdAction->setText(i18n("Create Audio CD"));
//
//     createDataCdAction = actionCollection()->addAction("createdatacd");
//     createDataCdAction->setText(i18n("Create Data CD/DVD"));

    editTagsAction = actionCollection()->addAction("edittags");
    editTagsAction->setText(i18n("Edit Tags"));

    editPlayQueueTagsAction = actionCollection()->addAction("editpqtags");
    editPlayQueueTagsAction->setText(i18n("Edit Song Tags"));

    libraryTabAction = actionCollection()->addAction("showlibrarytab");
    libraryTabAction->setText(i18n("Library"));

    albumsTabAction = actionCollection()->addAction("showalbumstab");
    albumsTabAction->setText(i18n("Albums"));

    foldersTabAction = actionCollection()->addAction("showfolderstab");
    foldersTabAction->setText(i18n("Folders"));

    playlistsTabAction = actionCollection()->addAction("showplayliststab");
    playlistsTabAction->setText(i18n("Playlists"));

    dynamicTabAction = actionCollection()->addAction("showdynamictab");
    dynamicTabAction->setText(i18n("Dynamic"));

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
    quitAction->setIcon(Icon("application-exit"));
    quitAction->setShortcut(QKeySequence::Quit);
    smallPlaybackButtonsAction = new QAction(tr("Small Playback Buttons"), this);
    smallControlButtonsAction = new QAction(tr("Small Control Buttons"), this);
    refreshAction = new QAction(tr("Refresh"), this);
    connectAction = new QAction(tr("Connect"), this);
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
    singlePlaylistAction = new QAction(tr("Single"), this);
    singlePlaylistAction->setWhatsThis(tr("When single is activated, playback is stopped after current song, or song is repeated if the 'repeat' mode is enabled."));
    consumePlaylistAction = new QAction(tr("Consume"), this);
    consumePlaylistAction->setWhatsThis(tr("When consume is activated, a song is removed from the play queue after it has been played."));
    locateTrackAction = new QAction(tr("Locate In Library"), this);
//     burnAction = new QAction(tr("Burn To CD/DVD"), this);
//     createAudioCdAction = new QAction(tr("Create Audio CD"), this);
//     createDataCdAction = new QAction(tr("Create Data CD"), this);
    editTagsAction = new QAction(tr("Edit Tags"), this);
    editPlayQueueTagsAction = new QAction(tr("Edit Song Tags"), this);
    libraryTabAction = new QAction(tr("Library"), this);
    albumsTabAction = new QAction(tr("Albums"), this);
    foldersTabAction = new QAction(tr("Folders"), this);
    playlistsTabAction = new QAction(tr("Playlists"), this);
    dynamicTabAction = new QAction(tr("Dynamic"), this);
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
    dynamicTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F5);
    streamsTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F6);
    lyricsTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F7);
    #ifdef ENABLE_WEBKIT
    infoTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F8);
    serverInfoTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F9);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F10);
    #endif // ENABLE_DEVICES_SUPPORT
    #else // ENABLE_WEBKIT
    serverInfoTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F8);
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setShortcut(Qt::MetaModifier+Qt::Key_F9);
    #endif // ENABLE_DEVICES_SUPPORT
    #endif // ENABLE_WEBKIT

    // Setup event handler for volume adjustment
    volumeSliderEventHandler = new VolumeSliderEventHandler(this);

    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(volumeSliderEventHandler);

    positionSlider->setStyle(new ProxyStyle());

    noCover = Icon(DEFAULT_ALBUM_ICON).pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    noStreamCover = Icon(DEFAULT_STREAM_ICON).pixmap(128, 128).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    coverWidget->setPixmap(noCover);

    playbackPlay = Icon("media-playback-start");
    playbackPause = Icon("media-playback-pause");
    randomPlaylistAction->setIcon(Icon("media-playlist-shuffle"));
    locateTrackAction->setIcon(Icon("edit-find"));
    #ifdef ENABLE_KDE_SUPPORT
    consumePlaylistAction->setIcon(Icon("cantata-view-media-consume"));
    repeatPlaylistAction->setIcon(Icon("cantata-view-media-repeat"));
    #else
    QIcon consumeIcon(":consume16.png");
    consumeIcon.addFile(":consume22.png");
    QIcon repeatIcon(":repeat16.png");
    repeatIcon.addFile(":repeat22.png");
    consumePlaylistAction->setIcon(consumeIcon);
    repeatPlaylistAction->setIcon(repeatIcon);
    #endif
    singlePlaylistAction->setIcon(createSingleIcon());
    removeAction->setIcon(Icon("list-remove"));
    addToPlaylistAction->setIcon(Icon("list-add"));
    replacePlaylistAction->setIcon(Icon("media-playback-start"));

//     burnAction->setIcon(Icon("tools-media-optical-burn"));
//     createDataCdAction->setIcon(Icon("media-optical"));
//     createAudioCdAction->setIcon(Icon(DEFAULT_ALBUM_ICON));
    editTagsAction->setIcon(Icon("document-edit"));
    editPlayQueueTagsAction->setIcon(Icon("document-edit"));
//     QMenu *cdMenu=new QMenu(this);
//     cdMenu->addAction(createAudioCdAction);
//     cdMenu->addAction(createDataCdAction);
//     burnAction->setMenu(cdMenu);
//     #ifdef ENABLE_KDE_SUPPORT
//     if (KStandardDirs::findExe("k3b").isEmpty()) {
//         burnAction->setVisible(false);
//     }
//     #endif

    prevTrackAction->setIcon(Icon("media-skip-backward"));
    nextTrackAction->setIcon(Icon("media-skip-forward"));
    playPauseTrackAction->setIcon(playbackPlay);
    stopTrackAction->setIcon(Icon("media-playback-stop"));
    removeFromPlaylistAction->setIcon(Icon("list-remove"));
    clearPlaylistAction->setIcon(Icon("edit-clear-list"));
    savePlaylistAction->setIcon(Icon("document-save-as"));
    expandInterfaceAction->setIcon(Icon("view-media-playlist"));
    refreshAction->setIcon(Icon("view-refresh"));
    connectAction->setIcon(Icon("network-connect"));
    #ifdef ENABLE_KDE_SUPPORT
    libraryTabAction->setIcon(Icon("cantata-view-media-library"));
    #else
    QIcon libraryIcon(":lib16.png");
    libraryIcon.addFile(":lib32.png");
    libraryTabAction->setIcon(libraryIcon);
    #endif
    albumsTabAction->setIcon(Icon(DEFAULT_ALBUM_ICON));
    foldersTabAction->setIcon(Icon("inode-directory"));
    playlistsTabAction->setIcon(Icon("view-media-playlist"));
    dynamicTabAction->setIcon(Icon("media-playlist-shuffle"));
    lyricsTabAction->setIcon(Icon("view-media-lyrics"));
    streamsTabAction->setIcon(Icon(DEFAULT_STREAM_ICON));
    #ifdef ENABLE_WEBKIT
    #ifdef ENABLE_KDE_SUPPORT
    infoTabAction->setIcon(Icon("cantata-view-wikipedia"));
    #else // ENABLE_KDE_SUPPORT
    QIcon wikiIcon(":wiki16.png");
    wikiIcon.addFile(":wiki32.png");
    infoTabAction->setIcon(wikiIcon);
    #endif // ENABLE_KDE_SUPPORT
    #endif
    serverInfoTabAction->setIcon(Icon("server-database"));
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesTabAction->setIcon(Icon("multimedia-player"));
    copyToDeviceAction->setMenu(DevicesModel::self()->menu());
    deleteSongsAction->setIcon(Icon("edit-delete"));
    deleteSongsAction->setText(i18n("Delete Songs"));
    organiseFilesAction->setIcon(Icon("inode-directory"));
    organiseFilesAction->setText(i18n("Organize Files"));
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction->setIcon(Icon("audio-x-generic"));
    replaygainAction->setText(i18n("ReplayGain"));
    #endif
    addToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());
    addToStoredPlaylistAction->setIcon(playlistsTabAction->icon());

    menuButton->setIcon(Icon("configure"));
    volumeButton->setIcon(Icon("audio-volume-high"));
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
    dynamicPage = new DynamicPage(this);
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
    singlePushButton->setDefaultAction(singlePlaylistAction);
    consumePushButton->setDefaultAction(consumePlaylistAction);

    QStringList hiddenPages=Settings::self()->hiddenPages();
    tabWidget->AddTab(libraryPage, libraryTabAction->icon(), libraryTabAction->text(), !hiddenPages.contains(libraryPage->metaObject()->className()));
    tabWidget->AddTab(albumsPage, albumsTabAction->icon(), albumsTabAction->text(), !hiddenPages.contains(albumsPage->metaObject()->className()));
    tabWidget->AddTab(folderPage, foldersTabAction->icon(), foldersTabAction->text(), !hiddenPages.contains(folderPage->metaObject()->className()));
    tabWidget->AddTab(playlistsPage, playlistsTabAction->icon(), playlistsTabAction->text(), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    tabWidget->AddTab(dynamicPage, dynamicTabAction->icon(), dynamicTabAction->text(), !hiddenPages.contains(dynamicPage->metaObject()->className()));
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
    singlePlaylistAction->setCheckable(true);
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
    btns << repeatPushButton << singlePushButton << randomPushButton << savePlaylistPushButton << removeAllFromPlaylistPushButton << consumePushButton;

    foreach (QToolButton *b, btns) {
        initButton(b);
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
    singlePlaylistAction->setChecked(false);
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
    expandedSize=Settings::self()->mainWindowSize();
    collapsedSize=Settings::self()->mainWindowCollapsedSize();
    if (!expandedSize.isEmpty()) {
        resize(expandedSize);
    }
    togglePlaylist();
    if (expandInterfaceAction->isChecked()) {
        if (!expandedSize.isEmpty()) {
            resize(expandedSize);
        }
    } else {
         if (!collapsedSize.isEmpty()) {
            resize(collapsedSize);
        }
    }
    #ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys | KXmlGuiWindow::Save | KXmlGuiWindow::Create);
    menuBar()->setVisible(false);
    #endif

    mainMenu->addAction(expandInterfaceAction);
    #ifdef ENABLE_KDE_SUPPORT
    QAction *menuAct=mainMenu->addAction(i18n("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
    menuAct->setIcon(Icon("configure"));
    mainMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)));
    mainMenu->addSeparator();
    mainMenu->addMenu(helpMenu());
    #else
    QAction *menuAct=mainMenu->addAction(tr("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
    menuAct->setIcon(Icon("configure"));
    prefAction=menuAct;
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
    connect(&playQueueModel, SIGNAL(statsUpdated(int, int, int, quint32)), this, SLOT(updatePlayQueueStats(int, int, int, quint32)));

    playQueueProxyModel.setSourceModel(&playQueueModel);
    playQueue->setModel(&playQueueModel);
    playQueue->addAction(removeFromPlaylistAction);
    playQueue->addAction(clearPlaylistAction);
    playQueue->addAction(savePlaylistAction);
    playQueue->addAction(cropPlaylistAction);
    playQueue->addAction(shufflePlaylistAction);
    Action *sep=new Action(this);
    sep->setSeparator(true);
    playQueue->addAction(sep);
    playQueue->addAction(locateTrackAction);
    playQueue->addAction(editPlayQueueTagsAction);
    //playQueue->addAction(copyTrackInfoAction);
    playQueue->tree()->installEventFilter(new DeleteKeyEventHandler(playQueue->tree(), removeFromPlaylistAction));
    playQueue->list()->installEventFilter(new DeleteKeyEventHandler(playQueue->list(), removeFromPlaylistAction));
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playlistItemsSelected(bool)));
    connect(streamsPage, SIGNAL(add(const QStringList &, bool)), &playQueueModel, SLOT(addItems(const QStringList &, bool)));
    autoScrollPlayQueue=Settings::self()->playQueueScroll();
    playQueueModel.setGrouped(Settings::self()->playQueueGrouped());
    playQueue->setGrouped(Settings::self()->playQueueGrouped());
    playQueue->setAutoExpand(Settings::self()->playQueueAutoExpand());
    playQueue->setStartClosed(Settings::self()->playQueueStartClosed());
    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());

    connect(MPDConnection::self(), SIGNAL(statsUpdated(const MPDStats &)), this, SLOT(updateStats()));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlaylist(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(Dynamic::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(Dynamic::self(), SIGNAL(running(bool)), this, SLOT(dynamicMode(bool)));
    connect(refreshAction, SIGNAL(triggered(bool)), this, SLOT(refresh()));
    connect(refreshAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(update()));
    connect(connectAction, SIGNAL(triggered(bool)), this, SLOT(connectToMpd()));
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
    connect(singlePlaylistAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setSingle(bool)));
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
    connect(editTagsAction, SIGNAL(activated()), this, SLOT(editTags()));
    connect(editPlayQueueTagsAction, SIGNAL(activated()), this, SLOT(editPlayQueueTags()));
    connect(locateTrackAction, SIGNAL(activated()), this, SLOT(locateTrack()));
    connect(libraryTabAction, SIGNAL(activated()), this, SLOT(showLibraryTab()));
    connect(albumsTabAction, SIGNAL(activated()), this, SLOT(showAlbumsTab()));
    connect(foldersTabAction, SIGNAL(activated()), this, SLOT(showFoldersTab()));
    connect(playlistsTabAction, SIGNAL(activated()), this, SLOT(showPlaylistsTab()));
    connect(dynamicTabAction, SIGNAL(activated()), this, SLOT(showDynamicTab()));
    connect(lyricsTabAction, SIGNAL(activated()), this, SLOT(showLyricsTab()));
    connect(streamsTabAction, SIGNAL(activated()), this, SLOT(showStreamsTab()));
    #ifdef ENABLE_WEBKIT
    connect(infoTabAction, SIGNAL(activated()), this, SLOT(showInfoTab()));
    #endif
    connect(serverInfoTabAction, SIGNAL(activated()), this, SLOT(showServerInfoTab()));
    #ifdef ENABLE_DEVICES_SUPPORT
    connect(devicesTabAction, SIGNAL(activated()), this, SLOT(showDevicesTab()));
    connect(DevicesModel::self(), SIGNAL(addToDevice(const QString &)), this, SLOT(addToDevice(const QString &)));
    connect(DevicesModel::self(), SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    connect(libraryPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(deleteSongsAction, SIGNAL(triggered()), SLOT(deleteSongs()));
    connect(organiseFilesAction, SIGNAL(triggered()), SLOT(organiseFiles()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    connect(replaygainAction, SIGNAL(triggered()), SLOT(replayGain()));
    #endif
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
    connect(playlistsPage, SIGNAL(add(const QStringList &, bool)), &playQueueModel, SLOT(addItems(const QStringList &, bool)));

    QByteArray state=Settings::self()->splitterState();

    if (state.isEmpty()) {
        QList<int> sizes;
        sizes << 250 << 500;
        splitter->setSizes(sizes);
        resize(800, 600);
    } else {
        splitter->restoreState(Settings::self()->splitterState());
    }
    toggleSplitterAutoHide(Settings::self()->splitterAutoHide());

    playlistItemsSelected(false);
    playQueue->setFocus();
    playQueue->initHeader();

    mpdThread=new QThread(this);
    MPDConnection::self()->moveToThread(mpdThread);
    mpdThread->start();
    connectToMpd();
    if (Settings::self()->enableHttp()) {
        HttpServer::self()->setPort(Settings::self()->httpPort());
    }
    #if defined ENABLE_REMOTE_DEVICES && defined ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->loadRemote();
    #endif
    QString page=Settings::self()->page();

    for (int i=0; i<tabWidget->count(); ++i) {
        if (tabWidget->widget(i)->metaObject()->className()==page) {
            tabWidget->SetCurrentIndex(i);
            break;
        }
    }

    connect(tabWidget, SIGNAL(CurrentChanged(int)), this, SLOT(currentTabChanged(int)));
    connect(tabWidget, SIGNAL(TabToggled(int)), this, SLOT(tabToggled(int)));
    connect(tabWidget, SIGNAL(AutoHideChanged(bool)), this, SLOT(toggleSplitterAutoHide(bool)));
    connect(tabWidget, SIGNAL(ModeChanged(FancyTabWidget::Mode)), this, SLOT(sidebarModeChanged()));
    connect(messageWidget, SIGNAL(visible(bool)), this, SLOT(messageWidgetVisibility(bool)));

    libraryPage->setView(0==Settings::self()->libraryView());
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    MPDParseUtils::setGroupMultiple(Settings::self()->groupMultiple());
    albumsPage->setView(Settings::self()->albumsView());
    AlbumsModel::setUseLibrarySizes(Settings::self()->albumsView()!=ItemView::Mode_IconTop);
    AlbumsModel::self()->setAlbumSort(Settings::self()->albumSort());
    playlistsPage->setView(Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(0==Settings::self()->devicesView());
    #endif

    fadeStop=Settings::self()->stopFadeDuration()>Settings::MinFade;
    playlistsPage->refresh();
    toggleMpris();
    if (Settings::self()->dockManager()) {
        QTimer::singleShot(250, this, SLOT(toggleDockManager()));
    }
}

MainWindow::~MainWindow()
{
    if (dock) {
        dock->setIcon(QString());
    }
    Settings::self()->saveMainWindowSize(splitter->isVisible() ? size() : expandedSize);
    Settings::self()->saveMainWindowCollapsedSize(splitter->isVisible() ? collapsedSize : size());
    #if defined ENABLE_REMOTE_DEVICES && defined ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->unmountRemote();
    #endif
    Settings::self()->saveShowPlaylist(expandInterfaceAction->isChecked());
    Settings::self()->saveSplitterState(splitter->saveState());
    Settings::self()->saveSidebar((int)(tabWidget->mode()));
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    Settings::self()->saveSmallPlaybackButtons(smallPlaybackButtonsAction->isChecked());
    Settings::self()->saveSmallControlButtons(smallControlButtonsAction->isChecked());
    Settings::self()->saveSplitterAutoHide(splitter->isAutoHideEnabled());
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
    streamsPage->save();
    lyricsPage->saveSettings();
    #ifdef ENABLE_WEBKIT
    infoPage->save();
    #endif
    Settings::self()->save(true);
    disconnect(MPDConnection::self(), 0, 0, 0);
    if (Settings::self()->stopDynamizerOnExit() || Settings::self()->stopOnExit()) {
        Dynamic::self()->stop();
    }
    if (Settings::self()->stopOnExit() || (fadeStop && StopState_Stopping==stopState)) {
        emit stop();
        Utils::sleep();
    }
    Utils::stopThread(mpdThread);
    Covers::self()->stop();
}

void MainWindow::load(const QList<QUrl> &urls)
{
    QStringList useable;
    bool allowLocal=MPDConnection::self()->isLocal();

    foreach (QUrl u, urls) {
        if ((allowLocal && QLatin1String("file")==u.scheme()) || QLatin1String("http")==u.scheme()) {
            useable.prepend(u.toString());
        }
    }
    if (useable.count()) {
        playQueueModel.addItems(useable, playQueueModel.rowCount(), false);
    }
}

const QDateTime & MainWindow::getDbUpdate() const
{
    return serverInfoPage->getDbUpdate();
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
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        #ifdef ENABLE_KDE_SUPPORT
        b->setIconSize(small ? QSize(22, 22) : QSize(28, 28));
        b->setMinimumSize(small ? QSize(26, 26) : QSize(32, 32));
        b->setMaximumSize(small ? QSize(26, 26) : QSize(32, 32));
        #else
        b->setIconSize(small ? QSize(24, 24) : QSize(28, 28));
        b->setMinimumSize(small ? QSize(26, 26) : QSize(36, 36));
        b->setMaximumSize(small ? QSize(26, 26) : QSize(36, 36));
        #endif
    }
}

void MainWindow::setControlButtonsSize(bool small)
{
    QList<QToolButton *> controlBtns;
    controlBtns << volumeButton << menuButton;

    foreach (QToolButton *b, controlBtns) {
        b->setMinimumSize(small ? QSize(22, 22) : QSize(26, 26));
        b->setMaximumSize(small ? QSize(22, 22) : QSize(26, 26));
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        #ifdef ENABLE_KDE_SUPPORT
        b->setIconSize(small ? QSize(16, 16) : QSize(22, 22));
        #else
        b->setIconSize(small ? QSize(18, 18) : QSize(22, 22));
        #endif
    }
}

void MainWindow::songLoaded()
{
    if (MPDState_Stopped==MPDStatus::self()->state()) {
        stopVolumeFade();
        emit play();
    }
}

void MainWindow::showError(const QString &message, bool showActions)
{
    messageWidget->setError(message);
    if (showActions) {
        messageWidget->addAction(prefAction);
        messageWidget->addAction(connectAction);
    } else {
        messageWidget->removeAction(prefAction);
        messageWidget->removeAction(connectAction);
    }
}

void MainWindow::messageWidgetVisibility(bool v)
{
    if (v && !splitter->isVisible()) {
        expandInterfaceAction->trigger();
    }
}

void MainWindow::dynamicMode(bool on)
{
    if (on==dynamicModeEnabled) {
        return;
    }
    dynamicModeEnabled=on;
    if (on) {
        #ifdef ENABLE_KDE_SUPPORT
        messageWidget->setInformation(i18n("Dynamic Mode Enabled"));
        #else
        messageWidget->setInformation(tr("Dynamic Mode Enabled"));
        #endif
    }

    playQueueModel.playListStats();
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    if (connected) {
        messageWidget->hide();
        if (CS_Connected!=connectedState) {
            emit getStatus();
            emit getStats();
            emit playListInfo();
            if (CS_Init!=connectedState) {
                loaded=0;
                currentTabChanged(tabWidget->current_index());
            }
            connectedState=CS_Connected;
        }
    } else {
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        lyricsPage->text->clear();
        serverInfoPage->clear();
        QString host=MPDConnection::self()->getHost();
        #ifdef ENABLE_KDE_SUPPORT
        if (host.startsWith('/')) {
            showError(i18n("Connection to %1 failed", host), true);
        } else {
            showError(i18nc("host:port", "Connection to %1:%2 failed", host, QString::number(MPDConnection::self()->getPort())), true);
        }
        #else
        if (host.startsWith('/')) {
            showError(tr("Connection to %1 failed").arg(host), true);
        } else {
            showError(tr("Connection to %1:%2 failed").arg(host).arg(QString::number(MPDConnection::self()->getPort())), true);
        }
        #endif
        connectedState=CS_Disconnected;
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

void MainWindow::connectToMpd()
{
    messageWidget->hide();
    emit setDetails(Settings::self()->connectionHost(), Settings::self()->connectionPort(), Settings::self()->connectionPasswd());
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

    connectToMpd();
    Covers::self()->setSaveInMpdDir(Settings::self()->storeCoversInMpdDir());
    HttpServer::self()->setPort(Settings::self()->enableHttp() ? Settings::self()->httpPort() : 0);
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction->setEnabled(QDir(Settings::self()->mpdDir()).isReadable());
    deleteSongsAction->setEnabled(copyToDeviceAction->isEnabled());
    organiseFilesAction->setEnabled(copyToDeviceAction->isEnabled());
//     burnAction->setEnabled(copyToDeviceAction->isEnabled());
    deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction->setEnabled(QDir(Settings::self()->mpdDir()).isReadable());
    #endif
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    Settings::self()->save();
    bool useLibSizeForAl=Settings::self()->albumsView()!=ItemView::Mode_IconTop;
    bool diffLibCovers=((int)MusicLibraryItemAlbum::currentCoverSize())!=Settings::self()->libraryCoverSize();
    bool diffAlCovers=((int)AlbumsModel::currentCoverSize())!=Settings::self()->albumsCoverSize() ||
                      albumsPage->viewMode()!=Settings::self()->albumsView() ||
                      useLibSizeForAl!=AlbumsModel::useLibrarySizes();
    bool diffLibYear=MusicLibraryItemAlbum::showDate()!=Settings::self()->libraryYear();
    bool diffGrouping=MPDParseUtils::groupSingle()!=Settings::self()->groupSingle() ||
                      MPDParseUtils::groupMultiple()!=Settings::self()->groupMultiple();

    if (diffLibCovers) {
        MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    }
    if (diffLibYear) {
        MusicLibraryItemAlbum::setShowDate(Settings::self()->libraryYear());
    }
    if (diffAlCovers) {
        AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());
    }
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    MPDParseUtils::setGroupMultiple(Settings::self()->groupMultiple());

    AlbumsModel::setUseLibrarySizes(useLibSizeForAl);
    AlbumsModel::self()->setAlbumSort(Settings::self()->albumSort());
    albumsPage->setView(Settings::self()->albumsView());
    if (diffAlCovers || diffGrouping) {
        albumsPage->clear();
    }
    if (diffLibCovers || diffAlCovers || diffLibYear || diffGrouping) {
        refresh();
    }

    libraryPage->setView(0==Settings::self()->libraryView());
    playlistsPage->setView(Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());
    if (folderPage->isVisible()) {
        folderPage->controlActions();
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(0==Settings::self()->devicesView());
    #endif
    setupTrayIcon();
    toggleDockManager();
    toggleMpris();
    autoScrollPlayQueue=Settings::self()->playQueueScroll();

    bool wasAutoExpand=playQueue->isAutoExpand();
    bool wasStartClosed=playQueue->isStartClosed();
    playQueue->setAutoExpand(Settings::self()->playQueueAutoExpand());
    playQueue->setStartClosed(Settings::self()->playQueueStartClosed());

    if (Settings::self()->playQueueGrouped()!=playQueueModel.isGrouped() ||
        (playQueueModel.isGrouped() && (wasAutoExpand!=playQueue->isAutoExpand() || wasStartClosed!=playQueue->isStartClosed())) ) {
        playQueueModel.setGrouped(Settings::self()->playQueueGrouped());
        playQueue->setGrouped(Settings::self()->playQueueGrouped());
        playQueue->updateRows(usingProxy ? playQueueModel.rowCount()+10 : playQueueModel.currentSongRow(),
                              !usingProxy && autoScrollPlayQueue && MPDState_Playing==MPDStatus::self()->state());
    }

    wasStartClosed=playlistsPage->isStartClosed();
    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());
    if (ItemView::Mode_GroupedTree==Settings::self()->playlistsView() && wasStartClosed!=playlistsPage->isStartClosed()) {
        playlistsPage->updateRows();
    }
}

#ifndef ENABLE_KDE_SUPPORT
void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("About Cantata"),
                       tr("Simple GUI front-end for MPD.<br/><br/>(c) Craig Drummond 2011-2012.<br/>Released under the GPLv2<br/><br/><i><small>Based upon QtMPC - (C) 2007-2010 The QtMPC Authors</small></i>"));
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
    if (!fadeStop || MPDState_Paused==MPDStatus::self()->state() || 0==volume) {
        emit stop();
    }
    stopTrackAction->setEnabled(false);
    nextTrackAction->setEnabled(false);
    prevTrackAction->setEnabled(false);
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
        if (StopState_Stopping==stopState) {
            emit stop();
        } /*else if (StopState_Pausing==stopState) {
            emit pause(true);
        }*/
        stopState=StopState_None;
        volume=origVolume;
        emit setVolume(origVolume);
    } else if (lastVolume!=v) {
        emit setVolume(v);
        lastVolume=v;
    }
}

void MainWindow::playPauseTrack()
{
    MPDStatus * const status = MPDStatus::self();

    if (MPDState_Playing==status->state()) {
        /*if (fadeStop) {
            startVolumeFade(false);
        } else*/ {
            emit pause(true);
        }
    } else if (MPDState_Paused==status->state()) {
        stopVolumeFade();
        emit pause(false);
    } else {
        stopVolumeFade();
        if (-1!=playQueueModel.currentSong() && -1!=playQueueModel.currentSongRow()) {
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
            playlistSearchTimer->setSingleShot(true);
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
            playQueue->setFilterActive(false);
            playQueue->updateRows(playQueueModel.currentSongRow(), autoScrollPlayQueue && MPDState_Playing==MPDStatus::self()->state());
            scrollPlayQueue();
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
            playQueue->updateRows(playQueueModel.rowCount()+10, false);
            playQueue->setFilterActive(true);
        }
    }
    playQueueProxyModel.update(filter);

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
    playPauseTrackAction->setEnabled(!songs.isEmpty());
    nextTrackAction->setEnabled(stopTrackAction->isEnabled() && songs.count()>1);
    prevTrackAction->setEnabled(stopTrackAction->isEnabled() && songs.count()>1);

    // Remeber first row and first song id
    bool hadSelection=false;
    qint32 firstSelectedSongId = -1;
    qint32 firstSelectedRow = -1;

    if (playQueue->selectionModel()->hasSelection()) {
        QModelIndexList items = playQueue->selectionModel()->selectedRows();
        hadSelection=true;
        // find smallest selected rownum
        foreach (const QModelIndex &index, items) {
            QModelIndex sourceIndex = usingProxy ? playQueueProxyModel.mapToSource(index) : index;
            if (firstSelectedRow == -1 || index.row() < firstSelectedRow) {
                firstSelectedRow = index.row();
                firstSelectedSongId = playQueueModel.getIdByRow(sourceIndex.row());
            }
        }
    }

    playQueueModel.update(songs);
    playQueue->updateRows(usingProxy ? playQueueModel.rowCount()+10 : playQueueModel.currentSongRow(), false);

    if (hadSelection) {
        // We had a selection before the update, and we will still have one now - as the model does add/del/move...
        // *But* the current index seems to get messed up
        // ...and if we have deleted all the selected items,we want to select the next one
        qint32 newCurrentRow = playQueueModel.getRowById(firstSelectedSongId);
        if (newCurrentRow<0) {
            // Previously selected row was deleted, so select row nearest to it...
            if (!playQueue->selectionModel()->hasSelection()) {
                QModelIndex index=usingProxy
                                    ? playQueueProxyModel.index(qBound(0, firstSelectedRow, playQueueProxyModel.rowCount() - 1), 0)
                                    : playQueueModel.index(qBound(0, firstSelectedRow, playQueueModel.rowCount() - 1), 0);

                playQueue->setCurrentIndex(index);
                playQueue->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        } else {
            // The row was not deleted, could have been moved, so we need t set the current index
            playQueue->setCurrentIndex(usingProxy ? playQueueProxyModel.mapFromSource(playQueueModel.index(newCurrentRow, 0)) : playQueueModel.index(newCurrentRow, 0));
        }
    }

    if (1==songs.count() && MPDState_Playing==MPDStatus::self()->state()) {
        updateCurrentSong(songs.at(0));
    } else if (0==songs.count()) {
        updateCurrentSong(Song());
    }
}

bool MainWindow::currentIsStream() const
{
    return playQueueModel.rowCount() && -1!=current.id && current.isStream();
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

    if (current.isCantataStream()) {
        Song mod=HttpServer::self()->decodeUrl(current.file);
        if (!mod.title.isEmpty()) {
            current=mod;
            current.id=song.id;
//             current.file="XXX";
        }
    }

    positionSlider->setEnabled(-1!=current.id && !currentIsStream());
    updateCurrentCover();

    if (current.name.isEmpty()) {
        trackLabel->setText(current.title);
    } else {
        trackLabel->setText(QString("%1 (%2)").arg(current.title).arg(current.name));
    }
    if (current.album.isEmpty()) {
        artistLabel->setText(current.artist);
    } else {
        QString album=current.album;

        if (current.year>0) {
            album+=QString(" (%1)").arg(current.year);
        }
        #ifdef ENABLE_KDE_SUPPORT
        artistLabel->setText(i18nc("artist - album", "%1 - %2", current.artist, album));
        #else
        artistLabel->setText(tr("%1 - %2").arg(current.artist).arg(album));
        #endif
    }

    playQueueModel.updateCurrentSong(current.id);
    playQueue->updateRows(usingProxy ? playQueueModel.rowCount()+10 : playQueueModel.getRowById(current.id),
                          !usingProxy && autoScrollPlayQueue && MPDState_Playing==MPDStatus::self()->state());
    scrollPlayQueue();

    if (current.artist.isEmpty()) {
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
        setWindowTitle(i18nc("track - artist :: Cantata", "%1 - %2 :: Cantata", trackLabel->text(), current.artist));
        #else
        setWindowTitle(tr("%1 - %2 :: Cantata").arg(trackLabel->text()).arg(current.artist));
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

    if (Settings::self()->showPopups() || trayItem) {
        if (!current.title.isEmpty() && !current.artist.isEmpty() && !current.album.isEmpty()) {
            #ifdef ENABLE_KDE_SUPPORT
            QPixmap *coverPixmap = 0;
            const QString text(i18n("<table>"
                                    "<tr><td align=\"right\"><b>Artist:</b></td><td>%1</td></tr>"
                                    "<tr><td align=\"right\"><b>Album:</b></td><td>%2</td></tr>"
                                    "<tr><td align=\"right\"><b>Song:</b></td><td>%3</td></tr>"
                                    "<tr><td align=\"right\"><b>Track:</b></td><td>%4</td></tr>"
                                    "<tr><td align=\"right\"><b>Length:</b></td><td>%5</td></tr>"
                                    "</table>").arg(current.artist).arg(current.album).arg(current.title)
                                               .arg(current.track).arg(Song::formattedTime(current.time)));
            if (!coverSong.album.isEmpty () && coverSong.albumArtist()==current.albumArtist() &&
                coverSong.album==current.album && coverWidget->pixmap()) {
                coverPixmap = const_cast<QPixmap*>(coverWidget->pixmap());
            }

            if (Settings::self()->showPopups()) {
                KNotification *notification = new KNotification("CurrentTrackChanged", this);
                notification->setText(text);
                if (coverPixmap) {
                    notification->setPixmap(*coverPixmap);
                }
                notification->sendEvent();
            }

            if (trayItem) {
                trayItem->setToolTip("cantata", i18n("Cantata"), text);

                // Use the cover as icon pixmap.
                if (coverPixmap) {
                    trayItem->setToolTipIconByPixmap(*coverPixmap);
                }
            }
            #else
            // The pure Qt implementation needs both, the tray icon
            // and the setting checked.
            if (Settings::self()->showPopups() && trayItem) {
                const QString text=tr("Album: %1\n"
                                      "Track: %2\n"
                                      "Length: %3").arg(current.album).arg(current.track).arg(Song::formattedTime(current.time));

                trayItem->showMessage(tr("%1 - %2").arg(current.artist).arg(current.title), text, QSystemTrayIcon::Information, 5000);
            }
            #endif
        } else if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setToolTip("cantata", i18n("Cantata"), QString());
            #endif
        }
    }
}

void MainWindow::updateCurrentCover()
{
    // Determine if album cover should be updated
    const QString &albumArtist=current.albumArtist();
    QString covArtist=coverWidget->property("artist").toString();
    QString covAlbum=coverWidget->property("album").toString();
    if (covArtist!= albumArtist || covAlbum != current.album || (covArtist.isEmpty() && covAlbum.isEmpty())) {
        if (!albumArtist.isEmpty() && !current.album.isEmpty()) {
            Covers::Image img=Covers::self()->get(current, MPDParseUtils::groupSingle() && MusicLibraryModel::self()->isFromSingleTracks(current));
            if (!img.img.isNull()) {
                coverSong=current;
                coverWidget->setPixmap(QPixmap::fromImage(img.img).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                coverFileName=img.fileName;
                emit coverFile(img.fileName);
            }
        } else {
            coverWidget->setPixmap(currentIsStream() ? noStreamCover : noCover);
            if (dock && dock->enabled()) {
                emit coverFile(QString());
            }
        }
        coverWidget->setProperty("artist", albumArtist);
        coverWidget->setProperty("album", current.album);
    }
}

void MainWindow::scrollPlayQueue()
{
    if (autoScrollPlayQueue && MPDState_Playing==MPDStatus::self()->state() && !playQueueModel.isGrouped()) {
        qint32 row=playQueueModel.currentSongRow();
        if (row>=0) {
            QModelIndex idx=usingProxy ? playQueueProxyModel.mapFromSource(playQueueModel.index(row, 0)) : playQueueModel.index(row, 0);
            playQueue->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        }
    }
}

void MainWindow::updateStats()
{
    /*
     * Check if remote db is more recent than local one
     * Also update the dirview
     */
    if (!lastDbUpdate.isValid() || serverInfoPage->getDbUpdate() > lastDbUpdate) {
        loaded|=TAB_LIBRARY|TAB_FOLDERS;
        if (!lastDbUpdate.isValid()) {
            libraryPage->clear();
            //albumsPage->clear();
            folderPage->clear();
            playlistsPage->clear();
        }
        libraryPage->refresh();
        folderPage->refresh();
        playlistsPage->refresh();
    }

    lastDbUpdate = serverInfoPage->getDbUpdate();
}

void MainWindow::updateStatus()
{
    MPDStatus * const status = MPDStatus::self();

    if (!draggingPositionSlider) {
        if (MPDState_Stopped==status->state() || MPDState_Inactive==status->state()) {
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
        volumeControl->setValue(volume);

        if (0==volume) {
            volumeButton->setIcon(Icon("audio-volume-muted"));
        } else if (volume<=33) {
            volumeButton->setIcon(Icon("audio-volume-low"));
        } else if (volume<=67) {
            volumeButton->setIcon(Icon("audio-volume-medium"));
        } else {
            volumeButton->setIcon(Icon("audio-volume-high"));
        }
    }

    randomPlaylistAction->setChecked(status->random());
    repeatPlaylistAction->setChecked(status->repeat());
    singlePlaylistAction->setChecked(status->single());
    consumePlaylistAction->setChecked(status->consume());

    QString timeElapsedFormattedString;

    if (!currentIsStream() || (status->timeTotal()>0 && status->timeElapsed()<=status->timeTotal())) {
        if (status->state() == MPDState_Stopped || status->state() == MPDState_Inactive) {
            timeElapsedFormattedString = "0:00 / 0:00";
        } else {
            timeElapsedFormattedString += Song::formattedTime(status->timeElapsed());
            timeElapsedFormattedString += " / ";
            timeElapsedFormattedString += Song::formattedTime(status->timeTotal());
            songTime = status->timeTotal();
        }
    }

    songTimeElapsedLabel->setText(timeElapsedFormattedString);

    playQueueModel.setState(status->state());
    switch (status->state()) {
    case MPDState_Playing:
        playPauseTrackAction->setIcon(playbackPause);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        //playPauseTrackButton->setChecked(false);
        if (StopState_Stopping!=stopState) {
            stopTrackAction->setEnabled(true);
            nextTrackAction->setEnabled(true);
            prevTrackAction->setEnabled(true);
        }
        positionSlider->startTimer();

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("media-playback-start");
            #else
            trayItem->setIcon(playbackPlay);
            #endif
        }

        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        stopTrackAction->setEnabled(false);
        nextTrackAction->setEnabled(false);
        prevTrackAction->setEnabled(false);
        if (!playPauseTrackAction->isEnabled()) {
            trackLabel->setText(QString());
            artistLabel->setText(QString());
            current=Song();
            updateCurrentCover();
        }
        setWindowTitle("Cantata");
        current.id=0;

        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("cantata");
            trayItem->setToolTip("cantata", i18n("Cantata"), "<i>Playback stopped</i>");
            #else
            trayItem->setIcon(windowIcon());
            #endif
        }

        positionSlider->stopTimer();
        break;
    case MPDState_Paused:
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        stopTrackAction->setEnabled(0!=playQueueModel.rowCount());
        nextTrackAction->setEnabled(playQueueModel.rowCount()>1);
        prevTrackAction->setEnabled(playQueueModel.rowCount()>1);
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
    if (lastState == MPDState_Inactive
            || (lastState == MPDState_Stopped && status->state() == MPDState_Playing)
            || lastSongId != status->songId()) {
        emit currentSong();
    }
    // Update status info
    lastState = status->state();
    lastSongId = status->songId();
}

void MainWindow::playlistItemActivated(const QModelIndex &index)
{
    emit startPlayingSongId(playQueueModel.getIdByRow(usingProxy ? playQueueProxyModel.mapToSource(index).row() : index.row()));
}

void MainWindow::removeFromPlaylist()
{
    const QModelIndexList items = playQueue->selectedIndexes();
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
//     emit clear();
//     emit getStatus();
    addToPlaylist(true);
}

void MainWindow::addToPlaylist()
{
    addToPlaylist(false);
}

void MainWindow::addToPlaylist(bool replace)
{
    searchPlaylistLineEdit->clear();
    if (libraryPage->isVisible()) {
        libraryPage->addSelectionToPlaylist(QString(), replace);
    } else if (albumsPage->isVisible()) {
        albumsPage->addSelectionToPlaylist(QString(), replace);
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToPlaylist(QString(), replace);
    } else if (playlistsPage->isVisible()) {
        playlistsPage->addSelectionToPlaylist(replace);
    } else if (streamsPage->isVisible()) {
        streamsPage->addSelectionToPlaylist(replace);
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

void MainWindow::updatePlayQueueStats(int artists, int albums, int songs, quint32 time)
{
    if (0==time) {
        playListStatsLabel->setText(QString());
        return;
    }

    QString status;
    if (dynamicModeEnabled) {
        #ifdef ENABLE_KDE_SUPPORT
        status+=i18n("[Dynamic]");
        #else
        status+=tr("[Dynamic]");
        #endif
        status+=' ';
    }
    #ifdef ENABLE_KDE_SUPPORT
    status+=i18np("1 Artist, ", "%1 Artists, ", artists);
    status+=i18np("1 Album, ", "%1 Albums, ", albums);
    status+=i18np("1 Track", "%1 Tracks", songs);
    #else
    status += QString::number(artists)+QString(1==artists ? tr(" Artist,") : tr(" Artists, "));
    status += QString::number(albums)+QString(1==albums ? tr(" Album,") : tr(" Albums, "));
    status += QString::number(songs)+QString(1==songs ? tr(" Track") : tr(" Tracks"));
    #endif
    status += " (";
    status += MPDParseUtils::formatDuration(time);
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
    const QModelIndexList items = playQueue->selectedIndexes();

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

int MainWindow::calcMinHeight()
{
    if (FancyTabWidget::Mode_LargeSidebar==tabWidget->mode()) {
        return coverWidget->height()+(tabWidget->visibleCount()*(32+fontMetrics().height()+4));
    } else if (FancyTabWidget::Mode_IconOnlyLargeSidebar==tabWidget->mode()) {
        return coverWidget->height()+(tabWidget->visibleCount()*(32+6));
    }
    return 256;
}

void MainWindow::togglePlaylist()
{
    if (!expandInterfaceAction->isChecked() && messageWidget->isVisible()) {
        expandInterfaceAction->trigger();
        return;
    }
    if (splitter->isVisible()==expandInterfaceAction->isChecked()) {
        setMinimumHeight(calcMinHeight());
        return;
    }
    static bool lastMax=false;

    bool showing=expandInterfaceAction->isChecked();
    QPoint p(isVisible() ? pos() : QPoint());
    int compactHeight=0;

    if (!showing) {
        setMinimumHeight(0);
        lastMax=isMaximized();
        expandedSize=size();
        int spacing=style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
        // For some reason height is always larger than it needs to be - so fix this to cover height +4
        compactHeight=qMax(qMax(playPauseTrackButton->height(), trackLabel->height()+artistLabel->height())+
                           songTimeElapsedLabel->height()+positionSlider->height()+(spacing*3),
                           coverWidget->height()+spacing);
    } else {
        collapsedSize=size();
        setMinimumHeight(calcMinHeight());
        setMaximumHeight(65535);
    }
    int prevWidth=size().width();
    splitter->setVisible(showing);
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
        if (messageWidget->isVisible()) {
            messageWidget->adjustSize();
            QApplication::processEvents();
        }
        if (lastMax) {
            showMaximized();
        }
    } else {
        // Widths also sometimes expands, so make sure this is no larger than it was before...
        resize(collapsedSize.isValid() ? collapsedSize.width() : (size().width()>prevWidth ? prevWidth : size().width()), compactHeight);
        setMinimumHeight(size().height());
        setMaximumHeight(size().height());
    }

    if (!p.isNull()) {
        move(p);
    }
}

void MainWindow::sidebarModeChanged()
{
    if (splitter->isVisible()) {
        setMinimumHeight(calcMinHeight());
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
    const QModelIndexList items = playQueue->selectedIndexes();

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
    trayItem->setToolTip("cantata", i18n("Cantata"), QString());

    trayItemMenu = new KMenu(this);
    trayItemMenu->addAction(prevTrackAction);
    trayItemMenu->addAction(playPauseTrackAction);
    trayItemMenu->addAction(stopTrackAction);
    trayItemMenu->addAction(nextTrackAction);
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
    trayItemMenu->addAction(playPauseTrackAction);
    trayItemMenu->addAction(stopTrackAction);
    trayItemMenu->addAction(nextTrackAction);
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
void MainWindow::trayItemClicked(QSystemTrayIcon::ActivationReason reason)
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
    case PAGE_LIBRARY:
        locateTrackAction->setVisible(tabWidget->isEnabled(index));
        break;
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
    sidebarModeChanged();
}

void MainWindow::cover(const QString &artist, const QString &album, const QImage &img, const QString &file)
{
    if (artist==current.albumArtist() && album==current.album) {
        if (img.isNull()) {
            coverSong=Song();
            coverWidget->setPixmap(currentIsStream() ? noStreamCover : noCover);
            coverFileName=QString();
            emit coverFile(QString());
        } else {
            coverSong=current;
            coverWidget->setPixmap(QPixmap::fromImage(img).scaled(coverWidget->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            coverFileName=file;
            emit coverFile(file);
        }
    }
}

void MainWindow::toggleSplitterAutoHide(bool ah)
{
    splitter->setAutoHideEnabled(ah);
    splitter->setAutohidable(0, ah);
}

void MainWindow::locateTrack()
{
    if (!libraryPage->isVisible()) {
        showLibraryTab();
    }
    libraryPage->showSongs(playQueue->selectedSongs());
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

    bool wasEnabled=dock->enabled();
    dock->setEnabled(Settings::self()->dockManager());
    if (dock->enabled() && !wasEnabled && !coverFileName.isEmpty()) {
        emit coverFile(coverFileName);
    }
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

void MainWindow::editTags()
{
    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (folderPage->isVisible()) {
        songs=folderPage->selectedSongs();
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }
    #endif
    editTags(songs, false);
}

void MainWindow::editPlayQueueTags()
{
    editTags(playQueue->selectedSongs(), true);
}

#ifdef ENABLE_KDE_SUPPORT
#define DIALOG_ERROR KMessageBox::error(this, i18n("Action is not currently possible, due to other open dialogs.")); return
#else
#define DIALOG_ERROR QMessageBox::information(this, tr("Action is not currently possible, due to other open dialogs."), QMessageBox::Ok); return
#endif

void MainWindow::editTags(const QList<Song> &songs, bool isPlayQueue)
{
    if (songs.isEmpty()) {
        return;
    }
    if (0!=TagEditor::instanceCount()) {
        return;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=ActionDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    if (0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (0!=RgDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif

    QSet<QString> artists;
    QSet<QString> albumArtists;
    QSet<QString> albums;
    QSet<QString> genres;
    #ifdef ENABLE_DEVICES_SUPPORT
    QString udi;
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
    TagEditor *dlg=new TagEditor(this, songs, artists, albumArtists, albums, genres
                                #ifdef ENABLE_DEVICES_SUPPORT
                                , udi
                                #endif
                                );
    dlg->show();
}

#ifdef ENABLE_DEVICES_SUPPORT
void MainWindow::organiseFiles()
{
    if (0!=TrackOrganiser::instanceCount()) {
        return;
    }

    if (0!=TagEditor::instanceCount() || 0!=ActionDialog::instanceCount()) {
        DIALOG_ERROR;
    }

    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (0!=RgDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif

    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (folderPage->isVisible()) {
        songs=folderPage->selectedSongs();
    } else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }

    if (!songs.isEmpty()) {
        QString udi;
        if (devicesPage->isVisible()) {
            udi=devicesPage->activeFsDeviceUdi();
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
    } else if (folderPage->isVisible()) {
        folderPage->addSelectionToDevice(udi);
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
    } else if (folderPage->isVisible()) {
        folderPage->deleteSongs();
    } else if (devicesPage->isVisible()) {
        devicesPage->deleteSongs();
    }
}

void MainWindow::copyToDevice(const QString &from, const QString &to, const QList<Song> &songs)
{
    if (0!=ActionDialog::instanceCount()) {
        return;
    }
    if (0!=TagEditor::instanceCount()) {
        DIALOG_ERROR;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (0!=RgDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif

    ActionDialog *dlg=new ActionDialog(this);
    dlg->copy(from, to, songs);
}

void MainWindow::deleteSongs(const QString &from, const QList<Song> &songs)
{
    if (0!=ActionDialog::instanceCount()) {
        return;
    }
    if (0!=TagEditor::instanceCount()) {
        DIALOG_ERROR;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (0!=RgDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif

    ActionDialog *dlg=new ActionDialog(this);
    dlg->remove(from, songs);
}
#endif

#ifdef ENABLE_REPLAYGAIN_SUPPORT
void MainWindow::replayGain()
{
    if (0!=RgDialog::instanceCount()) {
        return;
    }
    if (0!=ActionDialog::instanceCount() || 0!=TagEditor::instanceCount()) {
        DIALOG_ERROR;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif

    QList<Song> songs;
    if (libraryPage->isVisible()) {
        songs=libraryPage->selectedSongs();
    } else if (albumsPage->isVisible()) {
        songs=albumsPage->selectedSongs();
    } else if (folderPage->isVisible()) {
        songs=folderPage->selectedSongs();
    } else if (devicesPage->isVisible()) {
        songs=devicesPage->selectedSongs();
    }

    if (!songs.isEmpty()) {
        QString udi;
        if (devicesPage->isVisible()) {
            udi=devicesPage->activeFsDeviceUdi();
            if (udi.isEmpty()) {
                return;
            }
        }

        RgDialog *dlg=new RgDialog(this);
        dlg->show(songs, udi);
    }
}
#endif
