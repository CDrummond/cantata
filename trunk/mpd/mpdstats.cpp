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

#include "mpdstats.h"
#include <QtCore/QReadWriteLock>

static QReadWriteLock lock;
static MPDStats::Values values;

void MPDStats::set(const Values &v)
{
    lock.lockForWrite();
    // Music model relies on correct dbUpdate, so make sure this is valid!
    QDateTime oldDbUpdate=values.dbUpdate;
    values=v;
    if (!values.dbUpdate.isValid() && oldDbUpdate.isValid()) {
        values.dbUpdate=oldDbUpdate;
    }
    lock.unlock();
}

MPDStats::Values MPDStats::get()
{
    Values v;
    lock.lockForRead();
    v=values;
    lock.unlock();
    return v;
}

QDateTime MPDStats::getDbUpdate()
{
    QDateTime v;
    lock.lockForRead();
    v=values.dbUpdate;
    lock.unlock();
    return v;
}
