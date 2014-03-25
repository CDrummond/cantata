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

#include "configuration.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobal>
#include <KDE/KConfig>
#endif

QLatin1String Configuration::constMainGroup("General");

Configuration::Configuration(const QString &group)
    #ifdef ENABLE_KDE_SUPPORT
    : topCfg(KGlobal::config(), group)
    #endif
{
    #ifdef ENABLE_KDE_SUPPORT
    cfg=&topCfg;
    #else
    if (group!=constMainGroup) {
        beginGroup(group);
    }
    #endif
}

Configuration::~Configuration()
{
    #ifdef ENABLE_KDE_SUPPORT
    endGroup();
    #endif
}

int Configuration::get(const QString &key, int def, int min, int max)
{
    int v=get(key, def);
    return v<min || v>max ? def : v;
}

#ifdef ENABLE_KDE_SUPPORT
void Configuration::beginGroup(const QString &group)
{
    if (group==constMainGroup) {
        cfg=&topCfg;
    } else {
        cfg=new KConfigGroup(KGlobal::config(), group);
    }
}

void Configuration::endGroup()
{
    if (cfg!=&topCfg) {
        cfg->sync();
        delete cfg;
    }
    cfg=&topCfg;
}
#endif

