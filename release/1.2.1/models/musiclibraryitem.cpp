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

#include "musiclibraryitem.h"

MusicLibraryItem::MusicLibraryItem(const QString &data, MusicLibraryItemContainer *parent)
        : m_parentItem(parent)
        , m_itemData(data)
        , m_checkState(Qt::Unchecked)
        , m_row(0)
{
}

int MusicLibraryItem::row() const
{
    // Calculate row value of this item. Use 0 to mean not-yet calcualted. Therefore, we
    // store rows as value+1 - so need to decrement upon return!
    if (m_row>0) {
        return m_row-1;
    } else if (m_parentItem) {
        m_row=m_parentItem->m_childItems.indexOf(const_cast<MusicLibraryItem*>(this))+1;
        m_parentItem->m_rowsSet=true;
        return m_row-1;
    }
    return 0;
}

void MusicLibraryItem::setParent(MusicLibraryItemContainer *p)
{
    if (p==m_parentItem) {
        return;
    }
    if (m_parentItem) {
        m_parentItem->m_childItems.removeAll(this);
        m_parentItem->resetRows();
    }
    m_parentItem=p;
    m_parentItem->m_childItems.append(this);
    m_parentItem->m_genres+=allGenres();
}

MusicLibraryItem * MusicLibraryItemContainer::childItem(const QString &name) const
{
    foreach (MusicLibraryItem *i, m_childItems) {
        if (i->data()==name) {
            return i;
        }
    }

    return 0;
}

void MusicLibraryItemContainer::updateGenres()
{
    m_genres.clear();
    foreach (MusicLibraryItem *i, m_childItems) {
        if (MusicLibraryItem::Type_Song!=i->itemType()) {
            static_cast<MusicLibraryItemContainer *>(i)->updateGenres();
        }
        m_genres+=i->allGenres();
    }
}

void MusicLibraryItemContainer::resetRows()
{
    if (m_rowsSet) {
        foreach (MusicLibraryItem *i, m_childItems) {
            i->m_row=0;
        }
        m_rowsSet=false;
    }
}

void MusicLibraryItemContainer::clear()
{
    qDeleteAll(m_childItems);
    m_childItems.clear();
    m_genres.clear();
    m_rowsSet=false;
}
