/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "musiclibraryitemroot.h"
#include "musiclibraryitemartist.h"
#include "song.h"

MusicLibraryItemRoot::MusicLibraryItemRoot(const QString &data)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Root)
{
}

MusicLibraryItemRoot::~MusicLibraryItemRoot()
{
    qDeleteAll(m_childItems);
}

MusicLibraryItemArtist * MusicLibraryItemRoot::artist(const Song &s)
{
    QString aa=s.albumArtist();
    QMap<QString, int>::Iterator it=m_indexes.find(aa);

    if (m_indexes.end()==it) {
        MusicLibraryItemArtist *item=new MusicLibraryItemArtist(aa, this);
        m_indexes[aa]=m_childItems.count();
        m_childItems.append(item);
        return item;
    }
    return m_childItems.at(*it);
}

MusicLibraryItem * MusicLibraryItemRoot::child(int row) const
{
    return m_childItems.value(row);
}

int MusicLibraryItemRoot::childCount() const
{
    return m_childItems.count();
}
