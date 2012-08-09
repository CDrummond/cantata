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
#include <QtCore/QThread>
#include <QtCore/QDateTime>
#include <QtGui/QResizeEvent>
#include <QtGui/QMoveEvent>
#include <QtGui/QClipboard>
#include <QtGui/QProxyStyle>
#include <QtGui/QPainter>
#include <cstdlib>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KApplication>
#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KNotification>
#include <KDE/KStandardAction>
#include <KDE/KStandardDirs>
#include <KDE/KAboutApplicationDialog>
#include <KDE/KLineEdit>
#include <KDE/KXMLGUIFactory>
#include <KDE/KMenuBar>
#include <KDE/KMenu>
#include <KDE/KStatusNotifierItem>
#else
#include <QtGui/QMenuBar>
#include "networkproxyfactory.h"
#endif
#include "localize.h"
#include "mainwindow.h"
#ifdef PHONON_FOUND
#include <phonon/audiooutput.h>
#endif
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
#include "lyricspage.h"
#include "infopage.h"
#include "serverinfopage.h"
#if defined ENABLE_DEVICES_SUPPORT && defined TAGLIB_FOUND
#include "devicespage.h"
#include "devicesmodel.h"
#include "actiondialog.h"
#include "syncdialog.h"
#endif
#ifdef TAGLIB_FOUND
#include "httpserver.h"
#include "trackorganiser.h"
#include "tageditor.h"
#ifdef ENABLE_REPLAYGAIN_SUPPORT
#include "rgdialog.h"
#endif
#endif
#include "streamsmodel.h"
#include "playlistspage.h"
#include "fancytabwidget.h"
#include "timeslider.h"
#if !defined Q_OS_WIN && !defined CANTATA_ANDROID
#include "mpris.h"
#include "dockmanager.h"
#include "dynamicpage.h"
#include "dynamic.h"
#include "cantataadaptor.h"
#endif
#include "messagewidget.h"
#include "groupedview.h"
#include "actionitemdelegate.h"
#include "icon.h"
#include "debugtimer.h"
#ifdef Q_OS_WIN
static void raiseWindow(QWidget *w);
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
    QColor stdColor=QColor(QApplication::palette().color(QPalette::Active, QPalette::WindowText));
    if (stdColor==Qt::white) {
        stdColor=QColor(200, 200, 200);
    }
    QColor highlightColor=stdColor.red()<100 ? stdColor.lighter(50) : stdColor.darker(50);
    QList<int> sizes=QList<int>() << 16 << 22
                                 #ifdef CANTATA_ANDROID
                                  << 32 << 48 << 64
                                 #endif
                                  ;

    foreach (int s, sizes) {
        icon.addPixmap(createSingleIconPixmap(s, stdColor, 100.0));
        icon.addPixmap(createSingleIconPixmap(s, stdColor, 50.0), QIcon::Disabled);
        icon.addPixmap(createSingleIconPixmap(s, highlightColor, 100.0), QIcon::Active);
    }

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
        #ifndef CANTATA_ANDROID
        setBaseStyle(qApp->style());
        #endif
    }

    int styleHint(StyleHint stylehint, const QStyleOption *opt, const QWidget *widget, QStyleHintReturn *returnData) const
    {
        if (QStyle::SH_Slider_AbsoluteSetButtons==stylehint) {
            return Qt::LeftButton|QProxyStyle::styleHint(stylehint, opt, widget, returnData);
        } else {
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
    int size=Icon::stdSize(QApplication::fontMetrics().height());

    slider->setMinimumHeight(size*12);
    slider->setMaximumHeight(size*12);
    slider->setMinimumWidth(size*1.5);
    slider->setMaximumWidth(size*1.5);
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

#ifndef CANTATA_ANDROID
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
#endif

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

#ifdef ENABLE_KDE_SUPPORT
#define SET_SHORTCUT(ACT, S) ACT->setShortcut(S)
#else
#define SET_SHORTCUT(ACT, S) ACT->setShortcut(S); addAction(ACT); ACT->setShortcutContext(Qt::ApplicationShortcut)
#endif

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
    #ifndef CANTATA_ANDROID
    , trayItem(0)
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    , notification(0)
    #endif
    , lyricsNeedUpdating(false)
    #ifdef ENABLE_WEBKIT
    , infoNeedsUpdating(false)
    #endif
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    , dock(0)
    , mpris(0)
    #endif
    , playQueueSearchTimer(0)
    , usingProxy(false)
    , connectedState(CS_Init)
    , volumeFade(0)
    , origVolume(0)
    , lastVolume(0)
    , stopState(StopState_None)
    #ifdef PHONON_FOUND
    , phononStreamEnabled(false)
    , phononStream(0)
    #endif
{
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
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

    #ifndef ENABLE_KDE_SUPPORT
    appIcon=QIcon(":cantata.svg");
    appIcon.addFile(":cantata16.png");
    appIcon.addFile(":cantata22.png");
    appIcon.addFile(":cantata32.png");
    appIcon.addFile(":cantata48.png");
    appIcon.addFile(":cantata64.png");
    setWindowIcon(appIcon);
    QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::Instance());
    #endif

    #ifdef ENABLE_KDE_SUPPORT
    prefAction=KStandardAction::preferences(this, SLOT(showPreferencesDialog()), actionCollection());
    quitAction=KStandardAction::quit(kapp, SLOT(quit()), actionCollection());

    #ifndef CANTATA_ANDROID
    smallPlaybackButtonsAction = actionCollection()->addAction("smallplaybackbuttons");
    smallPlaybackButtonsAction->setText(i18n("Small Playback Buttons"));

    smallControlButtonsAction = actionCollection()->addAction("smallcontrolbuttons");
    smallControlButtonsAction->setText(i18n("Small Control Buttons"));
    #endif

    connectAction = actionCollection()->addAction("connect");
    connectAction->setText(i18n("Connect"));

    connectionsAction = actionCollection()->addAction("connections");
    connectionsAction->setText(i18n("Connection"));

    outputsAction = actionCollection()->addAction("outputs");
    outputsAction->setText(i18n("Outputs"));

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

    addToPlayQueueAction = actionCollection()->addAction("addtoplaylist");
    addToPlayQueueAction->setText(i18n("Add To Play Queue"));

    addToStoredPlaylistAction = actionCollection()->addAction("addtostoredplaylist");
    addToStoredPlaylistAction->setText(i18n("Add To Playlist"));

    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction = actionCollection()->addAction("copytodevice");
    copyToDeviceAction->setText(i18n("Copy To Device"));
    copyToDeviceAction->setIcon(Icon("multimedia-player"));
    deleteSongsAction = actionCollection()->addAction("deletesongs");
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction = actionCollection()->addAction("replaygain");
    #endif

    backAction = actionCollection()->addAction("back");
    backAction->setText(i18n("Back"));

    removeAction = actionCollection()->addAction("removeitems");
    removeAction->setText(i18n("Remove"));

    replacePlayQueueAction = actionCollection()->addAction("replaceplaylist");
    replacePlayQueueAction->setText(i18n("Replace Play Queue"));

    removeFromPlayQueueAction = actionCollection()->addAction("removefromplaylist");
    removeFromPlayQueueAction->setText(i18n("Remove From Play Queue"));

    copyTrackInfoAction = actionCollection()->addAction("copytrackinfo");
    copyTrackInfoAction->setText(i18n("Copy Track Info"));

    cropPlayQueueAction = actionCollection()->addAction("cropplaylist");
    cropPlayQueueAction->setText(i18n("Crop"));

    shufflePlayQueueAction = actionCollection()->addAction("shuffleplaylist");
    shufflePlayQueueAction->setText(i18n("Shuffle"));

    savePlayQueueAction = actionCollection()->addAction("saveplaylist");
    savePlayQueueAction->setText(i18n("Save As"));

    clearPlayQueueAction = actionCollection()->addAction("clearplaylist");
    clearPlayQueueAction->setText(i18n("Clear"));

    #ifndef CANTATA_ANDROID
    expandInterfaceAction = actionCollection()->addAction("expandinterface");
    expandInterfaceAction->setText(i18n("Expanded Interface"));
    #endif

    randomPlayQueueAction = actionCollection()->addAction("randomplaylist");
    randomPlayQueueAction->setText(i18n("Random"));

    repeatPlayQueueAction = actionCollection()->addAction("repeatplaylist");
    repeatPlayQueueAction->setText(i18n("Repeat"));

    singlePlayQueueAction = actionCollection()->addAction("singleplaylist");
    singlePlayQueueAction->setText(i18n("Single"));
    singlePlayQueueAction->setWhatsThis(i18n("When 'Single' is activated, playback is stopped after current song, or song is repeated if 'Repeat' is enabled."));

    consumePlayQueueAction = actionCollection()->addAction("consumeplaylist");
    consumePlayQueueAction->setText(i18n("Consume"));
    consumePlayQueueAction->setWhatsThis(i18n("When consume is activated, a song is removed from the play queue after it has been played."));

    addWithPriorityAction = actionCollection()->addAction("addwithprio");
    addWithPriorityAction->setText(i18n("Add With Priority"));

    setPriorityAction = actionCollection()->addAction("setprio");
    setPriorityAction->setText(i18n("Set Priority"));

    addPrioHighestAction = actionCollection()->addAction("highestprio");
    addPrioHighestAction->setText(i18n("Highest Priority (255)"));

    addPrioHighAction = actionCollection()->addAction("highprio");
    addPrioHighAction->setText(i18n("High Priority (200)"));

    addPrioMediumAction = actionCollection()->addAction("mediumprio");
    addPrioMediumAction->setText(i18n("Medium Priority (125)"));

    addPrioLowAction = actionCollection()->addAction("lowprio");
    addPrioLowAction->setText(i18n("Low Priority (50)"));

    addPrioDefaultAction = actionCollection()->addAction("defaultprio");
    addPrioDefaultAction->setText(i18n("Default Priority (0)"));

    addPrioCustomAction = actionCollection()->addAction("customprio");
    addPrioCustomAction->setText(i18n("Custom Priority..."));

    #ifdef PHONON_FOUND
    streamPlayAction = actionCollection()->addAction("streamplay");
    streamPlayAction->setText(i18n("Play Stream"));
    streamPlayAction->setWhatsThis(i18n("When 'Play Stream' is activated, the enabled stream is played locally."));
    #endif

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

    #ifdef TAGLIB_FOUND
    organiseFilesAction = actionCollection()->addAction("organizefiles");

    editTagsAction = actionCollection()->addAction("edittags");
    editTagsAction->setText(i18n("Edit Tags"));

    editPlayQueueTagsAction = actionCollection()->addAction("editpqtags");
    editPlayQueueTagsAction->setText(i18n("Edit Song Tags"));
    #endif

    showPlayQueueAction = actionCollection()->addAction("showplayqueue");
    showPlayQueueAction->setText(i18n("Play Queue"));

    libraryTabAction = actionCollection()->addAction("showlibrarytab");
    libraryTabAction->setText(i18n("Library"));

    albumsTabAction = actionCollection()->addAction("showalbumstab");
    albumsTabAction->setText(i18n("Albums"));

    foldersTabAction = actionCollection()->addAction("showfolderstab");
    foldersTabAction->setText(i18n("Folders"));

    playlistsTabAction = actionCollection()->addAction("showplayliststab");
    playlistsTabAction->setText(i18n("Playlists"));

    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    dynamicTabAction = actionCollection()->addAction("showdynamictab");
    dynamicTabAction->setText(i18n("Dynamic"));
    #endif

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

    searchAction = actionCollection()->addAction("search");
    searchAction->setText(i18n("Search"));

    expandAllAction = actionCollection()->addAction("expandall");
    expandAllAction->setText(i18n("Expand All"));

    collapseAllAction = actionCollection()->addAction("collapseall");
    collapseAllAction->setText(i18n("Collapse All"));
    #else
    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered(bool)), qApp, SLOT(quit()));
    quitAction->setIcon(Icon("application-exit"));
    quitAction->setShortcut(QKeySequence::Quit);
    #ifndef CANTATA_ANDROID
    smallPlaybackButtonsAction = new QAction(tr("Small Playback Buttons"), this);
    smallControlButtonsAction = new QAction(tr("Small Control Buttons"), this);
    #endif
    refreshAction = new QAction(tr("Refresh"), this);
    connectAction = new QAction(tr("Connect"), this);
    connectionsAction = new QAction(tr("Connections"), this);
    outputsAction = new QAction(tr("Outputs"), this);
    prevTrackAction = new QAction(tr("Previous Track"), this);
    nextTrackAction = new QAction(tr("Next Track"), this);
    playPauseTrackAction = new QAction(tr("Play/Pause"), this);
    stopTrackAction = new QAction(tr("Stop"), this);
    increaseVolumeAction = new QAction(tr("Increase Volume"), this);
    decreaseVolumeAction = new QAction(tr("Decrease Volume"), this);
    addToPlayQueueAction = new QAction(tr("Add To Play Queue"), this);
    addToStoredPlaylistAction = new QAction(tr("Add To Playlist"), this);
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction = new QAction(this);
    #endif
    backAction = new QAction(tr("Back"), this);
    removeAction = new QAction(tr("Remove"), this);
    replacePlayQueueAction = new QAction(tr("Replace Play Queue"), this);
    removeFromPlayQueueAction = new QAction(tr("Remove From Play Queue"), this);
    copyTrackInfoAction = new QAction(tr("Copy Track Info"), this);
    cropPlayQueueAction = new QAction(tr("Crop"), this);
    shufflePlayQueueAction = new QAction(tr("Shuffle"), this);
    savePlayQueueAction = new QAction(tr("Save As"), this);
    clearPlayQueueAction = new QAction(tr("Clear"), this);
    #ifndef CANTATA_ANDROID
    expandInterfaceAction = new QAction(tr("Expanded Interface"), this);
    #endif
    randomPlayQueueAction = new QAction(tr("Random"), this);
    repeatPlayQueueAction = new QAction(tr("Repeat"), this);
    singlePlayQueueAction = new QAction(tr("Single"), this);
    singlePlayQueueAction->setWhatsThis(tr("When single is activated, playback is stopped after current song, or song is repeated if the 'repeat' mode is enabled."));
    consumePlayQueueAction = new QAction(tr("Consume"), this);
    consumePlayQueueAction->setWhatsThis(tr("When consume is activated, a song is removed from the play queue after it has been played."));
    addWithPriorityAction = new QAction(tr("Add With Priority"), this);
    setPriorityAction = new QAction(tr("Set Priority"), this);
    addPrioHighestAction = new QAction(tr("Highest Priority (255)"), this);
    addPrioHighAction = new QAction(tr("High Priority (200)"), this);
    addPrioMediumAction = new QAction(tr("Medium Priority (125)"), this);
    addPrioLowAction = new QAction(tr("Low Priority (50)"), this);
    addPrioDefaultAction = new QAction(tr("Default Priority (0)"), this);
    addPrioCustomAction = new QAction(tr("Custom Priority..."), this);
    #ifdef PHONON_FOUND
    streamPlayAction= new QAction(tr("Play Stream"), this);
    streamPlayAction->setWhatsThis(tr("When 'Play Stream' is activated, the enabled stream is played locally."));
    #endif
    locateTrackAction = new QAction(tr("Locate In Library"), this);
