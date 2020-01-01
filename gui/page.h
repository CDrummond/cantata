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

#ifndef PAGE_H
#define PAGE_H

#include "mpd-interface/song.h"
#include "mpd-interface/mpdconnection.h"

class Page
{
public:
    Page() { }
    virtual ~Page() { }
    virtual Song coverRequest() const { return Song(); }
    virtual QList<Song> selectedSongs(bool allowPlaylists=false) const { Q_UNUSED(allowPlaylists) return QList<Song>(); }
    virtual void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) {
        Q_UNUSED(name) Q_UNUSED(action) Q_UNUSED(priority) Q_UNUSED(decreasePriority)
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    virtual void addSelectionToDevice(const QString &udi) { Q_UNUSED(udi) }
    virtual void deleteSongs() { }
    #endif
    virtual void focusSearch() { }
    virtual void removeItems() { }
    virtual void controlActions() { }
};

#endif
