/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtGui/QStyledItemDelegate>

class QAction;

class ActionItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static void setup();

    ActionItemDelegate(QObject *p, QAction *a1, QAction *a2, QAction *t, int actionLevel)
        : QStyledItemDelegate(p)
        , act1(a1)
        , act2(a2)
        , toggle(t)
        , actLevel(actionLevel) {
    }

    virtual ~ActionItemDelegate() {
    }

    static int constBorder;
    static int constActionBorder;
    static int constActionIconSize;

    static QRect calcActionRect(bool rtl, bool iconMode, const QRect &rect);
    static void adjustActionRect(bool rtl, bool iconMode, QRect &rect);
    static bool hasActions(const QModelIndex &index, int actLevel);

    void drawIcons(QPainter *painter, const QRect &r, bool mouseOver, bool rtl, bool iconMode, const QModelIndex &index) const;

public Q_SLOTS:
    bool helpEvent(QHelpEvent *e, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index);

private:
    QAction * getAction(QAbstractItemView *view, const QModelIndex &index);

public:
    QAction *act1;
    QAction *act2;
    QAction *toggle;
    int actLevel;
};

#endif

