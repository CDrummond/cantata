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

    DirViewItem(const QString name, Type type);
    virtual ~DirViewItem();

    virtual int row() const {
        return 0;
    }
    virtual DirViewItem * parent() const {
        return NULL;
    }
    virtual int childCount() const {
        return 0;
    }
    virtual DirViewItem * child(int /*row*/) const {
        return NULL;
    }
    virtual QString fileName() {
        return QString();
    }

    int columnCount() const {
        return 1;
    }
    QVariant data(int) const {
        return d_name;
    }
    QString name() {
        return d_name;
    }
    DirViewItem::Type type() const {
        return d_type;
    }

protected:
    QString d_name;
    Type d_type;
};

#endif
