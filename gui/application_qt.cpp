/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "application_qt.h"
#include "settings.h"
#include "support/gtkstyle.h"
#include "config.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QIcon>

static void setupIconTheme()
{
    QString cfgTheme=Settings::self()->iconTheme();
    if (cfgTheme.isEmpty() && GtkStyle::isActive()) {
        cfgTheme=GtkStyle::iconTheme();
    }
    if (!cfgTheme.isEmpty()) {
        QIcon::setThemeName(cfgTheme);
    }

    // BUG:130 Some non-DE environments (IceWM, etc) seem to set theme to HiColor, and this has missing
    // icons. So check for a themed icon, if the current theme does not have this - then see if oxygen
    // or gnome icon themes are installed, and set theme to one of those.
    if (!QIcon::hasThemeIcon("document-save-as")) {
        QStringList themes=QStringList() << QLatin1String("oxygen") << QLatin1String("gnome");
        QStringList prefixes=QStringList() << QLatin1String("/usr") << QLatin1String("/usr/local");
        if (!prefixes.contains(QLatin1String(INSTALL_PREFIX))) {
            prefixes+=QLatin1String(INSTALL_PREFIX);
        }
        foreach (const QString &theme, themes) {
            foreach (const QString &prefix, prefixes) {
                QString dir(prefix+QLatin1String("/share/icons/")+theme);
                if (QDir(dir).exists()) {
                    QIcon::setThemeName(theme);
                    // Add to theme search paths, if it is not there already...
                    if (!QIcon::themeSearchPaths().contains(prefix+QLatin1String("/share/icons"))) {
                        QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << QString(prefix+QLatin1String("/share/icons")));
                    }
                    return;
                }
            }
        }
    }
}

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    #if QT_VERSION >= 0x050400
    if (Settings::self()->retinaSupport()) {
        setAttribute(Qt::AA_UseHighDpiPixmaps);
    }
    #endif
}

bool Application::start()
{
    if (QDBusConnection::sessionBus().registerService(CANTATA_REV_URL)) {
        setupIconTheme();
        return true;
    }
    loadFiles();
    // ...and activate window!
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall("mpd.cantata", "/org/mpris/MediaPlayer2", "", "Raise"));
    return false;
}

void Application::loadFiles()
{
    QStringList args(arguments());
    if (args.count()>1) {
        args.takeAt(0);
        QDBusMessage m = QDBusMessage::createMethodCall("mpd.cantata", "/cantata", "", "load");
        QList<QVariant> a;
        a.append(args);
        m.setArguments(a);
        QDBusConnection::sessionBus().send(m);
    }
}

