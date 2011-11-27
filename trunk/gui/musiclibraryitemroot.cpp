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


MusicLibraryItemRoot::MusicLibraryItemRoot(const QString &data)
    : MusicLibraryItem(data, MusicLibraryItem::Type_Root)
{
}

MusicLibraryItemRoot::~MusicLibraryItemRoot()
{
    qDeleteAll(m_childItems);
}

void MusicLibraryItemRoot::appendChild(MusicLibraryItem * const item)
{
    m_childItems.append(static_cast<MusicLibraryItemArtist *>(item));
}

/**
 * Insert a new child item at a given place
 *
 * @param child The child item
 * @param place The place to insert the child item
 */
void MusicLibraryItemRoot::insertChild(MusicLibraryItem * const child, const int place)
{
    m_childItems.insert(place, static_cast<MusicLibraryItemArtist *>(child));
}

MusicLibraryItem * MusicLibraryItemRoot::child(int row) const
{
    return m_childItems.value(row);
}

int MusicLibraryItemRoot::childCount() const
{
    return m_childItems.count();
}

void MusicLibraryItemRoot::clearChildren()
{
    qDeleteAll(m_childItems);
}
