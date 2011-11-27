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

#include "dirviewitemroot.h"
#include "dirviewitemdir.h"
#include "dirviewitemfile.h"

DirViewItemDir::DirViewItemDir(const QString name, DirViewItem *parent)
    : DirViewItem(name, DirViewItem::Type_Dir),
      d_parentItem(parent)
{
}

DirViewItemDir::~DirViewItemDir()
{
    qDeleteAll(d_childItems);
}

int DirViewItemDir::row() const
{
    switch (d_parentItem->type()) {
    case DirViewItem::Type_Root:
        return static_cast<DirViewItemRoot *>(d_parentItem)->d_childItems.indexOf(const_cast<DirViewItemDir*>(this));
        break;
    case DirViewItem::Type_Dir:
        return static_cast<DirViewItemDir *>(d_parentItem)->d_childItems.indexOf(const_cast<DirViewItemDir*>(this));
        break;
    default:
        break;
    }

    return 0;
}

DirViewItem * DirViewItemDir::parent() const
{
    return d_parentItem;
}

int DirViewItemDir::childCount() const
{
    return d_childItems.count();
}

DirViewItem * DirViewItemDir::createDirectory(const QString dirName)
{
    DirViewItemDir *dir = new DirViewItemDir(dirName, this);
    d_childItems.append(dir);

    return dir;
}

DirViewItem * DirViewItemDir::insertFile(const QString fileName)
{
    DirViewItemFile *file = new DirViewItemFile(fileName, this);
    d_childItems.append(file);

    return file;
}

DirViewItem * DirViewItemDir::child(int row) const
{
    return d_childItems.value(row);
}
