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
#ifndef DIRVIEWITEMDIR_H
#define DIRVIEWITEMDIR_H

#include "dirviewitem.h"

class DirViewItemDir : public DirViewItem
{
public:
    DirViewItemDir(const QString &name=QString(), DirViewItem *parent=0)
        : DirViewItem(name, parent) {
    }

    virtual ~DirViewItemDir() {
        qDeleteAll(m_childItems);
    }

    virtual int indexOf(DirViewItem *c) const {
        return m_childItems.indexOf(c);
    }
    int childCount() const {
        return m_childItems.count();
    }
    DirViewItem * child(int row) const {
        return m_childItems.value(row);
    }
    DirViewItem * child(const QString &name) const {
        return m_indexes.contains(name) ? m_childItems.value(m_indexes[name]) : 0;
    }
    DirViewItem * createDirectory(const QString &dirName);
    DirViewItem * insertFile(const QString &fileName);
    void insertFile(const QStringList &path);
    void remove(DirViewItem *dir);

    bool hasChild(const QString &name) {
        return m_indexes.contains(name);
    }

    QSet<QString> allFiles() const;
    Type type() const {
        return Type_Dir;
    }

private:
    QHash<QString, int> m_indexes;
    QList<DirViewItem *> m_childItems;
};

#endif
