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

#ifdef ENABLE_KDE_SUPPORT
#include <KUniqueApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#else
#include <QApplication>
#endif
#include "config.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
#ifdef ENABLE_KDE_SUPPORT
    KAboutData aboutData(PACKAGE_NAME, 0,
                         ki18n("Cantata"), PACKAGE_VERSION,
                         ki18n("A simple interface to MPD"),
                         KAboutData::License_GPL_V2,
                         ki18n("Copyright (C) 2011 Craig Drummond"),
                         ki18n("Based upon QtMPC - (C) 2007-2010  The QtMPC Authors"),
                         QByteArray(), "craig.p.drummond@gmail.com");

    aboutData.addAuthor(ki18n("Craig Drummond"), ki18n("Maintainer"), "craig.p.drummond@gmail.com");
    KCmdLineArgs::init(argc, argv, &aboutData);

    KUniqueApplication app;
#else
    QApplication app(argc, argv);
    QApplication::setApplicationName(PACKAGE_NAME);
    QApplication::setOrganizationName(PACKAGE_NAME);
#endif

    MainWindow w;

    return app.exec();
}

