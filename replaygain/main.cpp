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

#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <stdio.h>
#include "replaygain.h"

int main(int argc, char *argv[])
{
    if (argc<2) {
        printf("Usage: %s <file 1..N>\n", argv[0]);
        return -1;
    }

    QStringList fileNames;
    for (int i=0; i<argc-1; ++i) {
        fileNames.append(QFile::decodeName(argv[i+1]));
    }

    QCoreApplication app(argc, argv);
    ReplayGain *rg=new ReplayGain(fileNames);
    QTimer::singleShot(0, rg, SLOT(scan()));
    return app.exec();
}
