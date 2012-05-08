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
#include <KDE/KMessageBox>
#else
#include <QtGui/QApplication>
#include <QtGui/QIcon>
#include <QtCore/QFile>
#include <QtCore/QDir>
#endif
#include "utils.h"
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
        if (w) {
            if (!w->isVisible()) {
                w->showNormal();
            }
        } else {
            if (0==Utils::getAudioGroupId() && KMessageBox::Cancel==KMessageBox::warningContinueCancel(0,
                    i18n("You are not currently a member of the \"audio\" group. "
                         "Cantata will function better (saving of album covers, lyrics, etc. with the correct permissions) if you "
                         "(or your administrator) add yourself to this group.\n\n"
                         "Note, that if you do add yourself you will need to logout and back in for this to take effect.\n\n"
                         "Select \"Continue\" to start Cantata as is."),
                    i18n("Audio Group"), KStandardGuiItem::cont(), KStandardGuiItem::cancel(), "audioGroupWarning")) {
                QApplication::exit(0);
            }
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
#else
void setupIconTheme()
{
    // Check that we have certain icons in the selected icon theme. If not, and oxygen is installed, then
    // set icon theme to oxygen.
    QString theme=QIcon::themeName();
    if (QLatin1String("oxygen")!=theme) {
        QStringList check=QStringList() << "actions/edit-clear-list" << "actions/view-media-playlist"
                                        << "actions/view-media-lyrics" << "actions/configure"
                                        << "actions/view-choose" << "actions/view-media-artist"
                                        << "places/server-database" << "devices/media-optical-audio";

        foreach (const QString &icn, check) {
            if (!QIcon::hasThemeIcon(icn)) {
                QStringList paths=QIcon::themeSearchPaths();

                foreach (const QString &p, paths) {
                    if (QDir(p+QLatin1String("/oxygen")).exists()) {
                        QIcon::setThemeName(QLatin1String("oxygen"));
                        return;
                    }
                }
                return;
            }
        }
    }
}
#endif

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

    CantataApp app;
#else
    QApplication app(argc, argv);
    QApplication::setApplicationName(PACKAGE_NAME);
    QApplication::setOrganizationName(PACKAGE_NAME);

    setupIconTheme();
    MainWindow mw;
#endif

    return app.exec();
}