//     burnAction = new QAction(tr("Burn To CD/DVD"), this);
//     createAudioCdAction = new QAction(tr("Create Audio CD"), this);
//     createDataCdAction = new QAction(tr("Create Data CD"), this);
    #ifdef TAGLIB_FOUND
    organiseFilesAction = new QAction(this);
    editTagsAction = new QAction(tr("Edit Tags"), this);
    editPlayQueueTagsAction = new QAction(tr("Edit Song Tags"), this);
    #endif
    searchAction = new QAction(tr("Search"), this);
    expandAllAction = new QAction(tr("Expand All"), this);
    collapseAllAction = new QAction(tr("Collapse All"), this);
    showPlayQueueAction = new QAction(tr("Play Queue"), this);
    libraryTabAction = new QAction(tr("Library"), this);
    albumsTabAction = new QAction(tr("Albums"), this);
    foldersTabAction = new QAction(tr("Folders"), this);
    playlistsTabAction = new QAction(tr("Playlists"), this);
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    dynamicTabAction = new QAction(tr("Dynamic"), this);
    #endif
    lyricsTabAction = new QAction(tr("Lyrics"), this);
    streamsTabAction = new QAction(tr("Streams"), this);
    #ifdef ENABLE_WEBKIT
    infoTabAction = new QAction(tr("Info"), this);
    #endif // ENABLE_WEBKIT
    serverInfoTabAction = new QAction(tr("Server Info"), this);
    #endif // ENABLE_KDE_SUPPORT

    SET_SHORTCUT(copyTrackInfoAction, QKeySequence::Copy);
    SET_SHORTCUT(backAction, QKeySequence::Back);

    int pageKey=Qt::Key_1;
    SET_SHORTCUT(showPlayQueueAction, Qt::AltModifier+Qt::Key_Q);
    SET_SHORTCUT(libraryTabAction, Qt::AltModifier+nextKey(pageKey));
    SET_SHORTCUT(albumsTabAction, Qt::AltModifier+nextKey(pageKey));
    SET_SHORTCUT(foldersTabAction, Qt::AltModifier+nextKey(pageKey));
    SET_SHORTCUT(playlistsTabAction, Qt::AltModifier+nextKey(pageKey));
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    SET_SHORTCUT(dynamicTabAction, Qt::AltModifier+nextKey(pageKey));
    #endif
    SET_SHORTCUT(streamsTabAction, Qt::AltModifier+nextKey(pageKey));
    SET_SHORTCUT(lyricsTabAction, Qt::AltModifier+nextKey(pageKey));

    #ifdef ENABLE_WEBKIT
    SET_SHORTCUT(infoTabAction, Qt::AltModifier+nextKey(pageKey));
    #endif // ENABLE_WEBKIT
    SET_SHORTCUT(serverInfoTabAction, Qt::AltModifier+nextKey(pageKey));
    #ifdef ENABLE_DEVICES_SUPPORT
    SET_SHORTCUT(devicesTabAction, Qt::AltModifier+nextKey(pageKey));
    #endif // ENABLE_DEVICES_SUPPORT

    SET_SHORTCUT(searchAction, Qt::ControlModifier+Qt::Key_F);
    SET_SHORTCUT(expandAllAction, Qt::ControlModifier+Qt::Key_Plus);
    SET_SHORTCUT(collapseAllAction, Qt::ControlModifier+Qt::Key_Minus);
    // Setup event handler for volume adjustment
    volumeSliderEventHandler = new VolumeSliderEventHandler(this);

    volumeControl = new VolumeControl(volumeButton);
    volumeControl->installSliderEventFilter(volumeSliderEventHandler);
    volumeButton->installEventFilter(volumeSliderEventHandler);

    positionSlider->setStyle(new ProxyStyle());

    playbackPlay = MediaIcon("media-playback-start");
    playbackPause = MediaIcon("media-playback-pause");
    randomPlayQueueAction->setIcon(Icon("media-playlist-shuffle"));
    locateTrackAction->setIcon(Icon("edit-find"));
    #if defined ENABLE_KDE_SUPPORT || defined CANTATA_ANDROID
    consumePlayQueueAction->setIcon(Icon("cantata-view-media-consume"));
    repeatPlayQueueAction->setIcon(Icon("cantata-view-media-repeat"));
//     randomPlayQueueAction->setIcon(Icon("cantata-view-media-shuffle"));
//     shufflePlayQueueAction->setIcon(Icon("cantata-view-media-shuffle"));
    #else
    QIcon consumeIcon(":consume16.png");
    consumeIcon.addFile(":consume22.png");
    QIcon repeatIcon(":repeat16.png");
    repeatIcon.addFile(":repeat22.png");
//     QIcon shuffleIcon(":shuffle16.png");
//     repeatIcon.addFile(":shuffle22.png");
    consumePlayQueueAction->setIcon(consumeIcon);
    repeatPlayQueueAction->setIcon(repeatIcon);
//     randomPlayQueueAction->setIcon(shuffleIcon);
//     shufflePlayQueueAction->setIcon(shuffleIcon);
    #endif
    singlePlayQueueAction->setIcon(createSingleIcon());
    #ifdef PHONON_FOUND
    streamPlayAction->setIcon(Icon(DEFAULT_STREAM_ICON));
    #endif
    backAction->setIcon(Icon("go-previous"));
    removeAction->setIcon(Icon("list-remove"));
    addToPlayQueueAction->setIcon(Icon("list-add"));
    replacePlayQueueAction->setIcon(Icon("media-playback-start"));

