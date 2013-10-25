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

#include "tagserver.h"
#include "tags.h"
#include <QDataStream>
#include <QVariant>
#include <QCoreApplication>
#include <QFile>

TagServer::TagServer()
{
    in=new QFile(0);
    in->open(stdin, QIODevice::ReadOnly);
    out=new QFile(0);
    out->open(stdout, QIODevice::WriteOnly);
}

TagServer::~TagServer()
{
    delete in;
    delete out;
}

int TagServer::process()
{
    while (in->isReadable() && out->isWritable()) {
        QString request;
        QString fileName;
        QDataStream inStream(in);
        QDataStream outStream(out);

        inStream >> request >> fileName;

        if (QLatin1String("read")==request) {
            outStream << Tags::read(fileName);
            out->flush();
        } else if (QLatin1String("readImage")==request) {
            outStream << Tags::readImage(fileName);
            out->flush();
        } else if (QLatin1String("readLyrics")==request) {
            outStream << Tags::readLyrics(fileName);
            out->flush();
        } else if (QLatin1String("updateArtistAndTitle")==request) {
            Song song;
            outStream << (int)Tags::updateArtistAndTitle(fileName, song);
            out->flush();
        } else if (QLatin1String("update")==request) {
            Song from;
            Song to;
            int id3Ver;
            inStream >> from >> to >> id3Ver;
            outStream << (int)Tags::update(fileName, from, to, id3Ver);
            out->flush();
        } else if (QLatin1String("readReplaygain")==request) {
            Tags::ReplayGain rg=Tags::readReplaygain(fileName);
            outStream << rg;
            out->flush();
        } else if (QLatin1String("updateReplaygain")==request) {
            Tags::ReplayGain rg;
            inStream >> rg;
            outStream << (int)Tags::updateReplaygain(fileName, rg);
            out->flush();
        } else if (QLatin1String("embedImage")==request) {
            QByteArray cover;
            inStream >> cover;
            outStream << (int)Tags::embedImage(fileName, cover);
            out->flush();
        }
    }
    return 0;
}
