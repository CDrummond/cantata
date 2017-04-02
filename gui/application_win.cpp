/*
 * Cantata
 *
 * Copyright (c) 2011-2016 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "application_win.h"
#include "settings.h"
#include "config.h"
#include <QIcon>
#include <windows.h>

Application::Application(int &argc, char **argv)
    : SingleApplication(argc, argv)
{
    installNativeEventFilter(this);
    QIcon::setThemeName(QLatin1String("cantata"));
    setAttribute(Qt::AA_DontShowIconsInMenus, true);
    if (Settings::self()->retinaSupport()) {
        setAttribute(Qt::AA_UseHighDpiPixmaps);
    }
}

bool Application::nativeEventFilter(const QByteArray &, void *message, long *result)
{
    MSG *msg = static_cast<MSG *>(message);
    if (msg && WM_POWERBROADCAST==msg->message && PBT_APMRESUMEAUTOMATIC==msg->wParam) {
        emit reconnect();
    }
    return false;
}

