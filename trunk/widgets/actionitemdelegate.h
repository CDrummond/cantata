/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef ACTIONITEMDELEGATE_H
#define ACTIONITEMDELEGATE_H

#include <QStyledItemDelegate>

class QAction;

class ActionItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    enum ActionPos {
        AP_HBottom,
        AP_VTop,
        AP_HMiddle
    };

    static void setup();

    ActionItemDelegate(QObject *p);
    virtual ~ActionItemDelegate() { }

    static int constBorder;
    static int constActionBorder;
    static int constActionIconSize;

    void drawIcons(QPainter *painter, const QRect &r, bool mouseOver, bool rtl, ActionPos actionPos, const QModelIndex &index) const;
    void setUnderMouse(bool um) { underMouse=um; }

public Q_SLOTS:
    bool helpEvent(QHelpEvent *e, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index);

public:
    QAction * getAction(const QModelIndex &index) const;

private:
    QRect calcActionRect(bool rtl, ActionPos actionPos, const QRect &rect) const;
    static void adjustActionRect(bool rtl, ActionPos actionPos, QRect &rect);

protected:
    bool underMouse;
};

#endif

