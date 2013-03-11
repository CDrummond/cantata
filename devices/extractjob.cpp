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
 */

#include "extractjob.h"
#include "device.h"
#include "utils.h"
#include "tags.h"
#include "cdparanoia.h"
#include "covers.h"
#include "mpdconnection.h"
#include "settings.h"
#include <QStringList>
#include <QProcess>
#include <QFile>

void ExtractJob::writeWavHeader(QIODevice &dev)
{
    static const unsigned char riffHeader[] = {
        0x52, 0x49, 0x46, 0x46, // 0  "RIFF"
        0x00, 0x00, 0x00, 0x00, // 4  wavSize
        0x57, 0x41, 0x56, 0x45, // 8  "WAVE"
        0x66, 0x6d, 0x74, 0x20, // 12 "fmt "
        0x10, 0x00, 0x00, 0x00, // 16
        0x01, 0x00, 0x02, 0x00, // 20
        0x44, 0xac, 0x00, 0x00, // 24
        0x10, 0xb1, 0x02, 0x00, // 28
        0x04, 0x00, 0x10, 0x00, // 32
        0x64, 0x61, 0x74, 0x61, // 36 "data"
        0x00, 0x00, 0x00, 0x00  // 40 byteCount
    };

    dev.write((char*)riffHeader, 44);
}

ExtractJob::ExtractJob(const Encoders::Encoder &enc, int val, const QString &src, const QString &dest, const Song &s, const QString &cover)
    : encoder(enc)
    , value(val)
    , srcFile(src)
    , destFile(dest)
    , song(s)
    , coverFile(cover)
    , copiedCover(false)
{
}

ExtractJob::~ExtractJob()
{
}

void ExtractJob::run()
{
    if (stopRequested) {
        emit result(Device::Cancelled);
    } else {
        QStringList encParams=encoder.params(value, "pipe:", destFile);
        CdParanoia cdparanoia(srcFile, Settings::self()->paranoiaFull(), Settings::self()->paranoiaNeverSkip());

        if (!cdparanoia) {
            emit result(Device::FailedToLockDevice);
            return;
        }
        QProcess process;
        QString cmd=encParams.takeFirst();
        process.start(cmd, encParams, QIODevice::WriteOnly);
        process.waitForStarted();

        if (stopRequested) {
            emit result(Device::Cancelled);
            process.close();
            return;
        }
        int firstSector = cdparanoia.firstSectorOfTrack(song.id);
        int lastSector = cdparanoia.lastSectorOfTrack(song.id);
        int total=lastSector-firstSector;
        int count=0;

        cdparanoia.seek(firstSector, SEEK_SET);

        writeWavHeader(process);
        while ((firstSector+count) <= lastSector) {
            qint16 *buf = cdparanoia.read();
            if (!buf) {
                emit result(Device::Failed);
                process.close();
                return;
            }
            if (stopRequested) {
                emit result(Device::Cancelled);
                process.close();
                return;
            }

            char *buffer=(char *)buf;

            qint64 writePos=0;
            do {
                qint64 bytesWritten = process.write(&buffer[writePos], CD_FRAMESIZE_RAW - writePos);
                if (stopRequested) {
                    emit result(Device::Cancelled);
                    process.close();
                    return;
                }
                if (-1==bytesWritten) {
                    emit result(Device::WriteFailed);
                    process.close();
                    return;
                }
                writePos+=bytesWritten;
            } while (writePos<CD_FRAMESIZE_RAW);

            count++;
            setPercent((count*100/total)+0.5);
        }
        process.closeWriteChannel();
        process.waitForFinished();
        Utils::setFilePerms(destFile);
        Tags::update(destFile, Song(), song, 3);

        if (!stopRequested && !coverFile.isEmpty()) {
            QString mpdCover=MPDConnection::self()->getDetails().coverName;
            if (mpdCover.isEmpty()) {
                mpdCover=Covers::constFileName;
            }
            copiedCover=Covers::copyImage(Utils::getDir(coverFile), Utils::getDir(destFile), Utils::getFile(coverFile), mpdCover+coverFile.mid(coverFile.length()-4), 0);
        }

        emit result(Device::Ok);
    }
}
