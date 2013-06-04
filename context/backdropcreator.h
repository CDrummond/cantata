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

#ifndef BACKDROP_CREATOR_H
#define BACKDROP_CREATOR_H

#include <QObject>
#include <QSet>
#include "song.h"

class Thread;
class QImage;

class BackdropCreator : public QObject
{
    Q_OBJECT
public:
    BackdropCreator();
    virtual ~BackdropCreator();

Q_SIGNALS:
    void created(const QString &artist, const QImage &img);

public Q_SLOTS:
    void create(const QString &artist, const QList<Song> &songs);

private Q_SLOTS:
    void coverRetrieved(const Song &s, const QImage &img, const QString &file);

private:
    void createImage();

private:
    int imageSize;
    QString requestedArtist;
    QSet<Song> requested;
    QList<QImage> images;
    QList<Song> albums;
    Thread *thread;
};

#endif
