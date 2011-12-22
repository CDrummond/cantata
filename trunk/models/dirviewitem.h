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
#ifndef DIRVIEWITEM_H
#define DIRVIEWITEM_H

#include <QString>
#include <QList>
#include <QVariant>

class DirViewItem
{
public:
    enum Type {
        Type_Root,
        Type_Dir,
        Type_File
    };

    DirViewItem(const QString name, Type type, DirViewItem *p)
        : m_parentItem(p)
        , m_name(name)
        , m_type(type) {
    }
    virtual ~DirViewItem() {
        qDeleteAll(m_childItems);
    }
    int row() const {
        return m_parentItem ? m_parentItem->m_childItems.indexOf(const_cast<DirViewItem *>(this)) : 0;
    }
    DirViewItem * parent() const {
        return m_parentItem;
    }
    int childCount() const {
        return m_childItems.count();;
    }
    DirViewItem * child(int row) const {
        return m_childItems.value(row);;
    }
    QString fullName();
    int columnCount() const {
        return 1;
    }
    const QString & data() const {
        return m_name;
    }
    const QString & name() {
        return m_name;
    }
    DirViewItem::Type type() const {
        return m_type;
    }

protected:
    DirViewItem * m_parentItem;
    QList<DirViewItem *> m_childItems;
    QString m_name;
    Type m_type;
};

#endif
