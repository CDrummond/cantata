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

#ifndef MUSIC_LIBRARY_ITEM_H
#define MUSIC_LIBRARY_ITEM_H

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QSet>

class MusicLibraryItem
{
public:
    enum Type {
        Type_Root,
        Type_Artist,
        Type_Album,
        Type_Song
    };
    MusicLibraryItem(const QString &data, Type type, MusicLibraryItem *parent)
        : m_parentItem(parent)
        , m_type(type)
        , m_itemData(data) {
    }

    virtual ~MusicLibraryItem() {
        qDeleteAll(m_childItems);
    }

    MusicLibraryItem * parent() const {
        return m_parentItem;
    }

    void setParent(MusicLibraryItem *p) {
        if (p==m_parentItem) {
            return;
        }
        if (m_parentItem) {
            m_parentItem->m_childItems.removeAll(this);
        }
        m_parentItem=p;
        m_parentItem->m_childItems.append(this);
    }

    void append(MusicLibraryItem *i) {
        m_childItems.append(i);
    }

    MusicLibraryItem * child(int row) const {
        return m_childItems.value(row);
    }

    int childCount() const {
        return m_childItems.count();
    }

    const QList<MusicLibraryItem *> & children() {
        return m_childItems;
    }

    int row() const {
        return m_parentItem ? m_parentItem->m_childItems.indexOf(const_cast<MusicLibraryItem*>(this)) : 0;
    }

    int columnCount() const {
        return 1;
    }

    void addGenre(const QString &genre) {
        m_genres.insert(genre);
    }

    bool hasGenre(const QString &genre) const {
        return m_genres.contains(genre);
    }

    const QSet<QString> & genres() const {
        return m_genres;
    }

    const QString & data() const {
        return m_itemData;
    }

    MusicLibraryItem::Type type() const {
        return m_type;
    }

protected:
    MusicLibraryItem *m_parentItem;
    QList<MusicLibraryItem *> m_childItems;
    Type m_type;
    QString m_itemData;
    QSet<QString> m_genres;
};

#endif
