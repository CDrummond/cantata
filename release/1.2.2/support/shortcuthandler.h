#ifndef __SHORTCUT_HANDLER_H__
#define __SHORTCUT_HANDLER_H__

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

#include <QObject>
#include <QSet>
#include <QList>

class QWidget;

class ShortcutHandler : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutHandler(QObject *parent = 0);
    virtual ~ShortcutHandler();

    bool hasSeenAlt(const QWidget *widget) const; 
    bool isAltDown() const { return altDown; }
    bool showShortcut(const QWidget *widget) const;

private Q_SLOTS:
    void widgetDestroyed(QObject *o);

protected:
    void updateWidget(QWidget *w);
    bool eventFilter(QObject *watched, QEvent *event);

private:
    bool altDown;
    QSet<QWidget *> seenAlt;
    QSet<QWidget *> updated;
    QList<QWidget *> openMenus;
};

#endif
