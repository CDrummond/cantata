/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPD_STATS_H
#define MPD_STATS_H

#include <QtCore/QDateTime>

namespace MPDStats
{
    struct Values {
        Values()
            : artists(0)
            , albums(0)
            , songs(0)
            , uptime(0)
            , playtime(0)
            , dbPlaytime(0) {
        }
        quint32 artists;
        quint32 albums;
        quint32 songs;
        quint32 uptime;
        quint32 playtime;
        quint32 dbPlaytime;
        QDateTime dbUpdate;
    };

    extern void set(const Values &v);
    extern Values get();
    extern QDateTime getDbUpdate();
};

#endif
