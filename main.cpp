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

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KUniqueApplication>
#include <KDE/KAboutData>
#include <KDE/KCmdLineArgs>
#include <KDE/KStartupInfo>
#else
#include <QtGui/QApplication>
#endif
#include "config.h"
#include "mainwindow.h"

#ifdef ENABLE_KDE_SUPPORT
class CantataApp : public KUniqueApplication
{
public:
#ifdef Q_WS_X11
    CantataApp(Display *display, Qt::HANDLE visual, Qt::HANDLE colormap)
        : KUniqueApplication(display, visual, colormap)
        , w(0)
    {
    }
#endif

    CantataApp()
        : KUniqueApplication()
        , w(0)
    {
    }

    ~CantataApp() {
        if (w) {
            delete w;
            w=0;
        }
    }

    int newInstance() {
        if (!w) {
            w=new MainWindow();
        }

        KCmdLineArgs *args(KCmdLineArgs::parsedArgs());
        QList<QUrl> urls;
        for (int i = 0; i < args->count(); ++i) {
            urls.append(QUrl(args->url(i)));
        }
        if (!urls.isEmpty()) {
            w->load(urls);
        }
        KStartupInfo::appStarted(startupId());
        return 0;
    }

    MainWindow *w;
};
#endif

int main(int argc, char *argv[])
{
#ifdef ENABLE_KDE_SUPPORT
    KAboutData aboutData(PACKAGE_NAME, 0,
                         ki18n("Cantata"), PACKAGE_VERSION,
                         ki18n("A simple interface to MPD"),
                         KAboutData::License_GPL_V2,
                         ki18n("Copyright (C) 2011-2012 Craig Drummond"),
                         ki18n("Based upon QtMPC - (C) 2007-2010  The QtMPC Authors"),
                         QByteArray(), "craig.p.drummond@gmail.com");

    aboutData.addAuthor(ki18n("Craig Drummond"), ki18n("Maintainer"), "craig.p.drummond@gmail.com");
    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineOptions options;
    options.add("+[URL]", ki18n("URL to open"));
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();
    if (!KUniqueApplication::start())
        exit(0);

    CantataApp app;
#else
    QApplication app(argc, argv);
    QApplication::setApplicationName(PACKAGE_NAME);
    QApplication::setOrganizationName(PACKAGE_NAME);

#endif

    return app.exec();
}
