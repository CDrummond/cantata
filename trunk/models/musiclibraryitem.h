/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QList>
#include <QVariant>
#include <QSet>

class MusicLibraryItemContainer;
class MusicLibraryItem
{
public:
    enum Type {
        Type_Root,
        Type_Artist,
        Type_Album,
        Type_Song,

        Type_Podcast
    };

    MusicLibraryItem(const QString &data, MusicLibraryItemContainer *parent);
    virtual ~MusicLibraryItem() { }

    MusicLibraryItemContainer * parentItem() const { return m_parentItem; }
    virtual MusicLibraryItem * childItem(int) const { return 0; }
    virtual int childCount() const { return 0; }
    int row() const;
    void setRow(int r) const { m_row=r+1; }
    int columnCount() const { return 1; }
    const QString & data() const { return m_itemData; }
    virtual QString displayData(bool full=false) const { Q_UNUSED(full) return m_itemData; }
    void setData(const QString &d) { m_itemData=d; }
    void setParent(MusicLibraryItemContainer *p);
    Qt::CheckState checkState() const { return m_checkState; }
    void setCheckState(Qt::CheckState s) { m_checkState=s; }

    virtual bool hasGenre(const QString &genre) const=0;
    virtual QSet<QString> allGenres() const=0;
    virtual Type itemType() const=0;

protected:
    friend class MusicLibraryItemContainer;
    MusicLibraryItemContainer *m_parentItem;
    QString m_itemData;
    Qt::CheckState m_checkState;
    mutable quint32 m_row;
};

class MusicLibraryItemContainer : public MusicLibraryItem
{
public:
    MusicLibraryItemContainer(const QString &data, MusicLibraryItemContainer *parent) : MusicLibraryItem(data, parent), m_rowsSet(false) { }
    virtual ~MusicLibraryItemContainer() { clear(); }

    virtual void append(MusicLibraryItem *i) { m_childItems.append(i); }
    virtual MusicLibraryItem * childItem(int row) const { return m_childItems.value(row); }
    MusicLibraryItem * childItem(const QString &name) const;

    int childCount() const { return m_childItems.count(); }
    const QList<MusicLibraryItem *> & childItems() const { return m_childItems; }
    void addGenre(const QString &genre) { m_genres.insert(genre); }
    bool hasGenre(const QString &genre) const { return m_genres.contains(genre); }
    const QSet<QString> & genres() const { return m_genres; }
    QSet<QString> allGenres() const { return genres(); }
    void updateGenres();
    void resetRows();
    void clear();
    int indexOf(MusicLibraryItem *c) const { return m_childItems.indexOf(c); }

protected:
    friend class MusicLibraryItem;
    QList<MusicLibraryItem *> m_childItems;
    QSet<QString> m_genres;
    bool m_rowsSet;
};

#endif
