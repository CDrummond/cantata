/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KUniqueApplication>
#include <KDE/KAboutData>
#include <KDE/KCmdLineArgs>
#include <KDE/KStartupInfo>
#endif
#include "utils.h"
#include "config.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    #ifdef ENABLE_KDE_SUPPORT
    KAboutData aboutData(PACKAGE_NAME, 0,
                         ki18n("Cantata"), PACKAGE_VERSION,
                         ki18n("A KDE client for MPD"),
                         KAboutData::License_GPL_V2,
                         ki18n("Copyright (C) 2011-2012 Craig Drummond"),
                         KLocalizedString(),
                         "http://cantata.googlecode.com", "craig.p.drummond@gmail.com");

    aboutData.addAuthor(ki18n("Craig Drummond"), ki18n("Maintainer"), "craig.p.drummond@gmail.com");
    aboutData.addAuthor(ki18n("Piotr Wicijowski"), ki18n("UI Improvements"), "piotr.wicijowski@gmail.com");
    aboutData.addAuthor(ki18n("Sander Knopper"), ki18n("QtMPC author"));
    aboutData.addAuthor(ki18n("Roeland Douma"), ki18n("QtMPC author"));
    aboutData.addAuthor(ki18n("Daniel Selinger"), ki18n("QtMPC author"));
    aboutData.addAuthor(ki18n("Armin Walland"), ki18n("QtMPC author"));
    aboutData.setOrganizationDomain("kde.org");
    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineOptions options;
    options.add("+[URL]", ki18n("URL to open"));
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();
    if (!KUniqueApplication::start())
        exit(0);

    Application app;
    #else
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    #ifdef Q_WS_WIN
    QCoreApplication::setOrganizationName("mpd");
    #else
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    #endif

    Application app(argc, argv);
    if (!app.start()) {
        return 0;
    }
    MainWindow mw;
    app.setActivationWindow(&mw);
    app.loadFiles();
    #endif

    return app.exec();
}
