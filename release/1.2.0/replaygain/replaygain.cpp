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

#include "replaygain.h"
#include "jobcontroller.h"
#include <QCoreApplication>
#include <stdio.h>
#include <locale.h>

ReplayGain::ReplayGain(const QStringList &fileNames)
    : QObject(0)
    , files(fileNames)
    , lastProgress(-1)
    , totalScanned(0)
{
    TrackScanner::init();
    JobController::self()->setMaxActive(8);
    setlocale(LC_NUMERIC, "C");
}

ReplayGain::~ReplayGain()
{
    clearScanners();
}

void ReplayGain::scan()
{
    for (int i=0; i<files.count(); ++i) {
        if (scanners.count()<100) {
            createScanner(i);
        } else {
            toScan.append(i);
        }
    }
}

void ReplayGain::createScanner(int index)
{
    TrackScanner *s=new TrackScanner(index);
    s->setFile(files.at(index));
    connect(s, SIGNAL(progress(int)), this, SLOT(scannerProgress(int)));
    connect(s, SIGNAL(done()), this, SLOT(scannerDone()));
    scanners.insert(index, s);
    JobController::self()->add(s);
}

void ReplayGain::clearScanners()
{
    JobController::self()->cancel();
    QMap<int, TrackScanner *>::ConstIterator it(scanners.constBegin());
    QMap<int, TrackScanner *>::ConstIterator end(scanners.constEnd());

    for (; it!=end; ++it) {
        it.value()->stop();
    }
    scanners.clear();
    toScan.clear();
    tracks.clear();
}

void ReplayGain::showProgress()
{
    int finished=0;
    quint64 totalProgress=0;
    QMap<int, Track>::iterator it=tracks.begin();
    QMap<int, Track>::iterator end=tracks.end();

    for (; it!=end; ++it) {
        if ((*it).finished) {
            finished++;;
        }
        totalProgress+=(*it).finished ? 100 : (*it).progress;
    }
    int progress=(totalProgress/files.count())+0.5;
    progress=(progress/5)*5;
    if (progress!=lastProgress) {
        lastProgress=progress;
        printf("PROGRESS: %02d\n", lastProgress);
        fflush(stdout);
    }
}

void ReplayGain::showResults()
{
    QList<TrackScanner *> okScanners;
    for (int i=0; i<files.count(); ++i) {
        TrackScanner *s=scanners[i];
        const Track &t=tracks[i];
        if (t.success) {
            printf("TRACK: %d %f %f\n", i, TrackScanner::reference(s->results().loudness), s->results().peakValue());
            okScanners.append(s);
        } else {
            printf("TRACK: %d FAILED\n", i);
        }
    }

    if (okScanners.isEmpty()) {
        printf("ALBUM: FAILED\n");
    } else {
        TrackScanner::Data album=TrackScanner::global(okScanners);
        printf("ALBUM: %f %f\n", TrackScanner::reference(album.loudness), album.peak);
    }
    fflush(stdout);

    QCoreApplication::exit(0);
}

void ReplayGain::scannerProgress(int p)
{
    TrackScanner *s=qobject_cast<TrackScanner *>(sender());
    if (!s) {
        return;
    }

    tracks[s->index()].progress=p;
    showProgress();
}

void ReplayGain::scannerDone()
{
    TrackScanner *s=qobject_cast<TrackScanner *>(sender());
    if (!s) {
        return;
    }
    Track &track=tracks[s->index()];
    if (!track.finished) {
        track.finished=true;
        track.success=s->success();
        track.progress=100;
        showProgress();
        totalScanned++;
    }

    if (toScan.isEmpty()) {
        if (totalScanned==files.count()) {
            showResults();
        }
    } else {
        int index=toScan.takeAt(0);
        createScanner(index);
    }
}
