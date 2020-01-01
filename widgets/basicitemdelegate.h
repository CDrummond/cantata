/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef BASIC_ITEM_DELEGATE_H
#define BASIC_ITEM_DELEGATE_H

#include <QStyledItemDelegate>

class BasicItemDelegate : public QStyledItemDelegate
{
public:
    static void drawLine(QPainter *p, const QRect &r, const QColor &color, bool fadeStart=true, bool fadeEnd=true, double alpha=0.1);
    BasicItemDelegate(QObject *p);
    ~BasicItemDelegate() override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    bool eventFilter(QObject *object, QEvent *event) override;
protected:
    bool trackMouse;
    bool underMouse;
};

#endif
