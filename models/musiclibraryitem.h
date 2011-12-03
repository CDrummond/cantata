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

#include <QList>
#include <QVariant>
#include <QSet>

class MusicLibraryItem
{
public:
    enum Type {
        Type_Root,
        Type_Artist,
        Type_Album,
        Type_Song
    };
    MusicLibraryItem(const QString &data, Type type);
    virtual ~MusicLibraryItem();

    virtual MusicLibraryItem * child(int /*row*/) const {
        return NULL;
    }
    virtual int childCount() const {
        return 0;
    }
    virtual int row() const {
        return 0;
    }
    virtual MusicLibraryItem * parent() const {
        return NULL;
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

    QVariant data(int column) const;
    MusicLibraryItem::Type type() const;

protected:
    Type m_type;
    QString m_itemData;
    QSet<QString> m_genres;
};

#endif
