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

#include "dirviewitemroot.h"
#include "dirviewitemdir.h"
#include "dirviewitemfile.h"

DirViewItemFile::DirViewItemFile(const QString name, DirViewItem *parent)
    : DirViewItem(name, DirViewItem::Type_File),
      d_parentItem(parent)
{
}

DirViewItemFile::~DirViewItemFile()
{
}

int DirViewItemFile::row() const
{
    switch (d_parentItem->type()) {
    case DirViewItem::Type_Root:
        return static_cast<DirViewItemRoot *>(d_parentItem)->d_childItems.indexOf(const_cast<DirViewItemFile*>(this));
        break;
    case DirViewItem::Type_Dir:
        return static_cast<DirViewItemDir *>(d_parentItem)->d_childItems.indexOf(const_cast<DirViewItemFile*>(this));
        break;
    default:
        break;
    }

    return 0;
}

DirViewItem * DirViewItemFile::parent() const
{
    return d_parentItem;
}

QString DirViewItemFile::fileName()
{
    QString filename = d_name;

    DirViewItem *current = this->parent();
    while (current->type() != DirViewItem::Type_Root) {
        filename.prepend("/");
        filename.prepend(current->name());
        current = current->parent();
    }
    return filename;
}
