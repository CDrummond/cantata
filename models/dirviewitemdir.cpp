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

#include "dirviewitemdir.h"
#include "dirviewitemfile.h"
#include <QtCore/QStringList>

DirViewItem * DirViewItemDir::createDirectory(const QString &dirName)
{
    DirViewItemDir *dir = new DirViewItemDir(dirName, this);
    m_indexes.insert(dirName, m_childItems.count());
    m_childItems.append(dir);
    return dir;
}

DirViewItem * DirViewItemDir::insertFile(const QString &fileName)
{
    DirViewItemFile *file = new DirViewItemFile(fileName, this);
    m_indexes.insert(fileName, m_childItems.count());
    m_childItems.append(file);
    return file;
}

void DirViewItemDir::insertFile(const QStringList &path)
{
    if (1==path.count()) {
        insertFile(path[0]);
    } else {
        static_cast<DirViewItemDir *>(createDirectory(path[0]))->insertFile(path.mid(1));
    }
}

void DirViewItemDir::remove(DirViewItem *dir)
{
    int index=m_childItems.indexOf(dir);

    if (-1==index) {
        return;
    }
    bool updateIndexes=(m_childItems.count()-1)!=index;
    DirViewItem *d=m_childItems.takeAt(index);

    if (updateIndexes) {
        m_indexes.clear();
        int idx=0;
        foreach (DirViewItem *i,  m_childItems) {
            m_indexes.insert(i->name(), idx++);
        }
    } else {
        m_indexes.remove(d->name());
    }
}

QSet<QString> DirViewItemDir::allFiles() const
{
    QSet<QString> files;

    foreach (DirViewItem *i,  m_childItems) {
        if (Type_Dir==i->type()) {
            files+=static_cast<DirViewItemDir *>(i)->allFiles();
        } else if (Type_File==i->type()) {
            files.insert(i->fullName());
        }
    }

    return files;
}
