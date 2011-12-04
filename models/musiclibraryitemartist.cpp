/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

void MusicLibraryItemArtist::appendChild(MusicLibraryItem * const item)
{
    m_childItems.append(static_cast<MusicLibraryItemAlbum *>(item));
}

/**
 * Insert a new child item at a given place
 *
 * @param child The child item
 * @param place The place to insert the child item
 */
void MusicLibraryItemArtist::insertChild(MusicLibraryItem * const child, const int place)
{
    m_childItems.insert(place, static_cast<MusicLibraryItemAlbum *>(child));
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

void MusicLibraryItemArtist::clearChildren()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
}

const QString & MusicLibraryItemArtist::baseArtist() const
{
    return nonTheArtist.isEmpty() ? m_itemData : nonTheArtist;
}

