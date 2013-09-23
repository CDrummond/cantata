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

#ifndef MPD_USER_H
#define MPD_USER_H

#include <QString>
#include "mpdconnection.h"

class MPDUser
{
public:
    static MPDUser * self();

    static const QString constName;
    static QString translatedName();

    MPDUser();

    bool isSupported();
    bool isRunning();
    void start();
    void stop();
    void setMusicFolder(const QString &folder);
    // Remove all files and folders (apart from Music folder!) associated with use MPD instance...
    void cleanup();

    const MPDConnectionDetails & details(bool createFiles=false) { init(createFiles); return det; }
    void setDetails(const MPDConnectionDetails &d);

private:
    void init(bool create);
    int getPid();
    bool controlMpd(bool stop);

private:
    QString mpdExe;
    QString pidFileName;
    MPDConnectionDetails det;
};

#endif
