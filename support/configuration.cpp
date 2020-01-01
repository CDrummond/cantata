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

#include "configuration.h"

QLatin1String Configuration::constMainGroup("General");

Configuration::Configuration(const QString &group)
{
    if (group!=constMainGroup) {
        beginGroup(group);
    }
}

Configuration::~Configuration()
{
}

int Configuration::get(const QString &key, int def, int min, int max)
{
    int v=get(key, def);
    return v<min ? min : (v>max ? max : v);
}

QString Configuration::getFilePath(const QString &key, const QString &def)
{
    #ifdef Q_OS_WIN
    return Utils::fixPath(QDir::fromNativeSeparators(get(key, def)), false);
    #else
    return Utils::tildaToHome(Utils::fixPath(get(key, def), false));
    #endif
}

QString Configuration::getDirPath(const QString &key, const QString &def)
{
    #ifdef Q_OS_WIN
    return Utils::fixPath(QDir::fromNativeSeparators(get(key, def)));
    #else
    return Utils::tildaToHome(Utils::fixPath(get(key, def)));
    #endif
}

