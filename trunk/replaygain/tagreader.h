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

#ifndef _TAGREADER_H_
#define _TAGREADER_H_

#include "jobcontroller.h"
#include "song.h"
#include "tags.h"

class TagReader : public Job
{
    Q_OBJECT

public:
    TagReader() { }
    virtual ~TagReader() { }

    void setDetails(const QList<Song> &s, const QString &dir);

private:
    void run();

Q_SIGNALS:
    void progress(int index, Tags::ReplayGain);

private:
    QList<Song> songs;
    QString baseDir;
};

#endif
