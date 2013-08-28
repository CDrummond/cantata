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

#include "tar.h"
#include "qtiocompressor/qtiocompressor.h"

Tar::Tar(const QString &fileName)
    : file(fileName)
    , compressor(0)
{
}

Tar::~Tar()
{
    delete compressor;
}

bool Tar::open()
{
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Check for gzip header...
    QByteArray header=file.read(2);
    bool isCompressed=((unsigned char)header[0])==0x1f && ((unsigned char)header[1])==0x8b;
    file.seek(0);

    if (isCompressed) {
         compressor=new QtIOCompressor(&file);
         compressor->setStreamFormat(QtIOCompressor::GzipFormat);
         if (!compressor->open(QIODevice::ReadOnly)) {
             return false;
         }
         dev=compressor;
    } else {
        dev=&file;
    }
    return true;
}

static const qint64 constHeaderLen=512;

static qint64 roundUp(qint64 sz)
{
    return ((sz/constHeaderLen)*constHeaderLen)+(sz%constHeaderLen ? constHeaderLen : 0);
}

struct TarHeader
{
    TarHeader() : fileSize(0) { }
    bool ok() const { return fileSize>0 && !fileName.isEmpty(); }
    QString fileName;
    qint64 fileSize;
};

static unsigned int octStrToInt(char *ch, unsigned int size) 
{
    unsigned int val = 0;
    while (size > 0){
        if (ch) {
            val = (val * 8) + (*ch - '0');
        }
        ch++;
        size--;
    }
    return val;
}

static TarHeader readHeader(QIODevice *dev)
{
    TarHeader header;
    char buffer[constHeaderLen];
    qint64 bytesRead=dev->read(buffer, constHeaderLen);
    if (constHeaderLen==bytesRead && ('0'==buffer[156] || '\0'==buffer[156])) {
        buffer[100]='\0';
        header.fileName=QFile::decodeName(buffer);
        header.fileSize=octStrToInt(&buffer[124], 11);
    }
    return header;
}

QMap<QString, QByteArray> Tar::extract(const QStringList &files)
{
    QMap<QString, QByteArray> data;
    if (!dev) {
        return data;
    }
    qint64 offset=0;
    qint64 pos=0;
    for (;;) {
        TarHeader header=readHeader(dev);
        if (header.ok()) {
            pos+=constHeaderLen;
            if (files.contains(header.fileName) && !data.contains(header.fileName)) {
                data[header.fileName]=dev->read(header.fileSize);
                pos+=header.fileSize;
            }
            offset+=constHeaderLen+header.fileSize;
            offset=roundUp(offset);
            if (dev->isSequential()) {
                static const qint64 constSkipBlock=1024;
                // Can't seek with QtIOCompressor - so fake this by reading and discarding
                while (pos<offset) {
                    qint64 toRead=qMin(constSkipBlock, offset-pos);
                    dev->read(toRead);
                    pos+=toRead;
                }
            } else {
                dev->seek(offset);
            }
        } else {
            break;
        }
    }
    return data;
}
