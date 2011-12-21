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
#ifndef DIRVIEWITEMROOT_H
#define DIRVIEWITEMROOT_H

#include <QStringList>
#include <QString>
#include <QList>
#include <QVariant>
#include "dirviewitem.h"

class DirViewItemRoot : public DirViewItem
{
public:
    DirViewItemRoot(const QString name=QString());
    ~DirViewItemRoot();

    DirViewItem * createDirectory(const QString dirName);
    DirViewItem * insertFile(const QString fileName);

    int childCount() const;
    DirViewItem * child(int row) const;

private:
    QList<DirViewItem *> d_childItems;

    friend class DirViewItemDir;
    friend class DirViewItemFile;
};

#endif
