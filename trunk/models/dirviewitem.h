/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtCore/QSet>

class DirViewItem
{
public:
    enum Type {
        Type_Root,
        Type_Dir,
        Type_File
    };

    DirViewItem(const QString &name=QString(), DirViewItem *p=0)
        : m_parentItem(p)
        , m_name(name) {
    }
    virtual ~DirViewItem() {
    }

    int row() const {
        return m_parentItem ? m_parentItem->indexOf(const_cast<DirViewItem *>(this)) : 0;
    }
    virtual int indexOf(DirViewItem *) const {
        return 0;
    }
    DirViewItem * parent() const {
        return m_parentItem;
    }
    virtual int childCount() const {
        return 0;
    }
    virtual DirViewItem * child(int) const {
        return 0;
    }
    QString fullName();
    int columnCount() const {
        return 1;
    }
    const QString & data() const {
        return m_name;
    }
    const QString & name() const {
        return m_name;
    }
    virtual Type type() const=0;

protected:
    DirViewItem * m_parentItem;
    QString m_name;
};

#endif
