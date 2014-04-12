/*
 * Cantata
 *
 * Copyright (c) 2014 Niklas Wenzel <nikwen.developer@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuick/QQuickPaintedItem>
#include <QStandardPaths>
#include <QDir>

#include "mpdconnection.h"
#include "song.h"
#include "mpdparseutils.h"
#include "settings.h"
#include "thread.h"
#include "ubuntu/backend/mpdbackend.h"
#include "utils.h"
#include "musiclibrarymodel.h"
#include "albumsmodel.h"
#include "playlistsmodel.h"
#include "currentcover.h"
#include "config.h"

// To enable debug...
#include "covers.h"
//#include "wikipediaengine.h"
//#include "lastfmengine.h"
//#include "metaengine.h"
//#include "backdropcreator.h"
//#ifdef ENABLE_DYNAMIC
//#include "dynamic.h"
//#endif
//#include "streamfetcher.h"
//#include "httpserver.h"
//#include "songdialog.h"
#include "networkaccessmanager.h"
//#include "ultimatelyricsprovider.h"
//#ifdef ENABLE_EXTERNAL_TAGS
//#include "taghelperiface.h"
//#endif
//#include "contextwidget.h"

#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QDateTime>
static QMutex msgMutex;
static bool firstMsg=true;
static void cantataQtMsgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    QMutexLocker locker(&msgMutex);
    QFile f(Utils::cacheDir(QString(), true)+"cantata.log");
    if (f.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)) {
        QTextStream stream(&f);
        if (firstMsg) {
            stream << "------------START------------" << endl;
            firstMsg=false;
        }
        stream << QDateTime::currentDateTime().toString(Qt::ISODate).replace("T", " ") << " - " << msg << endl;
    }
}

enum Debug {
    Dbg_Mpd               = 0x0001,
    Dbg_MpdParse          = 0x0002,
    Dbg_Covers            = 0x0004,
    //Dbg_Context_Wikipedia = 0x0008,
    //Dbg_Context_LastFm    = 0x0010,
    //Dbg_Context_Meta      = 0x0020,
    //Dbg_Context_Widget    = 0x0040,
    //Dbg_Context_Backdrop  = 0x0080,
    //Dbg_Dynamic           = 0x0100,
    //Dbg_StreamFetching    = 0x0200,
    //Dbg_HttpServer        = 0x0400,
    //Dbg_SongDialogs       = 0x0800,
    Dbg_NetworkAccess     = 0x1000,
    //Dbg_Context_Lyrics    = 0x2000,
    Dbg_Threads           = 0x4000,
    //Dbg_Tags              = 0x8000,

    // NOTE: MUST UPDATE Dbg_All IF ADD NEW ITEMS!!!
    Dbg_All               = 0x5007
};

static void installDebugMessageHandler()
{
    QString debug=qgetenv("CANTATA_DEBUG");
    if (!debug.isEmpty()) {
        int dbg=debug.toInt();
        bool logToFile=dbg>0;
        if (dbg<0) {
            dbg*=-1;
        }
        if (dbg&Dbg_Mpd) {
            MPDConnection::enableDebug();
        }
        if (dbg&Dbg_MpdParse) {
            MPDParseUtils::enableDebug();
        }
        if (dbg&Dbg_Covers) {
            Covers::enableDebug();
        }
//        if (dbg&Dbg_Context_Wikipedia) {
//            WikipediaEngine::enableDebug();
//        }
//        if (dbg&Dbg_Context_LastFm) {
//            LastFmEngine::enableDebug();
//        }
//        if (dbg&Dbg_Context_Meta) {
//            MetaEngine::enableDebug();
//        }
//        if (dbg&Dbg_Context_Widget) {
//            ContextWidget::enableDebug();
//        }
//        if (dbg&Dbg_Context_Backdrop) {
//            BackdropCreator::enableDebug();
//        }
//        #ifdef ENABLE_DYNAMIC
//        if (dbg&Dbg_Dynamic) {
//            Dynamic::enableDebug();
//        }
//        #endif
//        if (dbg&Dbg_StreamFetching) {
//            StreamFetcher::enableDebug();
//        }
//        if (dbg&Dbg_HttpServer) {
//            HttpServer::enableDebug();
//        }
//        if (dbg&Dbg_SongDialogs) {
//            SongDialog::enableDebug();
//        }
        if (dbg&Dbg_NetworkAccess) {
            NetworkAccessManager::enableDebug();
        }
//        if (dbg&Dbg_Context_Lyrics) {
//            UltimateLyricsProvider::enableDebug();
//        }
        if (dbg&Dbg_Threads) {
            ThreadCleaner::enableDebug();
        }
//        #ifdef ENABLE_EXTERNAL_TAGS
//        if (dbg&Dbg_Tags) {
//            TagHelperIface::enableDebug();
//        }
//        #endif
        if (dbg&Dbg_All && logToFile) {
            #if QT_VERSION < 0x050000
            qInstallMsgHandler(cantataQtMsgHandler);
            #else
            qInstallMessageHandler(cantataQtMsgHandler);
            #endif
        }
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(CANTATA_REV_URL);
    QThread::currentThread()->setObjectName("GUI");

    Utils::initRand();
    Song::initTranslations();

    // Ensure these objects are created in the GUI thread...
    ThreadCleaner::self();
    MPDStatus::self();
    MPDStats::self();
    MPDConnection::self();

    MPDBackend backend;

    QGuiApplication app(argc, argv);
    installDebugMessageHandler();
    qmlRegisterType<MPDBackend>("MPDBackend", 1, 0, "MPDBackend");
    QQuickView view;
    view.setMinimumSize(QSize(360, 540));
    view.rootContext()->setContextProperty("backend", &backend);
    view.rootContext()->setContextProperty("artistsProxyModel", backend.getArtistsProxyModel());
    view.rootContext()->setContextProperty("albumsProxyModel", backend.getAlbumsProxyModel());
    view.rootContext()->setContextProperty("playlistsProxyModel", backend.getPlaylistsProxyModel());
    view.rootContext()->setContextProperty("playQueueProxyModel", backend.getPlayQueueProxyModel());
    view.rootContext()->setContextProperty("currentCover", CurrentCover::self());
    view.rootContext()->setContextProperty("appDir", Utils::dataDir(QString(), true));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:qml/cantata/main.qml"));
    view.show();

    AlbumsModel::self()->setEnabled(true);

    return app.exec();
}