//     burnAction->setIcon(Icon("tools-media-optical-burn"));
//     createDataCdAction->setIcon(Icon("media-optical"));
//     createAudioCdAction->setIcon(Icon(DEFAULT_ALBUM_ICON));
    #ifdef TAGLIB_FOUND
    editTagsAction->setIcon(Icon("document-edit"));
    editPlayQueueTagsAction->setIcon(Icon("document-edit"));
    organiseFilesAction->setIcon(Icon("inode-directory"));
    organiseFilesAction->setText(i18n("Organize Files"));
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

    prevTrackAction->setIcon(MediaIcon("media-skip-backward"));
    nextTrackAction->setIcon(MediaIcon("media-skip-forward"));
    playPauseTrackAction->setIcon(playbackPlay);
    stopTrackAction->setIcon(MediaIcon("media-playback-stop"));
    removeFromPlayQueueAction->setIcon(Icon("list-remove"));
    clearPlayQueueAction->setIcon(Icon("edit-clear-list"));
    savePlayQueueAction->setIcon(Icon("document-save-as"));
    #ifndef CANTATA_ANDROID
    expandInterfaceAction->setIcon(Icon("view-media-playlist"));
    #endif
    refreshAction->setIcon(Icon("view-refresh"));
    connectAction->setIcon(Icon("network-connect"));
    connectionsAction->setIcon(Icon("network-server"));
    outputsAction->setIcon(Icon("speaker"));
    connectionsAction->setMenu(new QMenu(this));
    connectionsGroup=new QActionGroup(connectionsAction->menu());
    outputsAction->setMenu(new QMenu(this));
    outputsAction->setVisible(false);
    showPlayQueueAction->setIcon(Icon("media-playback-start"));
    #if defined ENABLE_KDE_SUPPORT || defined CANTATA_ANDROID
    libraryTabAction->setIcon(Icon("cantata-view-media-library"));
    #else
    QIcon libraryIcon(":lib16.png");
    libraryIcon.addFile(":lib32.png");
    libraryTabAction->setIcon(libraryIcon);
    #endif
    albumsTabAction->setIcon(Icon(DEFAULT_ALBUM_ICON));
    foldersTabAction->setIcon(Icon("inode-directory"));
    playlistsTabAction->setIcon(Icon("view-media-playlist"));
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    dynamicTabAction->setIcon(Icon("media-playlist-shuffle"));
    #endif
    lyricsTabAction->setIcon(Icon("view-media-lyrics"));
    streamsTabAction->setIcon(Icon(DEFAULT_STREAM_ICON));
    #ifdef ENABLE_WEBKIT
    #if defined ENABLE_KDE_SUPPORT || defined CANTATA_ANDROID
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
    #endif
    searchAction->setIcon(Icon("edit-find"));
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction->setIcon(Icon("audio-x-generic"));
    replaygainAction->setText(i18n("ReplayGain"));
    #endif
    addToStoredPlaylistAction->setMenu(PlaylistsModel::self()->menu());
    addToStoredPlaylistAction->setIcon(playlistsTabAction->icon());

    menuButton->setIcon(Icon("configure"));
    volumeButton->setIcon(Icon("audio-volume-high"));

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
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    dynamicPage = new DynamicPage(this);
    #endif
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

    savePlayQueuePushButton->setDefaultAction(savePlayQueueAction);
    removeAllFromPlayQueuePushButton->setDefaultAction(clearPlayQueueAction);
    randomPushButton->setDefaultAction(randomPlayQueueAction);
    repeatPushButton->setDefaultAction(repeatPlayQueueAction);
    singlePushButton->setDefaultAction(singlePlayQueueAction);
    consumePushButton->setDefaultAction(consumePlayQueueAction);
    #ifdef PHONON_FOUND
    streamButton->setDefaultAction(streamPlayAction);
    #else
    streamButton->setVisible(false);
    #endif

    initSizes();

    QStringList hiddenPages=Settings::self()->hiddenPages();

    #define TAB_ACTION(A) A->icon(), A->text(), A->text()+"<br/><small><i>"+A->shortcut().toString()+"</i></small>"

    playQueuePage=new PlayQueuePage(this);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, playQueuePage);
    layout->setContentsMargins(0, 0, 0, 0);
    bool playQueueInSidebar=!hiddenPages.contains(playQueuePage->metaObject()->className());
    if (playQueueInSidebar && (Settings::self()->firstRun() || Settings::self()->version()<CANTATA_MAKE_VERSION(0, 8, 0))) {
        playQueueInSidebar=false;
    }
    tabWidget->AddTab(playQueuePage, TAB_ACTION(showPlayQueueAction), playQueueInSidebar);
    tabWidget->AddTab(libraryPage, TAB_ACTION(libraryTabAction), !hiddenPages.contains(libraryPage->metaObject()->className()));
    tabWidget->AddTab(albumsPage, TAB_ACTION(albumsTabAction), !hiddenPages.contains(albumsPage->metaObject()->className()));
    tabWidget->AddTab(folderPage, TAB_ACTION(foldersTabAction), !hiddenPages.contains(folderPage->metaObject()->className()));
    tabWidget->AddTab(playlistsPage, TAB_ACTION(playlistsTabAction), !hiddenPages.contains(playlistsPage->metaObject()->className()));
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    tabWidget->AddTab(dynamicPage, TAB_ACTION(dynamicTabAction), !hiddenPages.contains(dynamicPage->metaObject()->className()));
    #endif
    tabWidget->AddTab(streamsPage, TAB_ACTION(streamsTabAction), !hiddenPages.contains(streamsPage->metaObject()->className()));
    tabWidget->AddTab(lyricsPage, TAB_ACTION(lyricsTabAction), !hiddenPages.contains(lyricsPage->metaObject()->className()));
    #ifdef ENABLE_WEBKIT
    tabWidget->AddTab(infoPage, TAB_ACTION(infoTabAction), !hiddenPages.contains(infoPage->metaObject()->className()));
    #endif
    tabWidget->AddTab(serverInfoPage, TAB_ACTION(serverInfoTabAction), !hiddenPages.contains(serverInfoPage->metaObject()->className()));
    #ifdef ENABLE_DEVICES_SUPPORT
    tabWidget->AddTab(devicesPage, TAB_ACTION(devicesTabAction), !hiddenPages.contains(devicesPage->metaObject()->className()));
    DevicesModel::self()->setEnabled(!hiddenPages.contains(devicesPage->metaObject()->className()));
    copyToDeviceAction->setVisible(DevicesModel::self()->isEnabled());
    #endif
    AlbumsModel::self()->setEnabled(!hiddenPages.contains(albumsPage->metaObject()->className()));
    folderPage->setEnabled(!hiddenPages.contains(folderPage->metaObject()->className()));
    streamsPage->setEnabled(!hiddenPages.contains(streamsPage->metaObject()->className()));

    #ifndef CANTATA_ANDROID
    autoHideSplitterAction=new QAction(i18n("Auto Hide"), this);
    autoHideSplitterAction->setCheckable(true);
    autoHideSplitterAction->setChecked(Settings::self()->splitterAutoHide());
    tabWidget->addMenuAction(autoHideSplitterAction);
    connect(autoHideSplitterAction, SIGNAL(toggled(bool)), this, SLOT(toggleSplitterAutoHide()));
    #endif

    if (playQueueInSidebar) {
        tabToggled(PAGE_PLAYQUEUE);
    } else {
        tabWidget->SetCurrentIndex(PAGE_LIBRARY);
    }

    #ifdef CANTATA_ANDROID
    tabWidget->SetMode(FancyTabWidget::Mode_IconOnlyLargeSidebar);
    tabWidget->ToggleTab(PAGE_PLAYQUEUE, false);
    showTab(PAGE_LIBRARY);
    setLayout(width()>height());
    #else
    tabWidget->SetMode(FancyTabWidget::Mode_LargeSidebar);
    expandInterfaceAction->setCheckable(true);
    #endif
    randomPlayQueueAction->setCheckable(true);
    repeatPlayQueueAction->setCheckable(true);
    singlePlayQueueAction->setCheckable(true);
    consumePlayQueueAction->setCheckable(true);
    #ifdef PHONON_FOUND
    streamPlayAction->setCheckable(true);
    #endif

    searchPlayQueueLineEdit->setPlaceholderText(i18n("Search Play Queue..."));
    QList<QToolButton *> playbackBtns;
    QList<QToolButton *> controlBtns;
    QList<QToolButton *> btns;
    playbackBtns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    controlBtns << volumeButton << menuButton << streamButton;
    btns << repeatPushButton << singlePushButton << randomPushButton << savePlayQueuePushButton << removeAllFromPlayQueuePushButton << consumePushButton;

    foreach (QToolButton *b, btns) {
        Icon::init(b);
    }

    #ifndef CANTATA_ANDROID
    smallControlButtonsAction->setCheckable(true);
    smallControlButtonsAction->setChecked(Settings::self()->smallControlButtons());
    controlBtnsMenu = new QMenu(this);
    controlBtnsMenu->addAction(smallControlButtonsAction);
    connect(smallControlButtonsAction, SIGNAL(triggered(bool)), SLOT(setControlButtonsSize(bool)));
    #endif
    foreach (QToolButton *b, controlBtns) {
        b->setAutoRaise(true);
        #ifndef CANTATA_ANDROID
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(controlButtonsMenu()));
        #endif
    }

    #ifndef CANTATA_ANDROID
    smallPlaybackButtonsAction->setCheckable(true);
    smallPlaybackButtonsAction->setChecked(Settings::self()->smallPlaybackButtons());
    playbackBtnsMenu = new QMenu(this);
    playbackBtnsMenu->addAction(smallPlaybackButtonsAction);
    connect(smallPlaybackButtonsAction, SIGNAL(triggered(bool)), SLOT(setPlaybackButtonsSize(bool)));
    #endif
    foreach (QToolButton *b, playbackBtns) {
        b->setAutoRaise(true);
        #ifndef CANTATA_ANDROID
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(b, SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(playbackButtonsMenu()));
        #endif
    }

    setPlaybackButtonsSize(Settings::self()->smallPlaybackButtons());
    setControlButtonsSize(Settings::self()->smallControlButtons());

    trackLabel->setText(QString());
    artistLabel->setText(QString());

    #ifndef CANTATA_ANDROID
    expandInterfaceAction->setChecked(Settings::self()->showPlaylist());
    #endif
    randomPlayQueueAction->setChecked(false);
    repeatPlayQueueAction->setChecked(false);
    singlePlayQueueAction->setChecked(false);
    consumePlayQueueAction->setChecked(false);
    #ifdef PHONON_FOUND
    streamPlayAction->setChecked(false);
    #endif

    MusicLibraryItemAlbum::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->libraryCoverSize());
    MusicLibraryItemAlbum::setShowDate(Settings::self()->libraryYear());
    AlbumsModel::setCoverSize((MusicLibraryItemAlbum::CoverSize)Settings::self()->albumsCoverSize());

    #ifndef CANTATA_ANDROID
    tabWidget->SetMode((FancyTabWidget::Mode)Settings::self()->sidebar());
    setupTrayIcon();
    expandedSize=Settings::self()->mainWindowSize();
    collapsedSize=Settings::self()->mainWindowCollapsedSize();
    if (!expandedSize.isEmpty()) {
        resize(expandedSize);
    }
    togglePlayQueue();
    if (expandInterfaceAction->isChecked()) {
        if (!expandedSize.isEmpty()) {
            resize(expandedSize);
        }
    } else {
         if (!collapsedSize.isEmpty()) {
            resize(collapsedSize);
        }
    }
    #endif
    #ifdef ENABLE_KDE_SUPPORT
    setupGUI(KXmlGuiWindow::Keys | KXmlGuiWindow::Save | KXmlGuiWindow::Create);
    menuBar()->setVisible(false);
    #endif

    #ifndef CANTATA_ANDROID
    mainMenu->addAction(expandInterfaceAction);
    #endif
    mainMenu->addAction(connectionsAction);
    mainMenu->addAction(outputsAction);
    QAction *menuAct=mainMenu->addAction(tr("Configure Cantata..."), this, SLOT(showPreferencesDialog()));
    menuAct->setIcon(Icon("configure"));
    #ifdef ENABLE_KDE_SUPPORT
    mainMenu->addAction(actionCollection()->action(KStandardAction::name(KStandardAction::KeyBindings)));
    mainMenu->addSeparator();
    mainMenu->addMenu(helpMenu());
    #else

    prefAction=menuAct;
    mainMenu->addSeparator();
