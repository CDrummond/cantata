/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef SMART_PLAYLISTS_H
#define SMART_PLAYLISTS_H

#include "rulesplaylists.h"

class SmartPlaylists : public RulesPlaylists
{
    Q_OBJECT

public:
    static SmartPlaylists * self();

    SmartPlaylists();
    ~SmartPlaylists() override { }

    QString name() const override;
    QString title() const override;
    QString descr() const override;
    QVariant data(const QModelIndex &index, int role) const override;
    int maxTracks() const override { return 10000; }
    int defaultNumTracks() const override { return 100; }
};

#endif
