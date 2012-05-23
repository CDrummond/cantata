/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <KDE/ThreadWeaver/Job>
#include "song.h"
#include "tags.h"

class TagReader : public ThreadWeaver::Job
{
    Q_OBJECT

public:
    TagReader(QObject *p);
    ~TagReader();

    void setDetails(const QList<Song> &s, const QString &dir);
    void requestAbort() { abortRequested=true; }

private:
    void run();

Q_SIGNALS:
    void progress(int index, Tags::ReplayGain);

private:
    bool abortRequested;
    QList<Song> songs;
    QString baseDir;
};

#endif
