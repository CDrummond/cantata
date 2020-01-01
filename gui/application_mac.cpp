/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "application_mac.h"
#include "support/utils.h"
#include "config.h"
#include <stdlib.h>
#include <QDir>

Application::Application(int &argc, char **argv)
    : SingleApplication(argc, argv)
{
    setAttribute(Qt::AA_DontShowIconsInMenus, true);

    // Set DYLD_LIBRARY_PATH so that Qt finds our openSSL libs
    QDir dir(argv[0]);
    dir.cdUp();

    QByteArray ldPath = qgetenv("DYLD_LIBRARY_PATH");
    if (!ldPath.isEmpty()) {
        ldPath=':'+ldPath;
    }
    ldPath = dir.absolutePath().toLocal8Bit()+ldPath;
}
