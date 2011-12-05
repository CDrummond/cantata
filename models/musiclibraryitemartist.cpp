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
#include "musiclibraryitemalbum.h"
#include "song.h"
#include "mpdparseutils.h"

MusicLibraryItemArtist::MusicLibraryItemArtist(const QString &data, MusicLibraryItem *parent)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Artist),
      m_parentItem(static_cast<MusicLibraryItemRoot *>(parent))
{
    if (m_itemData.startsWith(QLatin1String("The "))) {
        nonTheArtist=m_itemData.mid(4);
    }
}

MusicLibraryItemArtist::~MusicLibraryItemArtist()
{
    qDeleteAll(m_childItems);
}

MusicLibraryItemAlbum * MusicLibraryItemArtist::album(const Song &s)
{
    QHash<QString, int>::Iterator it=m_indexes.find(s.album);

    if (m_indexes.end()==it) {
        MusicLibraryItemAlbum *item=new MusicLibraryItemAlbum(s.album, MPDParseUtils::getDir(s.file), this);
        m_indexes[s.album]=m_childItems.count();
        m_childItems.append(item);
        return item;
    }
    return m_childItems.at(*it);
}

MusicLibraryItem * MusicLibraryItemArtist::child(int row) const
{
    return m_childItems.value(row);
}

int MusicLibraryItemArtist::childCount() const
{
    return m_childItems.count();
}

MusicLibraryItem * MusicLibraryItemArtist::parent() const
{
    return m_parentItem;
}

void MusicLibraryItemArtist::setParent(MusicLibraryItem * const parent)
{
    m_parentItem = static_cast<MusicLibraryItemRoot *>(parent);
}

int MusicLibraryItemArtist::row() const
{
    return m_parentItem->m_childItems.indexOf(const_cast<MusicLibraryItemArtist*>(this));
}

const QString & MusicLibraryItemArtist::baseArtist() const
{
    return nonTheArtist.isEmpty() ? m_itemData : nonTheArtist;
}

