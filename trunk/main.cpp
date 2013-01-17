/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#else
#include <QtCore/QTranslator>
#include <QtCore/QTextCodec>
#include <QtCore/QLibraryInfo>
#include <QtCore/QDir>
#endif
#include "utils.h"
#include "config.h"
#include "settings.h"
#include "initialsettingswizard.h"
#include "mainwindow.h"

#ifndef ENABLE_KDE_SUPPORT
// Taken from Clementine!
//
// We convert from .po files to .qm files, which loses context information.
// This translator tries loading strings with an empty context if it can't
// find any others.

class PoTranslator : public QTranslator
{
public:
    QString translate(const char *context, const char *sourceText, const char *disambiguation = 0) const
    {
        QString ret = QTranslator::translate(context, sourceText, disambiguation);
        return !ret.isEmpty() ? ret : QTranslator::translate(NULL, sourceText, disambiguation);
    }
};

static void loadTranslation(const QString &prefix, const QString &path, const QString &overrideLanguage = QString())
{
    #if QT_VERSION < 0x040700
    // QTranslator::load will try to open and read "cantata" if it exists,
    // without checking if it's a file first.
    // This was fixed in Qt 4.7
    QFileInfo maybeCantataDirectory(path + "/cantata");
    if (maybeCantataDirectory.exists() && !maybeCantataDirectory.isFile()) {CK
        return;
    }
    #endif

    QString language = overrideLanguage.isEmpty() ? QLocale::system().name() : overrideLanguage;
    QTranslator *t = new PoTranslator;

    if (t->load(prefix+"_"+language, path)) {
        QCoreApplication::installTranslator(t);
    } else {
        delete t;
    }

    QTextCodec::setCodecForTr(QTextCodec::codecForLocale());
}
#endif

int main(int argc, char *argv[])
{
    #ifdef ENABLE_KDE_SUPPORT
    KAboutData aboutData(PACKAGE_NAME, 0,
                         ki18n("Cantata"), PACKAGE_VERSION_STRING,
                         ki18n("A KDE client for MPD"),
                         KAboutData::License_GPL_V2,
                         ki18n("Copyright (C) 2011-2013 Craig Drummond"),
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
    #ifdef TAGLIB_FOUND
    KCmdLineOptions options;
    options.add("+[URL]", ki18n("URL to open"));
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();
    #endif
    if (!KUniqueApplication::start())
        exit(0);

    Application app;
    #else // ENABLE_KDE_SUPPORT
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    #ifdef Q_OS_WIN
    QCoreApplication::setOrganizationName("mpd");
    #else
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    #endif

    Application app(argc, argv);
    if (!app.start()) {
        return 0;
    }

    // Translations
    QString langEnv=qgetenv("CANTATA_LANG");
    loadTranslation("qt", QLibraryInfo::location(QLibraryInfo::TranslationsPath), langEnv);
    #ifdef Q_OS_WIN
    loadTranslation("qt", app.applicationDirPath()+QLatin1String("/translations"), langEnv);
    loadTranslation("qt", QDir::currentPath()+QLatin1String("/translations"), langEnv);
    #endif
    loadTranslation("cantata", app.applicationDirPath()+QLatin1String("/translations"), langEnv);
    loadTranslation("cantata", QDir::currentPath()+QLatin1String("/translations"), langEnv);
    #ifndef Q_OS_WIN
    loadTranslation("cantata", INSTALL_PREFIX"/share/cantata/translations/", langEnv);
    #endif

    if (Settings::self()->firstRun()) {
        InitialSettingsWizard wz;
        if (QDialog::Rejected==wz.exec()) {
            return 0;
        } else {
            Settings::self()->saveConnectionDetails(wz.getDetails());
            Settings::self()->save(true);
        }
    }
    MainWindow mw;
    app.setActivationWindow(&mw);
    #ifdef TAGLIB_FOUND
    app.loadFiles();
    #endif // TAGLIB_FOUND
    #endif // ENABLE_KDE_SUPPORT

    return app.exec();
}
