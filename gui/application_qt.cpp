/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "config.h"
#ifdef QT_QTDBUS_FOUND
#include <QDBusConnection>
#include <QDBusMessage>
#endif
#include <QDir>
#include <QIcon>

static void setupIconTheme(Application *app)
{
    QIcon::setThemeSearchPaths(QStringList() << CANTATA_SYS_ICONS_DIR << QIcon::themeSearchPaths());
    QIcon::setThemeName(QLatin1String("cantata"));
    if (Utils::KDE!=Utils::currentDe()) {
        app->setAttribute(Qt::AA_DontShowIconsInMenus, true);
    }
}

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
}

bool Application::start()
{
#ifdef QT_QTDBUS_FOUND
    if (QDBusConnection::sessionBus().registerService(CANTATA_REV_URL)) {
        setupIconTheme(this);
        return true;
    }
    loadFiles();
    // ...and activate window!
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall("mpd.cantata", "/org/mpris/MediaPlayer2", "", "Raise"));
    return false;
#endif
}

void Application::loadFiles()
{
#ifdef QT_QTDBUS_FOUND
    QStringList args(arguments());
    if (args.count()>1) {
        args.takeAt(0);
        QDBusMessage m = QDBusMessage::createMethodCall("mpd.cantata", "/cantata", "", "load");
        QList<QVariant> a;
        a.append(args);
        m.setArguments(a);
        QDBusConnection::sessionBus().send(m);
    }
#endif
}

