/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "application_kde.h"
#include "initialsettingswizard.h"
#include "mainwindow.h"
#include "settings.h"
#include <KDE/KCmdLineArgs>
#include <KDE/KStartupInfo>

#ifdef Q_WS_X11
Application::Application(Display *display, Qt::HANDLE visual, Qt::HANDLE colormap)
    : KUniqueApplication(display, visual, colormap)
    , w(0)
{
    #if QT_VERSION >= 0x050400
    if (Settings::self()->retinaSupport()) {
        setAttribute(Qt::AA_UseHighDpiPixmaps);
    }
    #endif
}
#endif

Application::Application()
    : KUniqueApplication()
    , w(0)
{
    #if QT_VERSION >= 0x050400
    if (Settings::self()->retinaSupport()) {
        setAttribute(Qt::AA_UseHighDpiPixmaps);
    }
    #endif
}

Application::~Application() {
    if (w) {
        disconnect(w, SIGNAL(destroyed(QObject *)), this, SLOT(mwDestroyed(QObject *)));
        delete w;
        w=0;
    }
}

int Application::newInstance() {
    static bool in=false;
    if (in) {
        return 0;
    } else {
        in=true;
    }

    if (w) {
        w->restoreWindow();
    } else {
        initObjects();
        if (Settings::self()->firstRun()) {
            InitialSettingsWizard wz;
            if (QDialog::Rejected==wz.exec()) {
                QApplication::exit(0);
                return 0;
            }
        }
        w=new MainWindow();
        connect(w, SIGNAL(destroyed(QObject *)), this, SLOT(mwDestroyed(QObject *)));
        if (!Settings::self()->startHidden()) {
            w->show();
        }
    }

    KCmdLineArgs *args=KCmdLineArgs::parsedArgs();
    if (args && args->count()>0) {
        QStringList urls;
        for (int i = 0; i < args->count(); ++i) {
            urls.append(args->arg(i));
        }
        if (!urls.isEmpty()) {
            w->load(urls);
        }
    }
    KStartupInfo::appStarted(startupId());
    in=false;
    return 0;
}

void Application::mwDestroyed(QObject *obj)
{
    if (obj==w) {
        w=0;
    }
}

