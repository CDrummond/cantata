/*
 * Cantata Web
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "application.h"
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"
#include "support/thread.h"
#include "db/librarydb.h"
#include "httpserver.h"
#include "apihandler.h"
#include "config.h"
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QDateTime>
static QMutex msgMutex;
static bool firstMsg=true;
static void cantataQtMsgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    QMutexLocker locker(&msgMutex);
    QFile f(PACKAGE_NAME ".log");
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
    Dbg_Mpd               = 0x00000001,
    Dbg_MpdParse          = 0x00000002,
//    Dbg_Covers            = 0x00000004,
    Dbg_Library = 0x00000008,
    Dbg_Api               = 0x00000200,
    Dbg_HttpServer        = 0x00000400,
    Dbg_Threads           = 0x00004000,
//    Dbg_Tags              = 0x00008000,

    // NOTE: MUST UPDATE Dbg_All IF ADD NEW ITEMS!!!
    Dbg_All               = 0x0000460B
};

static void installDebugMessageHandler()
{
    QString debug=qgetenv("CANTATA_WEB_DEBUG");
    if (!debug.isEmpty()) {
        int dbg=0;
        if (debug.contains(QLatin1String("0x"))) {
            dbg=debug.startsWith(QLatin1Char('-')) ? (debug.mid(1).toInt(0, 16)*-1) : debug.toInt(0, 16);
        } else {
            dbg=debug.toInt();
        }
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
//        if (dbg&Dbg_Covers) {
//            Covers::enableDebug();
//        }
        if (dbg&Dbg_Library) {
            LibraryDb::enableDebug();
        }
        if (dbg&Dbg_Api) {
            ApiHandler::enableDebug();
        }
        if (dbg&Dbg_HttpServer) {
            HttpServer::enableDebug();
        }
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
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    QCoreApplication::setApplicationVersion(PACKAGE_VERSION_STRING);
    installDebugMessageHandler();
    Application app(argc, argv);
    return app.exec();
}
