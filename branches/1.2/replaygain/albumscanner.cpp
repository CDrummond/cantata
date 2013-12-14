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

#include "albumscanner.h"
#include "config.h"
#include <QProcess>
#include <locale.h>

AlbumScanner::AlbumScanner(const QMap<int, QString> &files)
    : proc(0)
    , oldLocale(setlocale(LC_NUMERIC, "C"))
{
    QMap<int, QString>::ConstIterator it=files.constBegin();
    QMap<int, QString>::ConstIterator end=files.constEnd();

    for (int i=0; it!=end; ++it, ++i) {
        fileNames.append(it.value());
        trackIndexMap[i]=it.key();
    }
}

AlbumScanner::~AlbumScanner()
{
    setlocale(LC_NUMERIC, oldLocale);
    stop();
}

void AlbumScanner::start()
{
    if (!proc) {
        proc=new QProcess(this);
        proc->setReadChannelMode(QProcess::MergedChannels);
        proc->setReadChannel(QProcess::StandardOutput);
        connect(proc, SIGNAL(finished(int)), this, SLOT(procFinished()));
        connect(proc, SIGNAL(readyReadStandardOutput()), this, SLOT(read()));
        proc->start(INSTALL_PREFIX"/lib/cantata/cantata-replaygain", fileNames, QProcess::ReadOnly);
    }
}

void AlbumScanner::stop()
{
    if (proc) {
        disconnect(proc, SIGNAL(finished(int)), this, SLOT(procFinished()));
        disconnect(proc, SIGNAL(readyReadStandardOutput()), this, SLOT(read()));
        proc->terminate();
        proc->deleteLater();
        proc=0;
    }
}

static const QString constProgLine=QLatin1String("PROGRESS: ");
static const QString constTrackLine=QLatin1String("TRACK: ");
static const QString constAlbumLine=QLatin1String("ALBUM: ");

void AlbumScanner::read()
{
    if (!proc) {
        return;
    }

    QString output = proc->readAllStandardOutput().data();
    if (output.isEmpty()) {
        return;
    }

    QStringList lines=output.split("\n", QString::SkipEmptyParts);

    foreach (const QString &line, lines) {
        if (line.startsWith(constProgLine)) {
            emit progress(line.mid(constProgLine.length()).toUInt());
        } else if (line.startsWith(constTrackLine)) {
            QStringList parts=line.mid(constTrackLine.length()).split(" ", QString::SkipEmptyParts);
            if (!parts.isEmpty()) {
                int num=parts[0].toUInt();
                Values vals;
                if (parts.length()>=3) {
                    vals.gain=parts[1].toDouble();
                    vals.peak=parts[2].toDouble();
                    vals.ok=true;
                }
                tracks[trackIndexMap[num]]=vals;
            }
        } else if (line.startsWith(constAlbumLine)) {
            QStringList parts=line.mid(constAlbumLine.length()).split(" ", QString::SkipEmptyParts);
            if (parts.length()>=2) {
                album.gain=parts[0].toDouble();
                album.peak=parts[1].toDouble();
                album.ok=true;
            }
        }
    }
}

void AlbumScanner::procFinished()
{
    setFinished(true);
    emit done();
}
