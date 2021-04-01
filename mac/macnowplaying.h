/*
 * Cantata
 *
 * Copyright (c) 2021 David Hoyes <dphoyes@gmail.com>
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

#ifndef MACMEDIAPLAYER_H
#define MACMEDIAPLAYER_H

#include <QObject>
#include "mpd-interface/song.h"
#include "mpd-interface/mpdstatus.h"

class MacNowPlaying : public QObject
{
    Q_OBJECT

public:
    MacNowPlaying(QObject *p);
    ~MacNowPlaying() override;

public:
    void updateCurrentSong(const Song &song);
    void updateStatus(MPDStatus *const status);

public Q_SLOTS:
    void updateCurrentCover(const QString &fileName);

Q_SIGNALS:
    void setSeekId(qint32 songId, quint32 time);

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

#endif
