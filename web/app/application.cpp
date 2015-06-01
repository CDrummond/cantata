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
#include "httpserver.h"
#include "statusapi.h"
#include "db/mpdlibrarydb.h"
#include "config.h"
#include <QCommandLineParser>
#include <QFile>
#include <QSettings>

Application::Application(int argc, char *argv[])
    : QCoreApplication(argc, argv)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("HTTP Interface for MPD/CantataWeb");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOption(QStringList() << "c" << "c", "Config file (default " PACKAGE_NAME ".conf).", "configfile", PACKAGE_NAME".conf");
    parser.addOption(configOption);
    parser.process(*this);
    QString cfgFile=parser.value(configOption);

    if (!QFile::exists(cfgFile)) {
        qWarning() << cfgFile << "does not exist";
        ::exit(-1);
    }

    QSettings *cfg=new QSettings(cfgFile, QSettings::IniFormat, this);

    MPDConnection::self()->start(); // Place in separate thread...
    connect(this, SIGNAL(mpdDetails(MPDConnectionDetails)), MPDConnection::self(), SLOT(setDetails(MPDConnectionDetails)));
    MPDConnectionDetails mpd;
    mpd.hostname=cfg->value("mpd/host", "127.0.0.1").toString();
    mpd.port=cfg->value("mpd/port", "6600").toUInt();
    mpd.password=cfg->value("mpd/password", QString()).toString();
    mpd.dir=cfg->value("mpd/dir", "/var/lib/mpd/music").toString();
    QString dbFile(cfg->value("db", QCoreApplication::applicationDirPath()+QDir::separator()+PACKAGE_NAME".sqlite").toString());
    if (!MpdLibraryDb::self()->init(dbFile)) {
        qWarning() << dbFile << "failed to initialize";
        ::exit(-1);
    }
    emit mpdDetails(mpd);
    HttpServer *http=new HttpServer(this, cfg);
    if (!http->start()) {
        qWarning() << "Failed to start HTTP server";
        ::exit(-1);
    }
    StatusApi::self();
}
