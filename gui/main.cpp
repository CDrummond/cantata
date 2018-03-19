/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QTranslator>
#include <QTextCodec>
#include <QLibraryInfo>
#include <QDir>
#include <QFile>
#include <QSettings>
#include "support/utils.h"
#include "config.h"
#include "settings.h"
#include "initialsettingswizard.h"
#include "mainwindow.h"
#include "mpd-interface/song.h"
#include "support/thread.h"
#include "db/librarydb.h"

// To enable debug...
#include "mpd-interface/mpdconnection.h"
#include "mpd-interface/mpdparseutils.h"
#include "covers.h"
#include "context/wikipediaengine.h"
#include "context/lastfmengine.h"
#include "context/metaengine.h"
#include "playlists/dynamicplaylists.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#endif
#include "streams/streamfetcher.h"
#include "http/httpserver.h"
#include "widgets/songdialog.h"
#include "network/networkaccessmanager.h"
#include "context/ultimatelyricsprovider.h"
#include "tags/taghelperiface.h"
#include "context/contextwidget.h"
#include "scrobbling/scrobbler.h"
#include "gui/mediakeys.h"
#ifdef ENABLE_HTTP_STREAM_PLAYBACK
#include "mpd-interface/httpstream.h"
#endif
#ifdef AVAHI_FOUND
#include "avahidiscovery.h"
#endif
#include "customactions.h"

#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QDateTime>
#include <QByteArray>
#include <QCommandLineParser>
#include <iostream>

static QMutex msgMutex;
static bool firstMsg=true;
static bool debugToFile=false;
static void cantataQtMsgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    if (debugToFile) {
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
    } else {
        std::cout << QDateTime::currentDateTime().toString(Qt::ISODate).replace("T", " ").toLatin1().constData()
                  << " - " << msg.toLocal8Bit().constData() << std::endl;
    }
}

static void loadTranslation(const QString &prefix, const QString &path, const QString &overrideLanguage = QString())
{
    QString language = overrideLanguage.isEmpty() ? QLocale::system().name() : overrideLanguage;
    QTranslator *t = new QTranslator;
    if (t->load(prefix+"_"+language, path)) {
        QCoreApplication::installTranslator(t);
    } else {
        delete t;
    }
}

static void removeOldFiles(const QString &d, const QStringList &types)
{
    if (!d.isEmpty()) {
        QDir dir(d);
        if (dir.exists()) {
            QFileInfoList files=dir.entryInfoList(types, QDir::Files|QDir::NoDotAndDotDot);
            for (const QFileInfo &file: files) {
                QFile::remove(file.absoluteFilePath());
            }
            QString dirName=dir.dirName();
            if (!dirName.isEmpty()) {
                dir.cdUp();
                dir.rmdir(dirName);
            }
        }
    }
}

static void removeOldFiles()
{
    // Remove Cantata 1.x XML cache files
    removeOldFiles(Utils::cacheDir("library"), QStringList() << "*.xml" << "*.xml.gz");
    removeOldFiles(Utils::cacheDir("jamendo"), QStringList() << "*.xml.gz");
    removeOldFiles(Utils::cacheDir("magnatune"), QStringList() << "*.xml.gz");
}

static QString debugAreas()
{
    return  QObject::tr("mpd - MPD communication")+QLatin1Char('\n')
            +QObject::tr("mpdparse - Parsing of MPD response")+QLatin1Char('\n')
            +QObject::tr("covers - Cover fetching, and loading")+QLatin1Char('\n')
            +QObject::tr("covers-verbose - Cover fetching, and loading (verbose) ")+QLatin1Char('\n')
            +QObject::tr("context-wikipedia - Wikipedia info in context view")+QLatin1Char('\n')
            +QObject::tr("context-lastfm - Last.fm info in context view")+QLatin1Char('\n')
            +QObject::tr("context-widget - General debug in context view")+QLatin1Char('\n')
            +QObject::tr("context-lyrics - Lyrics in context view")+QLatin1Char('\n')
            +QObject::tr("dynamic - Dynamic playlists")+QLatin1Char('\n')
            +QObject::tr("stream-fetcher - Fetching of stream URLs")+QLatin1Char('\n')
            +QObject::tr("http-server - Built-in HTTP server")+QLatin1Char('\n')
            +QObject::tr("song-dialog - Song dialogs (tags, replaygain, organiser)")+QLatin1Char('\n')
            +QObject::tr("network-access - Network access")+QLatin1Char('\n')
            +QObject::tr("threads - Threads")+QLatin1Char('\n')
            +QObject::tr("scrobbler - Scrobbling")+QLatin1Char('\n')
            +QObject::tr("sql - SQL access")+QLatin1Char('\n')
            +QObject::tr("media-keys - Media-keys")+QLatin1Char('\n')
            +QObject::tr("custom-actions - Custom actions")+QLatin1Char('\n')
            #ifdef TAGLIB_FOUND
            +QObject::tr("tags - Tag reading/writing")+QLatin1Char('\n')
            #endif
            #ifdef ENABLE_DEVICES_SUPPORT
            +QObject::tr("devices - Device support")+QLatin1Char('\n')
            #endif
            #ifdef ENABLE_HTTP_STREAM_PLAYBACK
            +QObject::tr("http-stream - Playback of MPD output streams")+QLatin1Char('\n')
            #endif
            #ifdef AVAHI_FOUND
            +QObject::tr("avahi - Auto-discovery of MPD servers")+QLatin1Char('\n')
            #endif
            ;
}

