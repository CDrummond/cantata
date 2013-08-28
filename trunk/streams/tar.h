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

#ifndef TAR_H
#define TAR_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QByteArray>
#include <QFile>

class QtIOCompressor;
class Tar
{
public:
    Tar(const QString &fileName);
    ~Tar();

    bool open();
    QMap<QString, QByteArray> extract(const QStringList &files);
    
private:
    QFile file;
    QtIOCompressor *compressor;
    QIODevice *dev;
};

#endif

