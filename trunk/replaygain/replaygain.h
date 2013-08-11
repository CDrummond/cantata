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

#ifndef _REPLAYGAIN_H_
#define _REPLAYGAIN_H_

#include <QObject>
#include <QList>
#include <QMap>
#include <QStringList>
#include "trackscanner.h"
#include "config.h"

class ReplayGain : public QObject
{
    Q_OBJECT

public:
    ReplayGain(const QStringList &fileNames);
    virtual ~ReplayGain();

public Q_SLOTS:
    void scan();

private:
    void createScanner(int index);
    void clearScanners();
    void showProgress();
    void showResults();

private Q_SLOTS:
    void scannerProgress(int p);
    void scannerDone();

private:
    struct Track {
        Track() : progress(0), finished(false), success(false) { }
        unsigned char progress;
        bool finished : 1;
        bool success : 1;
    };

    QStringList files;
    QMap<int, TrackScanner *> scanners;
    QList<int> toScan;
    QMap<int, Track> tracks;
    int lastProgress;
    int totalScanned;
};

#endif
