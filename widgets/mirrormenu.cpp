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

#include "mirrormenu.h"
#include <QAction>

MirrorMenu::MirrorMenu(QWidget *p)
    : QMenu(p)
{
}

void MirrorMenu::addAction(QAction *act)
{
    QMenu::addAction(act);
    updateMenus();
}

QAction * MirrorMenu::addAction(const QString &text)
{
    QAction *act=QMenu::addAction(text);
    updateMenus();
    return act;
}

QAction * MirrorMenu::addAction(const QIcon &icon, const QString &text)
{
    QAction *act=QMenu::addAction(icon, text);
    updateMenus();
    return act;
}

QAction * MirrorMenu::addAction(const QString &text, const QObject *receiver, const char *member, const QKeySequence &shortcut)
{
    QAction *act=QMenu::addAction(text, receiver, member, shortcut);
    updateMenus();
    return act;
}

QAction * MirrorMenu::addAction(const QIcon &icon, const QString &text, const QObject *receiver, const char *member, const QKeySequence &shortcut)
{
    QAction *act=QMenu::addAction(icon, text, receiver, member, shortcut);
    updateMenus();
    return act;
}

void MirrorMenu::removeAction(QAction *act)
{
    QMenu::removeAction(act);
    updateMenus();
}

void MirrorMenu::clear()
{
    QMenu::clear();
    updateMenus();
}

QMenu * MirrorMenu::duplicate(QWidget *p)
{
    QMenu *menu=new QMenu(p);
    menus.append(menu);
    updateMenu(menu);
    connect(menu, SIGNAL(destroyed(QObject*)), this, SLOT(menuDestroyed(QObject*)));
    return menu;
}

void MirrorMenu::updateMenus()
{
    for (QMenu *m: menus) {
        updateMenu(m);
    }
}

void MirrorMenu::updateMenu(QMenu *menu)
{
    menu->clear();
    menu->addActions(actions());
}

void MirrorMenu::menuDestroyed(QObject *obj)
{
    if (qobject_cast<QMenu *>(obj)) {
        menus.removeAll(static_cast<QMenu *>(obj));
    }
}

#include "moc_mirrormenu.cpp"