//     QMenu *menu=new QMenu(tr("Help"), this);
//     QAction *menuAct=menu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
    menuAct=mainMenu->addAction(tr("About Cantata..."), this, SLOT(showAboutDialog()));
    menuAct->setIcon(appIcon);
//     mainMenu->addMenu(menu);
    #endif
    mainMenu->addSeparator();
    mainMenu->addAction(quitAction);

    #ifndef CANTATA_ANDROID
    coverWidget->installEventFilter(new CoverEventHandler(this));
    #endif
    dynamicLabel->setVisible(false);

    addWithPriorityAction->setIcon(Icon("favorites"));
    setPriorityAction->setIcon(Icon("favorites"));
    addWithPriorityAction->setVisible(false);
    setPriorityAction->setVisible(false);
    addPrioHighestAction->setData(255);
    addPrioHighAction->setData(200);
    addPrioMediumAction->setData(125);
    addPrioLowAction->setData(50);
    addPrioDefaultAction->setData(0);
    addPrioCustomAction->setData(-1);
    QMenu *prioMenu=new QMenu(this);
    prioMenu->addAction(addPrioHighestAction);
    prioMenu->addAction(addPrioHighAction);
    prioMenu->addAction(addPrioMediumAction);
    prioMenu->addAction(addPrioLowAction);
    prioMenu->addAction(addPrioDefaultAction);
    prioMenu->addAction(addPrioCustomAction);
    addWithPriorityAction->setMenu(prioMenu);
    setPriorityAction->setMenu(prioMenu);

    // Ensure these objects are created in the GUI thread...
    MPDStatus::self();
    MPDStats::self();

    connect(addPrioHighestAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(addPrioHighAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(addPrioMediumAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(addPrioLowAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(addPrioDefaultAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));
    connect(addPrioCustomAction, SIGNAL(triggered(bool)), this, SLOT(addWithPriority()));

    connect(MPDConnection::self(), SIGNAL(playlistLoaded(const QString &)), SLOT(songLoaded()));
    connect(MPDConnection::self(), SIGNAL(added(const QStringList &)), SLOT(songLoaded()));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(outputsUpdated(const QList<Output> &)));
    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
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
    connect(this, SIGNAL(setDetails(const MPDConnectionDetails &)), MPDConnection::self(), SLOT(setDetails(const MPDConnectionDetails &)));
    connect(this, SIGNAL(setPriority(const QList<quint32> &, quint8 )), MPDConnection::self(), SLOT(setPriority(const QList<quint32> &, quint8)));
    connect(&playQueueModel, SIGNAL(statsUpdated(int, quint32)), this, SLOT(updatePlayQueueStats(int, quint32)));

    playQueueProxyModel.setSourceModel(&playQueueModel);
    playQueue->setModel(&playQueueModel);
    playQueue->addAction(removeFromPlayQueueAction);
    playQueue->addAction(clearPlayQueueAction);
    playQueue->addAction(savePlayQueueAction);
    playQueue->addAction(cropPlayQueueAction);
    playQueue->addAction(shufflePlayQueueAction);
    Action *sep2=new Action(this);
    sep2->setSeparator(true);
    playQueue->addAction(sep2);
    playQueue->addAction(setPriorityAction);
    Action *sep=new Action(this);
    sep->setSeparator(true);
    playQueue->addAction(sep);
    playQueue->addAction(locateTrackAction);
    #ifdef TAGLIB_FOUND
    playQueue->addAction(editPlayQueueTagsAction);
    #endif
    //playQueue->addAction(copyTrackInfoAction);
    playQueue->tree()->installEventFilter(new DeleteKeyEventHandler(playQueue->tree(), removeFromPlayQueueAction));
    playQueue->list()->installEventFilter(new DeleteKeyEventHandler(playQueue->list(), removeFromPlayQueueAction));
    connect(playQueue, SIGNAL(itemsSelected(bool)), SLOT(playQueueItemsSelected(bool)));
    connect(streamsPage, SIGNAL(add(const QStringList &, bool, quint8)), &playQueueModel, SLOT(addItems(const QStringList &, bool, quint8)));
    playQueueModel.setGrouped(Settings::self()->playQueueGrouped());
    playQueue->setGrouped(Settings::self()->playQueueGrouped());
    playQueue->setAutoExpand(Settings::self()->playQueueAutoExpand());
    playQueue->setStartClosed(Settings::self()->playQueueStartClosed());
    playlistsPage->setStartClosed(Settings::self()->playListsStartClosed());

    connect(MPDStats::self(), SIGNAL(updated()), this, SLOT(updateStats()));
    connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(updateStatus()));
    connect(MPDConnection::self(), SIGNAL(playlistUpdated(const QList<Song> &)), this, SLOT(updatePlayQueue(const QList<Song> &)));
    connect(MPDConnection::self(), SIGNAL(currentSongUpdated(const Song &)), this, SLOT(updateCurrentSong(const Song &)));
    connect(MPDConnection::self(), SIGNAL(storedPlayListUpdated()), MPDConnection::self(), SLOT(listPlaylists()));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), SLOT(mpdConnectionStateChanged(bool)));
    connect(MPDConnection::self(), SIGNAL(error(const QString &, bool)), SLOT(showError(const QString &, bool)));
    connect(MPDConnection::self(), SIGNAL(info(const QString &)), SLOT(showInformation(const QString &)));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(checkMpdDir()));
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    connect(Dynamic::self(), SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(Dynamic::self(), SIGNAL(running(bool)), dynamicLabel, SLOT(setVisible(bool)));
    #endif
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
    connect(positionSlider, SIGNAL(sliderReleased()), this, SLOT(setPosition()));
    connect(randomPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRandom(bool)));
    connect(repeatPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setRepeat(bool)));
    connect(singlePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setSingle(bool)));
    connect(consumePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(setConsume(bool)));
    #ifdef PHONON_FOUND
    connect(streamPlayAction, SIGNAL(triggered(bool)), this, SLOT(toggleStream(bool)));
    #endif
    connect(backAction, SIGNAL(triggered(bool)), this, SLOT(goBack()));
    connect(searchPlayQueueLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPlayQueue()));
    connect(searchPlayQueueLineEdit, SIGNAL(textChanged(const QString)), this, SLOT(searchPlayQueue()));
    connect(playQueue, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(playQueueItemActivated(const QModelIndex &)));
    connect(removeAction, SIGNAL(triggered(bool)), this, SLOT(removeItems()));
    connect(addToPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(addToPlayQueue()));
    connect(replacePlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(replacePlayQueue()));
    connect(removeFromPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(removeFromPlayQueue()));
    connect(clearPlayQueueAction, SIGNAL(triggered(bool)), searchPlayQueueLineEdit, SLOT(clear()));
    connect(clearPlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(clear()));
    connect(copyTrackInfoAction, SIGNAL(triggered(bool)), this, SLOT(copyTrackInfo()));
    connect(cropPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(cropPlayQueue()));
    connect(shufflePlayQueueAction, SIGNAL(triggered(bool)), MPDConnection::self(), SLOT(shuffle()));
    #ifndef CANTATA_ANDROID
    connect(expandInterfaceAction, SIGNAL(triggered(bool)), this, SLOT(togglePlayQueue()));
    #endif
    connect(positionSlider, SIGNAL(valueChanged(int)), this, SLOT(updatePosition()));
    connect(volumeButton, SIGNAL(clicked()), SLOT(showVolumeControl()));