static void installDebugMessageHandler(const QString &cmdLine)
{
    QStringList items=cmdLine.split(",", QString::SkipEmptyParts);

    for (const auto &area: items) {
        if (QLatin1String("mpd")==area) {
            MPDConnection::enableDebug();
        } else if (QLatin1String("mpdparse")==area) {
            MPDParseUtils::enableDebug();
        } else if (QLatin1String("covers")==area) {
            Covers::enableDebug(false);
        } else if (QLatin1String("covers-verbose")==area) {
            Covers::enableDebug(true);
        } else if (QLatin1String("context-wikipedia")==area) {
            WikipediaEngine::enableDebug();
        } else if (QLatin1String("context-lastfm")==area) {
            LastFmEngine::enableDebug();
        } else if (QLatin1String("context-info")==area) {
            MetaEngine::enableDebug();
        } else if (QLatin1String("context-widget")==area) {
            ContextWidget::enableDebug();
        } else if (QLatin1String("dynamic")==area) {
            DynamicPlaylists::enableDebug();
        } else if (QLatin1String("stream-fetcher")==area) {
            StreamFetcher::enableDebug();
        } else if (QLatin1String("http-server")==area) {
            HttpServer::enableDebug();
        } else if (QLatin1String("song-dialog")==area) {
            SongDialog::enableDebug();
        } else if (QLatin1String("network-access")==area) {
            NetworkAccessManager::enableDebug();
        } else if (QLatin1String("context-lyrics")==area) {
            UltimateLyricsProvider::enableDebug();
        } else if (QLatin1String("threads")==area) {
            ThreadCleaner::enableDebug();
        } else if (QLatin1String("scrobbler")==area) {
            Scrobbler::enableDebug();
        } else if (QLatin1String("sql")==area) {
            LibraryDb::enableDebug();
        } else if (QLatin1String("media-keys")==area) {
            MediaKeys::enableDebug();
        } else if (QLatin1String("custom-actions")==area) {
            CustomActions::enableDebug();
        }
        #ifdef TAGLIB_FOUND
        else if (QLatin1String("tags")==area) {
            TagHelperIface::enableDebug();
        }
        #endif
        #ifdef ENABLE_DEVICES_SUPPORT
        else if (QLatin1String("devices")==area) {
            DevicesModel::enableDebug();
        }
        #endif
        #ifdef ENABLE_HTTP_STREAM_PLAYBACK
        else if (QLatin1String("http-stream")==area) {
            HttpStream::enableDebug();
        }
        #endif
        #ifdef AVAHI_FOUND
        else if (QLatin1String("avahi")==area) {
            AvahiDiscovery::enableDebug();
        }
        #endif
    }
    qInstallMessageHandler(cantataQtMsgHandler);
}

int main(int argc, char *argv[])
{
    QThread::currentThread()->setObjectName("GUI");
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    QCoreApplication::setOrganizationName(ORGANIZATION_NAME);

    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    #if QT_VERSION >= 0x050600 && defined Q_OS_WIN
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #endif

    Application app(argc, argv);
    app.setApplicationVersion(PACKAGE_VERSION_STRING);

    QCommandLineParser cmdLineParser;
    cmdLineParser.setApplicationDescription(QObject::tr("MPD Client"));
    cmdLineParser.addHelpOption();
    cmdLineParser.addVersionOption();
    QCommandLineOption debugOption(QStringList() << "d" << "debug", QObject::tr("Comma-separated list of debug areas - possible values:\n")+debugAreas(), "debug", "");
    QCommandLineOption debugToFileOption(QStringList() << "f" << "debug-to-file", QObject::tr("Log debug messages to %1").arg(Utils::cacheDir(QString(), true)+"cantata.log"), "", "false");
    QCommandLineOption noNetworkOption(QStringList() << "n" << "no-network", QObject::tr("Disable network access"), "", "false");
    cmdLineParser.addOption(debugOption);
    cmdLineParser.addOption(debugToFileOption);
    cmdLineParser.addOption(noNetworkOption);
    cmdLineParser.process(app);

    if (!app.start()) {
        return 0;
    }

    if (cmdLineParser.isSet(noNetworkOption)) {
        NetworkAccessManager::disableNetworkAccess();
    }

    // Set the permissions on the config file on Unix - it can contain passwords
    // for internet services so it's important that other users can't read it.
    // On Windows these are stored in the registry instead.
    #ifdef Q_OS_UNIX
    QSettings s;

    // Create the file if it doesn't exist already
    if (!QFile::exists(s.fileName())) {
        QFile file(s.fileName());
        file.open(QIODevice::WriteOnly);
    }

    // Set -rw-------
    QFile::setPermissions(s.fileName(), QFile::ReadOwner | QFile::WriteOwner);
    #endif

    removeOldFiles();
    if (cmdLineParser.isSet(debugOption)) {
        installDebugMessageHandler(cmdLineParser.value(debugOption));
        debugToFile=cmdLineParser.isSet(debugToFileOption);
    }

    // Translations
    QString lang=Settings::self()->lang();
    #if defined Q_OS_WIN || defined Q_OS_MAC
    loadTranslation("qt", CANTATA_SYS_TRANS_DIR, lang);
    #else
    loadTranslation("qt", QLibraryInfo::location(QLibraryInfo::TranslationsPath), lang);
    #endif
    loadTranslation("cantata", CANTATA_SYS_TRANS_DIR, lang);

    Application::init();

    if (Settings::self()->firstRun()) {
        InitialSettingsWizard wz;
        if (QDialog::Rejected==wz.exec()) {
            return 0;
        }
    }
    MainWindow mw;
    #if defined Q_OS_WIN || defined Q_OS_MAC
    app.setActivationWindow(&mw);
    #endif // !defined Q_OS_MAC
    app.loadFiles();

    return app.exec();
}
