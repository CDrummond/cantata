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

DirViewItemRoot::DirViewItemRoot(const QString name)
    : DirViewItem(name, DirViewItem::Type_Root)
{
}

DirViewItemRoot::~DirViewItemRoot()
{
    qDeleteAll(d_childItems);
}

int DirViewItemRoot::childCount() const
{
    return d_childItems.count();
}

DirViewItem * DirViewItemRoot::createDirectory(const QString dirName)
{
    DirViewItemDir *dir = new DirViewItemDir(dirName, this);
    d_childItems.append(dir);

    return dir;
}

DirViewItem * DirViewItemRoot::insertFile(const QString fileName)
{
    DirViewItemFile *file = new DirViewItemFile(fileName, this);
    d_childItems.append(file);

    return file;
}

DirViewItem * DirViewItemRoot::child(int row) const
{
    return d_childItems.value(row);
}