//     connect(createDataCdAction, SIGNAL(triggered(bool)), this, SLOT(createDataCd()));
//     connect(createAudioCdAction, SIGNAL(triggered(bool)), this, SLOT(createAudioCd()));
    #ifdef TAGLIB_FOUND
    connect(editTagsAction, SIGNAL(triggered(bool)), this, SLOT(editTags()));
    connect(editPlayQueueTagsAction, SIGNAL(triggered(bool)), this, SLOT(editPlayQueueTags()));
    connect(organiseFilesAction, SIGNAL(triggered(bool)), SLOT(organiseFiles()));
    #endif
    connect(locateTrackAction, SIGNAL(triggered(bool)), this, SLOT(locateTrack()));
    connect(showPlayQueueAction, SIGNAL(triggered(bool)), this, SLOT(showPlayQueue()));
    connect(libraryTabAction, SIGNAL(triggered(bool)), this, SLOT(showLibraryTab()));
    connect(albumsTabAction, SIGNAL(triggered(bool)), this, SLOT(showAlbumsTab()));
    connect(foldersTabAction, SIGNAL(triggered(bool)), this, SLOT(showFoldersTab()));
    connect(playlistsTabAction, SIGNAL(triggered(bool)), this, SLOT(showPlaylistsTab()));
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    connect(dynamicTabAction, SIGNAL(triggered(bool)), this, SLOT(showDynamicTab()));
    #endif
    connect(lyricsTabAction, SIGNAL(triggered(bool)), this, SLOT(showLyricsTab()));
    connect(streamsTabAction, SIGNAL(triggered(bool)), this, SLOT(showStreamsTab()));
    #ifdef ENABLE_WEBKIT
    connect(infoTabAction, SIGNAL(triggered(bool)), this, SLOT(showInfoTab()));
    #endif
    connect(serverInfoTabAction, SIGNAL(triggered(bool)), this, SLOT(showServerInfoTab()));
    connect(searchAction, SIGNAL(triggered(bool)), this, SLOT(focusSearch()));
    connect(expandAllAction, SIGNAL(triggered(bool)), this, SLOT(expandAll()));
    connect(collapseAllAction, SIGNAL(triggered(bool)), this, SLOT(collapseAll()));
    #ifdef ENABLE_DEVICES_SUPPORT
    connect(devicesTabAction, SIGNAL(triggered(bool)), this, SLOT(showDevicesTab()));
    connect(DevicesModel::self(), SIGNAL(addToDevice(const QString &)), this, SLOT(addToDevice(const QString &)));
    connect(DevicesModel::self(), SIGNAL(error(const QString &)), this, SLOT(showError(const QString &)));
    connect(libraryPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(devicesPage, SIGNAL(addToDevice(const QString &, const QString &, const QList<Song> &)), SLOT(copyToDevice(const QString &, const QString &, const QList<Song> &)));
    connect(deleteSongsAction, SIGNAL(triggered(bool)), SLOT(deleteSongs()));
    connect(devicesPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(libraryPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(albumsPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    connect(folderPage, SIGNAL(deleteSongs(const QString &, const QList<Song> &)), SLOT(deleteSongs(const QString &, const QList<Song> &)));
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    connect(replaygainAction, SIGNAL(triggered(bool)), SLOT(replayGain()));
    #endif
    connect(PlaylistsModel::self(), SIGNAL(addToNew()), this, SLOT(addToNewStoredPlaylist()));
    connect(PlaylistsModel::self(), SIGNAL(addToExisting(const QString &)), this, SLOT(addToExistingStoredPlaylist(const QString &)));
    connect(playlistsPage, SIGNAL(add(const QStringList &, bool, quint8)), &playQueueModel, SLOT(addItems(const QStringList &, bool, quint8)));
    connect(coverWidget, SIGNAL(coverImage(const QImage &)), lyricsPage, SLOT(setImage(const QImage &)));

    #ifdef CANTATA_ANDROID
    tabWidget->setMinimumWidth((width()*0.4)-tabWidget->tabSize().width());
    controlLayout->setSpacing(playPauseTrackButton->width()*0.1);
    #else
    if (!playQueueInSidebar) {
        QByteArray state=Settings::self()->splitterState();

        if (state.isEmpty()) {

            QList<int> sizes=QList<int>() << 250 << 500;
            splitter->setSizes(sizes);
            resize(800, 600);
        } else {
            splitter->restoreState(Settings::self()->splitterState());
        }
    }
    #endif

    playQueueItemsSelected(false);
    playQueue->setFocus();
    playQueue->initHeader();

    mpdThread=new QThread(this);
    MPDConnection::self()->moveToThread(mpdThread);
    mpdThread->start();
    connectToMpd();

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
    connect(tabWidget, SIGNAL(ModeChanged(FancyTabWidget::Mode)), this, SLOT(sidebarModeChanged()));
    connect(messageWidget, SIGNAL(visible(bool)), this, SLOT(messageWidgetVisibility(bool)));

    #ifndef CANTATA_ANDROID
    toggleSplitterAutoHide();
    #endif
    readSettings();
    updateConnectionsMenu();
    fadeStop=Settings::self()->stopFadeDuration()>Settings::MinFade;
    playlistsPage->refresh();
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    toggleMpris();
    if (Settings::self()->dockManager()) {
        QTimer::singleShot(250, this, SLOT(toggleDockManager()));
    }
    #endif
}

MainWindow::~MainWindow()
{
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    if (dock) {
        dock->setIcon(QString());
    }
    #endif
    #ifndef CANTATA_ANDROID
    Settings::self()->saveMainWindowSize(splitter->isVisible() ? size() : expandedSize);
    Settings::self()->saveMainWindowCollapsedSize(splitter->isVisible() ? collapsedSize : size());
    #if defined ENABLE_REMOTE_DEVICES && defined ENABLE_DEVICES_SUPPORT
    DevicesModel::self()->unmountRemote();
    #endif
    #ifdef PHONON_FOUND
    Settings::self()->savePlayStream(streamPlayAction->isChecked());
    #endif
    if (!tabWidget->isEnabled(PAGE_PLAYQUEUE)) {
        Settings::self()->saveSplitterState(splitter->saveState());
    }
    Settings::self()->saveShowPlaylist(expandInterfaceAction->isChecked());
    Settings::self()->saveSplitterAutoHide(autoHideSplitterAction->isChecked());
    Settings::self()->saveSidebar((int)(tabWidget->mode()));
    Settings::self()->savePage(tabWidget->currentWidget()->metaObject()->className());
    Settings::self()->saveSmallPlaybackButtons(smallPlaybackButtonsAction->isChecked());
    Settings::self()->saveSmallControlButtons(smallControlButtonsAction->isChecked());
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
    #endif // CANTATA_ANDROID
    streamsPage->save();
    lyricsPage->saveSettings();
    #ifdef ENABLE_WEBKIT
    infoPage->save();
    #endif
    Settings::self()->save(true);
    disconnect(MPDConnection::self(), 0, 0, 0);
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    if (Settings::self()->stopDynamizerOnExit() || Settings::self()->stopOnExit()) {
        Dynamic::self()->stop();
    }
    #endif
    if (Settings::self()->stopOnExit() || (fadeWhenStop() && StopState_Stopping==stopState)) {
        emit stop();
        Utils::sleep();
    }
    Utils::stopThread(mpdThread);
    Covers::self()->stop();
}

void MainWindow::initSizes()
{
    ItemView::setup();
    FancyTabWidget::setup();
    GroupedView::setup();
    ActionItemDelegate::setup();
    MusicLibraryItemAlbum::setup();

    // Calculate size for cover widget...
    int spacing=style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
    if (spacing<0) {
        spacing=4;
    }
    int cwSize=qMax(playPauseTrackButton->height(), trackLabel->height()+artistLabel->height()+spacing)+
               songTimeElapsedLabel->height()+positionSlider->height()+(spacing*2);

    cwSize=(((int)(cwSize/4))*4)+(cwSize%4 ? 4 : 0);
    if (cwSize<72) {
        cwSize=72;
    } else if (cwSize>200) {
        cwSize=200;
    }
    coverWidget->setMinimumSize(cwSize, cwSize);
    coverWidget->setMaximumSize(cwSize, cwSize);
}

#ifdef TAGLIB_FOUND
void MainWindow::load(const QList<QUrl> &urls)
{
    QStringList useable;
    bool haveHttp=HttpServer::self()->isAlive();
    bool alwaysUseHttp=haveHttp && Settings::self()->alwaysUseHttp();
    bool mpdLocal=MPDConnection::self()->getDetails().isLocal();
    bool allowLocal=haveHttp || mpdLocal;

    foreach (QUrl u, urls) {
        if (QLatin1String("http")==u.scheme()) {
            useable.append(u.toString());
        } else if (allowLocal && (u.scheme().isEmpty() || QLatin1String("file")==u.scheme())) {
            if (alwaysUseHttp || !mpdLocal) {
                useable.append(HttpServer::self()->encodeUrl(u.path()));
            } else {
                useable.append(u.toString());
            }
        }
    }
    if (useable.count()) {
        playQueueModel.addItems(useable, playQueueModel.rowCount(), false, 0);
    }
}
#endif

void MainWindow::playbackButtonsMenu()
{
    playbackBtnsMenu->exec(QCursor::pos());
}

void MainWindow::controlButtonsMenu()
{
    controlBtnsMenu->exec(QCursor::pos());
}

void MainWindow::setPlaybackButtonsSize(bool smallButtons)
{
    #ifdef CANTATA_ANDROID
    int size=Icon::stdSize(QApplication::fontMetrics().height()*2);
    #endif

    QList<QToolButton *> playbackBtns;
    playbackBtns << prevTrackButton << stopTrackButton << playPauseTrackButton << nextTrackButton;
    foreach (QToolButton *b, playbackBtns) {
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        #ifdef CANTATA_ANDROID
        if (size>22) {
            b->setMinimumSize(QSize(size+2, size+2));
            b->setMaximumSize(QSize(size+2, size+2));
            b->setIconSize(QSize(size, size));
        } else
        #endif
        {
            b->setIconSize(smallButtons ? QSize(22, 22) : QSize(28, 28));
            b->setMinimumSize(smallButtons ? QSize(26, 26) : QSize(32, 32));
            b->setMaximumSize(smallButtons ? QSize(26, 26) : QSize(32, 32));
        }
    }
}

void MainWindow::setControlButtonsSize(bool smallButtons)
{
    #ifdef CANTATA_ANDROID
    int size=Icon::stdSize(QApplication::fontMetrics().height()*2);
    #endif
    QList<QToolButton *> controlBtns;
    controlBtns << volumeButton << menuButton;
    #ifdef PHONON_FOUND
    controlBtns << streamButton;
    #endif
    foreach (QToolButton *b, controlBtns) {
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        #ifdef CANTATA_ANDROID
        if (size>22) {
            b->setMinimumSize(QSize(size+2, size+2));
            b->setMaximumSize(QSize(size+2, size+2));
            b->setIconSize(QSize(size, size));
        } else
        #endif
        {
            b->setMinimumSize(smallButtons ? QSize(22, 22) : QSize(26, 26));
            b->setMaximumSize(smallButtons ? QSize(22, 22) : QSize(26, 26));
            #ifdef ENABLE_KDE_SUPPORT
            b->setIconSize(smallButtons ? QSize(16, 16) : QSize(22, 22));
            #else
            b->setIconSize(smallButtons ? QSize(18, 18) : QSize(22, 22));
            #endif
        }
    }
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
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    if (QLatin1String("NO_SONGS")==message) {
        messageWidget->setError(i18n("Failed to locate any songs matching the dynamic playlist rules."));
    } else
    #endif
    {
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
    #ifdef CANTATA_ANDROID
    Q_UNUSED(v)
    #else
    if (v && !splitter->isVisible()) {
        expandInterfaceAction->trigger();
    }
    #endif
}

void MainWindow::mpdConnectionStateChanged(bool connected)
{
    if (connected) {
        messageWidget->hide();
        if (CS_Connected!=connectedState) {
            emit getStatus();
            emit playListInfo();
            emit outputs();
            if (CS_Init!=connectedState) {
                loaded=0;
                currentTabChanged(tabWidget->current_index());
            }
            connectedState=CS_Connected;
            addWithPriorityAction->setVisible(MPDConnection::self()->canUsePriority());
            setPriorityAction->setVisible(addWithPriorityAction->isVisible());
        }
    } else {
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        lyricsPage->text->clear();
        serverInfoPage->clear();
        connectedState=CS_Disconnected;
        outputsAction->setVisible(false);
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

#ifdef CANTATA_ANDROID
void MainWindow::resizeEvent(QResizeEvent *event)
{
    setLayout(event->size().width()>event->size().height());
    if (messageWidget->isVisible()) {
        messageWidget->adjustSize();
    }
}

void MainWindow::setLayout(bool landscape)
{
    if (landscape) {
        if (tabWidget->isEnabled(PAGE_PLAYQUEUE)) {
            coverWidget->setVisible(true);
            tabWidget->ToggleTab(PAGE_PLAYQUEUE, false);
            tabWidget->SetMode(FancyTabWidget::Mode_IconOnlyLargeSidebar);
            if (PAGE_PLAYQUEUE==tabWidget->current_index()) {
                showTab(PAGE_LIBRARY);
            }
            tabWidget->adjustSize();
            playQueueWidget->adjustSize();
        }
    } else if (!tabWidget->isEnabled(PAGE_PLAYQUEUE)) {
        coverWidget->setVisible(false);
        tabWidget->SetMode(FancyTabWidget::Mode_IconOnlyBottomBar);
        tabWidget->ToggleTab(PAGE_PLAYQUEUE, true);
        showTab(PAGE_PLAYQUEUE);
        tabWidget->adjustSize();
    }
}

#else
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (trayItem) {
        lastPos=pos();
        hide();
        if (event->spontaneous()) {
            event->ignore();
        }
    } else {
        quitAction->trigger();
        // Ignore this event, as we have called the quit action...
        event->ignore();
    }
}
#endif

void MainWindow::showVolumeControl()
{
//     qWarning() << volumeControl->pos().x() << volumeControl->pos().y()
//     << volumeButton->pos().x() << volumeButton->pos().y()
//     << volumeButton->mapToGlobal(volumeButton->pos()).x() << volumeButton->mapToGlobal(volumeButton->pos()).y();
//     volumeControl->move();
    volumeControl->popup(volumeButton->mapToGlobal(QPoint((volumeButton->width()-volumeControl->width())/2, volumeButton->height())));
}

void MainWindow::playQueueItemsSelected(bool s)
{
    if (playQueue->model()->rowCount()) {
        playQueue->setContextMenuPolicy(Qt::ActionsContextMenu);
        removeFromPlayQueueAction->setEnabled(s);
        copyTrackInfoAction->setEnabled(s);
        clearPlayQueueAction->setEnabled(true);
        cropPlayQueueAction->setEnabled(playQueue->haveUnSelectedItems());
        shufflePlayQueueAction->setEnabled(true);
    }
}

void MainWindow::connectToMpd(const MPDConnectionDetails &details)
{
    messageWidget->hide();

    if (!MPDConnection::self()->isConnected() || details!=MPDConnection::self()->getDetails()) {
        libraryPage->clear();
        albumsPage->clear();
        folderPage->clear();
        playlistsPage->clear();
        playQueueModel.clear();
        lyricsPage->text->clear();
        serverInfoPage->clear();
        #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
        Dynamic::self()->stop();
        #endif
        showInformation(i18n("Connecting to %1").arg(details.description()));
        outputsAction->setVisible(false);
        connectedState=CS_Disconnected;
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
    emit getStats();
}

#define DIALOG_ERROR MessageBox::error(this, i18n("Action is not currently possible, due to other open dialogs.")); return

void MainWindow::showPreferencesDialog()
{
    static bool showing=false;

    if (!showing) {
        #ifdef TAGLIB_FOUND
        if (0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
            DIALOG_ERROR;
        }
        #endif
        #ifdef ENABLE_DEVICES_SUPPORT
        if (0!=ActionDialog::instanceCount()  || 0!=SyncDialog::instanceCount()) {
            DIALOG_ERROR;
        }
        #endif
        #ifdef ENABLE_REPLAYGAIN_SUPPORT
        if (0!=RgDialog::instanceCount()) {
            DIALOG_ERROR;
        }
        #endif

        showing=true;
        PreferencesDialog pref(this, lyricsPage);
        connect(&pref, SIGNAL(settingsSaved()), this, SLOT(updateSettings()));
        connect(&pref, SIGNAL(connectTo(const MPDConnectionDetails &)), this, SLOT(connectToMpd(const MPDConnectionDetails &)));

        pref.exec();
        updateConnectionsMenu();
        showing=false;
    }
}

void MainWindow::checkMpdDir()
{
    #ifdef TAGLIB_FOUND
    editPlayQueueTagsAction->setEnabled(MPDConnection::self()->getDetails().dirReadable);
    organiseFilesAction->setEnabled(editPlayQueueTagsAction->isEnabled());
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    copyToDeviceAction->setEnabled(editPlayQueueTagsAction->isEnabled());
    deleteSongsAction->setEnabled(editPlayQueueTagsAction->isEnabled());
//     burnAction->setEnabled(copyToDeviceAction->isEnabled());
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    replaygainAction->setEnabled(editPlayQueueTagsAction->isEnabled());
    #endif
    switch (tabWidget->current_index()) {
    #if defined ENABLE_DEVICES_SUPPORT && defined TAGLIB_FOUND
    case PAGE_DEVICES:   devicesPage->controlActions();    break;
    #endif
    case PAGE_LIBRARY:   libraryPage->controlActions();    break;
    case PAGE_ALBUMS:    albumsPage->controlActions();     break;
    case PAGE_FOLDERS:   folderPage->controlActions();     break;
    case PAGE_PLAYLISTS: playlistsPage->controlActions();  break;
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    case PAGE_DYNAMIC:   dynamicPage->controlActions();    break;
    #endif
    case PAGE_STREAMS:   streamsPage->controlActions();    break;
    case PAGE_LYRICS:                                      break;
    #ifdef ENABLE_WEBKIT
    case PAGE_INFO:                                        break;
    #endif
    case PAGE_SERVER_INFO:                                 break;
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
                SET_SHORTCUT(act, Qt::MetaModifier+nextKey(i));
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
                QAction *act=menu->addAction(d.name.isEmpty() ? i18n("Default") : d.name, this, SLOT(changeConnection()));
                act->setData(d.name);
                act->setCheckable(true);
                act->setChecked(d.name==current);
                act->setActionGroup(connectionsGroup);
                SET_SHORTCUT(act, Qt::ControlModifier+nextKey(i));
            }
        }
    }
}

void MainWindow::readSettings()
{
    checkMpdDir();
    #ifdef TAGLIB_FOUND
    Covers::self()->setSaveInMpdDir(Settings::self()->storeCoversInMpdDir());
    if (Settings::self()->enableHttp()) {
        HttpServer::self()->setDetails(Settings::self()->httpAddress(), Settings::self()->httpPort());
    } else {
        HttpServer::self()->setDetails(QString(), 0);
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    deleteSongsAction->setVisible(Settings::self()->showDeleteAction());
    #endif
    lyricsPage->setEnabledProviders(Settings::self()->lyricProviders());
    MPDParseUtils::setGroupSingle(Settings::self()->groupSingle());
    MPDParseUtils::setGroupMultiple(Settings::self()->groupMultiple());
    albumsPage->setView(Settings::self()->albumsView());
    AlbumsModel::self()->setAlbumSort(Settings::self()->albumSort());

    #ifdef PHONON_FOUND
    streamButton->setVisible(!Settings::self()->streamUrl().isEmpty());
    streamPlayAction->setChecked(streamButton->isVisible() && Settings::self()->playStream());
    toggleStream(streamPlayAction->isChecked());
    #endif
    libraryPage->setView(Settings::self()->libraryView());
    MusicLibraryModel::self()->setUseArtistImages(Settings::self()->libraryArtistImage());
    playlistsPage->setView(Settings::self()->playlistsView());
    streamsPage->setView(0==Settings::self()->streamsView());
    folderPage->setView(0==Settings::self()->folderView());
    #ifdef ENABLE_DEVICES_SUPPORT
    devicesPage->setView(0==Settings::self()->devicesView());
    #endif
    #ifndef CANTATA_ANDROID
    setupTrayIcon();
    #if !defined Q_OS_WIN
    toggleDockManager();
    toggleMpris();
    #endif
    #endif
    autoScrollPlayQueue=Settings::self()->playQueueScroll();
    updateWindowTitle();
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
        refresh();
    } else if (diffLibCovers || diffLibYear || diffLibArtistImages || diffAlCovers) {
        libraryPage->clear();
        libraryPage->refresh();
    }

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

    if (Settings::self()->lyricsBgnd()!=lyricsPage->bgndImageEnabled()) {
        lyricsPage->setBgndImageEnabled(Settings::self()->lyricsBgnd());
        if (lyricsPage->bgndImageEnabled() && !coverWidget->isEmpty()) {
            Covers::Image img=Covers::self()->get(coverWidget->song());
            if (!img.img.isNull()) {
                lyricsPage->setImage(img.img);
            }
        }
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
    bool allowChange=true;
    #ifdef TAGLIB_FOUND
    if (0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        allowChange=false;
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=ActionDialog::instanceCount() || 0!=SyncDialog::instanceCount()) {
        allowChange=false;
    }
    #endif
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    if (0!=RgDialog::instanceCount()) {
        allowChange=false;
    }
    #endif

    if (allowChange) {
        QAction *act=qobject_cast<QAction *>(sender());

        if (act) {
            Settings::self()->saveCurrentConnection(act->data().toString());
            connectToMpd();
        }
    } else {
        QString current=Settings::self()->currentConnection();
        foreach (QAction *act, connectionsAction->menu()->actions()) {
            if (act->data().toString()==current) {
                act->setChecked(true);
            }
            break;
        }
    }
}

#ifndef ENABLE_KDE_SUPPORT
void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("About Cantata"),
                       tr("<b>Cantata %1</b><br/><br/>Simple GUI front-end for MPD.<br/><br/>(c) Craig Drummond 2011-2012.<br/>Released under the GPLv2<br/><br/><i><small>Based upon QtMPC - (C) 2007-2010 The QtMPC Authors</small></i>").arg(PACKAGE_VERSION));
}
#endif

#ifdef PHONON_FOUND
void MainWindow::toggleStream(bool s)
{
    MPDStatus * const status = MPDStatus::self();
    phononStreamEnabled = s;
    if (!s){
        if (phononStream) {
            phononStream->stop();
        }
    } else {
        if (phononStream) {
            switch (status->state()) {
            case MPDState_Playing:
                phononStream->play();
                break;
            case MPDState_Inactive:
            case MPDState_Stopped:
                phononStream->stop();
            break;
            case MPDState_Paused:
                phononStream->pause();
            default:
            break;
            }
        } else {
            phononStream=new Phonon::MediaObject(this);
            Phonon::createPath(phononStream, new Phonon::AudioOutput(Phonon::MusicCategory, this));
            phononStream->setCurrentSource(Settings::self()->streamUrl());
        }
    }
}
#endif

void MainWindow::stopTrack()
{
    if (!fadeWhenStop() || MPDState_Paused==MPDStatus::self()->state() || 0==volume) {
        emit stop();
    }
    stopTrackAction->setEnabled(false);
    nextTrackAction->setEnabled(false);
    prevTrackAction->setEnabled(false);
    startVolumeFade(/*true*/);
}

void MainWindow::startVolumeFade(/*bool stop*/)
{
    if (!fadeWhenStop()) {
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
        volume=origVolume;
        emit setVolume(origVolume);
        if (StopState_Stopping==stopState) {
            emit stop();
        } /*else if (StopState_Pausing==stopState) {
            emit pause(true);
        }*/
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
        /*if (fadeWhenStop()) {
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

void MainWindow::searchPlayQueue()
{
    if (searchPlayQueueLineEdit->text().isEmpty()) {
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
    QList<qint32> selectedSongIds;
    if (playQueueSearchTimer) {
        playQueueSearchTimer->stop();
    }
    QString filter=searchPlayQueueLineEdit->text().trimmed();
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

void MainWindow::updatePlayQueue(const QList<Song> &songs)
{
    TF_DEBUG
    playPauseTrackAction->setEnabled(!songs.isEmpty());
    nextTrackAction->setEnabled(stopTrackAction->isEnabled() && songs.count()>1);
    prevTrackAction->setEnabled(stopTrackAction->isEnabled() && songs.count()>1);

    playQueueModel.update(songs);
    playQueue->updateRows(usingProxy ? playQueueModel.rowCount()+10 : playQueueModel.currentSongRow(), false);

    /*if (1==songs.count() && MPDState_Playing==MPDStatus::self()->state()) {
        updateCurrentSong(songs.at(0));
    } else*/ if (0==songs.count()) {
        updateCurrentSong(Song());
    }
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
    QString connection=MPDConnection::self()->getDetails().name;

    if (multipleConnections && connection.isEmpty()) {
        connection=i18n("Default");
    }
    if (stopped) {
        setWindowTitle(multipleConnections ? i18n("Cantata (%1)").arg(connection) : "Cantata");
    } else if (current.artist.isEmpty()) {
        if (trackLabel->text().isEmpty()) {
            setWindowTitle(multipleConnections ? i18n("Cantata (%1)").arg(connection) : "Cantata");
        } else {
            #ifdef ENABLE_KDE_SUPPORT
            setWindowTitle(multipleConnections
                            ? i18nc("track :: Cantata (connection)", "%1 :: Cantata (%2)", trackLabel->text(), connection)
                            : i18nc("track :: Cantata", "%1 :: Cantata", trackLabel->text()));
            #else
            setWindowTitle(multipleConnections
                            ? tr("%1 :: Cantata (%2)").arg(trackLabel->text()).arg(connection)
                            : tr("%1 :: Cantata").arg(trackLabel->text()));
            #endif
        }
    } else {
        #ifdef ENABLE_KDE_SUPPORT
        setWindowTitle(multipleConnections
                        ? i18nc("track - artist :: Cantata (connection)", "%1 - %2 :: Cantata (%3)", trackLabel->text(), current.artist,connection)
                        : i18nc("track - artist :: Cantata", "%1 - %2 :: Cantata", trackLabel->text(), current.artist));
        #else
        setWindowTitle(multipleConnections
                        ? tr("%1 - %2 :: Cantata (%3)").arg(trackLabel->text()).arg(current.artist).arg(connection)
                        : tr("%1 - %2 :: Cantata").arg(trackLabel->text()).arg(current.artist));
        #endif
    }
}

void MainWindow::updateCurrentSong(const Song &song)
{
    if (fadeWhenStop() && StopState_None!=stopState) {
        if (StopState_Stopping==stopState) {
            emit stop();
        } /*else if (StopState_Pausing==stopState) {
            emit pause(true);
        }*/
    }

    current=song;

    #ifdef TAGLIB_FOUND
    if (current.isCantataStream()) {
        Song mod=HttpServer::self()->decodeUrl(current.file);
        if (!mod.title.isEmpty()) {
            current=mod;
            current.id=song.id;
//             current.file="XXX";
        }
    }
    #endif

    positionSlider->setEnabled(-1!=current.id && !currentIsStream());
    coverWidget->update(current);

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

    updateWindowTitle();
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

    #ifndef CANTATA_ANDROID
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
            if (coverWidget->isValid()) {
                coverPixmap = const_cast<QPixmap*>(coverWidget->pixmap());
            }

            if (Settings::self()->showPopups()) {
                if (notification) {
                    notification->close();
                }
                notification = new KNotification("CurrentTrackChanged", this);
                connect(notification, SIGNAL(closed()), this, SLOT(notificationClosed()));
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
    #endif
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
    if (!MusicLibraryModel::self()->lastUpdate().isValid() || MPDStats::self()->dbUpdate() > MusicLibraryModel::self()->lastUpdate()) {
        loaded|=TAB_LIBRARY|TAB_FOLDERS;
        if (!MusicLibraryModel::self()->lastUpdate().isValid()) {
            libraryPage->clear();
            //albumsPage->clear();
            folderPage->clear();
            playlistsPage->clear();
        }
        libraryPage->refresh();
        folderPage->refresh();
        playlistsPage->refresh();
    }
}

void MainWindow::updateStatus()
{
    MPDStatus * const status = MPDStatus::self();

    if (MPDState_Stopped==status->state() || MPDState_Inactive==status->state()) {
        positionSlider->setValue(0);
    } else {
        positionSlider->setRange(0, status->timeTotal());
        positionSlider->setValue(status->timeElapsed());
    }

    if (!stopState) {
        volume=status->volume();

        volumeControl->blockSignals(true);
        if (volume<0) {
            volumeButton->setEnabled(false);
            volumeButton->setToolTip(i18n("Volume Disabled"));
            volumeControl->setToolTip(i18n("Volume Disabled"));
            volumeControl->setValue(0);
        } else {
            volumeButton->setEnabled(true);
            volumeButton->setToolTip(i18n("Volume %1%").arg(volume));
            volumeControl->setToolTip(i18n("Volume %1%").arg(volume));
            volumeControl->setValue(volume);
        }
        volumeControl->blockSignals(false);

        if (volume<=0) {
            volumeButton->setIcon(Icon("audio-volume-muted"));
        } else if (volume<=33) {
            volumeButton->setIcon(Icon("audio-volume-low"));
        } else if (volume<=67) {
            volumeButton->setIcon(Icon("audio-volume-medium"));
        } else {
            volumeButton->setIcon(Icon("audio-volume-high"));
        }
    }

    randomPlayQueueAction->setChecked(status->random());
    repeatPlayQueueAction->setChecked(status->repeat());
    singlePlayQueueAction->setChecked(status->single());
    consumePlayQueueAction->setChecked(status->consume());

    QString timeElapsedFormattedString;
    if (status->timeElapsed()<172800 && (!currentIsStream() || (status->timeTotal()>0 && status->timeElapsed()<=status->timeTotal()))) {
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
        #ifdef PHONON_FOUND
        if (phononStreamEnabled && phononStream) {
            phononStream->play();
        }
        #endif
        playPauseTrackAction->setIcon(playbackPause);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        //playPauseTrackButton->setChecked(false);
        if (StopState_Stopping!=stopState) {
            stopTrackAction->setEnabled(true);
            nextTrackAction->setEnabled(true);
            prevTrackAction->setEnabled(true);
        }
        positionSlider->startTimer();

        #ifndef CANTATA_ANDROID
        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("media-playback-start");
            #else
            trayItem->setIcon(playbackPlay);
            #endif
        }
        #endif
        break;
    case MPDState_Inactive:
    case MPDState_Stopped:
        #ifdef PHONON_FOUND
        if (phononStreamEnabled && phononStream) {
            phononStream->stop();
        }
        #endif
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        stopTrackAction->setEnabled(false);
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

        #ifndef CANTATA_ANDROID
        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("cantata");
            trayItem->setToolTip("cantata", i18n("Cantata"), "<i>Playback stopped</i>");
            #else
            trayItem->setIcon(appIcon);
            #endif
        }
        #endif
        positionSlider->stopTimer();
        break;
    case MPDState_Paused:
        #ifdef PHONON_FOUND
        if (phononStreamEnabled && phononStream) {
            phononStream->pause();
        }
        #endif
        playPauseTrackAction->setIcon(playbackPlay);
        playPauseTrackAction->setEnabled(0!=playQueueModel.rowCount());
        stopTrackAction->setEnabled(0!=playQueueModel.rowCount());
        nextTrackAction->setEnabled(playQueueModel.rowCount()>1);
        prevTrackAction->setEnabled(playQueueModel.rowCount()>1);
        #ifndef CANTATA_ANDROID
        if (trayItem) {
            #ifdef ENABLE_KDE_SUPPORT
            trayItem->setIconByName("media-playback-pause");
            #else
            trayItem->setIcon(playbackPause);
            #endif
        }
        #endif
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

void MainWindow::playQueueItemActivated(const QModelIndex &index)
{
    emit startPlayingSongId(playQueueModel.getIdByRow(usingProxy ? playQueueProxyModel.mapToSource(index).row() : index.row()));
}

void MainWindow::removeFromPlayQueue()
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

void MainWindow::replacePlayQueue()
{
//     emit clear();
//     emit getStatus();
    addToPlayQueue(true);
}

void MainWindow::addToPlayQueue()
{
    addToPlayQueue(false);
}

void MainWindow::addToPlayQueue(bool replace, quint8 priority)
{
    searchPlayQueueLineEdit->clear();
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
}

void MainWindow::addWithPriority()
{
    if (!MPDConnection::self()->canUsePriority() || !addWithPriorityAction->isVisible()) {
        return;
    }

    QAction *act=qobject_cast<QAction *>(sender());

    if (!act) {
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
            QList<quint32> ids;
            foreach (const QModelIndex &idx, pqItems) {
                ids.append(playQueueModel.getIdByRow(usingProxy ? playQueueProxyModel.mapToSource(idx).row() : idx.row()));
            }
            emit setPriority(ids, prio);
        } else {
            addToPlayQueue(false, prio);
        }
    }
}

void MainWindow::addToNewStoredPlaylist()
{
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

void MainWindow::updatePlayQueueStats(int songs, quint32 time)
{
    if (0==time) {
        playQueueStatsLabel->setText(QString());
        return;
    }

    QString status;
    #ifdef ENABLE_KDE_SUPPORT
    status+=i18np("1 Track", "%1 Tracks", songs);
    #else
    status += QString::number(songs)+QString(1==songs ? tr(" Track") : tr(" Tracks"));
    #endif
    status += " (";
    status += MPDParseUtils::formatDuration(time);
    status += ")";
    playQueueStatsLabel->setText(status);
}

void MainWindow::updatePosition()
{
    if (positionSlider->value()<172800) {
        QString timeElapsedFormattedString;
        if (positionSlider->value() != positionSlider->maximum()) {
            timeElapsedFormattedString += Song::formattedTime(positionSlider->value());
            timeElapsedFormattedString += " / ";
            timeElapsedFormattedString += Song::formattedTime(songTime);
            songTimeElapsedLabel->setText(timeElapsedFormattedString);
        }
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

#ifndef CANTATA_ANDROID
void MainWindow::togglePlayQueue()
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
        if (spacing<0) {
            spacing=4;
        }
        // For some reason height is always larger than it needs to be - so fix this to cover height +4
        compactHeight=qMax(qMax(playPauseTrackButton->height(), trackLabel->height()+artistLabel->height()+spacing)+
                           songTimeElapsedLabel->height()+positionSlider->height()+(spacing*2),
                           coverWidget->height()+spacing);
    } else {
        collapsedSize=size();
        setMinimumHeight(calcMinHeight());
        setMaximumHeight(INT_MAX);
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
#endif

void MainWindow::sidebarModeChanged()
{
    #ifndef CANTATA_ANDROID
    if (splitter->isVisible()) {
        setMinimumHeight(calcMinHeight());
    }
    #endif
}

/*
 * Crop playqueue
 * Do this by taking the set off all song id's and subtracting from that
 * the set of selected song id's. Feed that list to emit removeSongs
 */
void MainWindow::cropPlayQueue()
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

#ifndef CANTATA_ANDROID
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
    trayItem->setIcon(appIcon);
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

void MainWindow::notificationClosed()
{
    if (sender() == notification) {
        notification=0;
    }
}
#else
void MainWindow::trayItemClicked(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
        if (isHidden()) {
            #ifdef Q_OS_WIN
            raiseWindow(this);
            #endif
            raise();
            showNormal();
            activateWindow();
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
#endif // CANTATA_ANDROID

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
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    case PAGE_DYNAMIC:
        dynamicPage->controlActions();
        break;
    #endif
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
    case PAGE_PLAYQUEUE:
        if (tabWidget->isEnabled(index)) {
            #ifndef CANTATA_ANDROID
            autoHideSplitterAction->setVisible(false);
            splitter->setAutohidable(0, autoHideSplitterAction->isChecked() && !tabWidget->isEnabled(PAGE_PLAYQUEUE));
            #endif
            playQueueWidget->setParent(playQueuePage);
            playQueuePage->layout()->addWidget(playQueueWidget);
            playQueueWidget->setVisible(true);

        } else {

            playQueuePage->layout()->removeWidget(playQueueWidget);
            playQueueWidget->setParent(splitter);
            playQueueWidget->setVisible(true);
            #ifndef CANTATA_ANDROID
            autoHideSplitterAction->setVisible(true);
            splitter->setAutohidable(0, autoHideSplitterAction->isChecked() && !tabWidget->isEnabled(PAGE_PLAYQUEUE));
            #endif
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

#ifndef CANTATA_ANDROID
void MainWindow::toggleSplitterAutoHide()
{
    bool ah=autoHideSplitterAction->isChecked() && !tabWidget->isEnabled(PAGE_PLAYQUEUE);
    splitter->setAutoHideEnabled(ah);
    splitter->setAutohidable(0, ah);
}
#endif

void MainWindow::locateTrack()
{
    if (!libraryPage->isVisible()) {
        showLibraryTab();
    }
    libraryPage->showSongs(playQueue->selectedSongs());
}

void MainWindow::showPage(const QString &page, bool focusSearch)
{
    QString p=page.toLower();
    if (QLatin1String("library")==p) {
        showTab(MainWindow::PAGE_LIBRARY);
    } else if (QLatin1String("albums")==p) {
        showTab(MainWindow::PAGE_ALBUMS);
    } else if (QLatin1String("folders")==p) {
        showTab(MainWindow::PAGE_FOLDERS);
    } else if (QLatin1String("playlists")==p) {
        showTab(MainWindow::PAGE_PLAYLISTS);
    }
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    else if (QLatin1String("dynamic")==p) {
        showTab(MainWindow::PAGE_DYNAMIC);
    }
    #endif
    else if (QLatin1String("streams")==p) {
        showTab(MainWindow::PAGE_STREAMS);
    } else if (QLatin1String("lyrics")==p) {
        showTab(MainWindow::PAGE_LYRICS);
    }
    #ifdef ENABLE_WEBKIT
    else if (QLatin1String("info")==p) {
        showTab(MainWindow::PAGE_INFO);
    }
    #endif
    else if (QLatin1String("serverinfo")==p) {
        showTab(MainWindow::PAGE_SERVER_INFO);
    }
    #if defined ENABLE_KDE_SUPPORT && defined TAGLIB_FOUND
    else if (QLatin1String("devices")==p) {
        showTab(MainWindow::PAGE_DEVICES);
    }
    #endif
    if (focusSearch) {
        focusTabSearch();
    }
}

#if !defined Q_OS_WIN && !defined CANTATA_ANDROID
void MainWindow::dynamicStatus(const QString &message)
{
    Dynamic::self()->helperMessage(message);
}
#endif

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
    default:                                              break;
    }
}

void MainWindow::focusSearch()
{
    if (searchPlayQueueLineEdit->hasFocus()) {
        return;
    }
    if (playQueue->hasFocus()) {
        searchPlayQueueLineEdit->setFocus();
    } else {
        focusTabSearch();
    }
}

void MainWindow::focusTabSearch()
{
    if (libraryPage->isVisible()) {
        libraryPage->focusSearch();
    } else if (albumsPage->isVisible()) {
        albumsPage->focusSearch();
    } else if (folderPage->isVisible()) {
        folderPage->focusSearch();
    } else if (playlistsPage->isVisible()) {
        playlistsPage->focusSearch();
    }
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    else if (dynamicPage->isVisible()) {
        dynamicPage->focusSearch();
    }
    #endif
    else if (streamsPage->isVisible()) {
        streamsPage->focusSearch();
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else if (devicesPage->isVisible()) {
        devicesPage->focusSearch();
    }
    #endif
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

#if !defined Q_OS_WIN && !defined CANTATA_ANDROID
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
        connect(coverWidget, SIGNAL(coverFile(const QString &)), dock, SLOT(setIcon(const QString &)));
    }

    bool wasEnabled=dock->enabled();
    dock->setEnabled(Settings::self()->dockManager());
    if (dock->enabled() && !wasEnabled && !coverWidget->fileName().isEmpty()) {
        dock->setIcon(coverWidget->fileName());
    }
}
#endif

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

void MainWindow::editTags(const QList<Song> &songs, bool isPlayQueue)
{
    if (songs.isEmpty()) {
        return;
    }

    if (0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=ActionDialog::instanceCount() || 0!=SyncDialog::instanceCount()) {
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

void MainWindow::organiseFiles()
{
    if (0!=TrackOrganiser::instanceCount()) {
        return;
    }

    if (0!=TagEditor::instanceCount()) {
        DIALOG_ERROR;
    }

    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=ActionDialog::instanceCount() || 0!=SyncDialog::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
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
}
#endif

#ifdef ENABLE_DEVICES_SUPPORT
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
    if (songs.isEmpty() || 0!=ActionDialog::instanceCount()) {
        return;
    }
    #ifdef TAGLIB_FOUND
    if (0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=SyncDialog::instanceCount()) {
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
    if (songs.isEmpty() || 0!=ActionDialog::instanceCount()) {
        return;
    }
    #ifdef TAGLIB_FOUND
    if (0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=SyncDialog::instanceCount()) {
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
    #ifdef TAGLIB_FOUND
    if (0!=TagEditor::instanceCount() || 0!=TrackOrganiser::instanceCount()) {
        DIALOG_ERROR;
    }
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    if (0!=ActionDialog::instanceCount() || 0!=SyncDialog::instanceCount()) {
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
}
#endif

#ifdef Q_OS_WIN
// This is down here, because windows.h includes ALL windows stuff - and we get conflics with MessageBox :-(
#include <windows.h>
static void raiseWindow(QWidget *w)
{
    ::SetWindowPos(w->effectiveWinId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::SetWindowPos(w->effectiveWinId(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}
#endif
