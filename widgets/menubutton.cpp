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

#include "menubutton.h"
#include "support/icon.h"
#include "icons.h"
#include <QAction>
#include <QMenu>
#include <QEvent>
#include <QApplication>
#include <QDesktopWidget>
#include <QProxyStyle>

#ifdef Q_OS_WIN
#include <windows.h>
#include <VersionHelpers.h>
#ifndef _WIN32_WINNT_WIN10
#define _WIN32_WINNT_WIN10 0x0A00
#endif
#endif

MenuButton::MenuButton(QWidget *parent)
    : ToolButton(parent)
{
    setPopupMode(QToolButton::InstantPopup);
    setIcon(Icons::self()->menuIcon);
    setToolTip(tr("Menu"));
    installEventFilter(this);
}

void MenuButton::controlState()
{
    if (!menu()) {
        return;
    }
    for (QAction *a: menu()->actions()) {
        if (a->isEnabled() && a->isVisible() && !a->isSeparator()) {
            setEnabled(true);
            return;
        }
    }
    setEnabled(false);
}

void MenuButton::setAlignedMenu(QMenu *m)
{
    QToolButton::setMenu(m);
    #ifdef Q_OS_WIN
    if (!IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN10), LOBYTE(_WIN32_WINNT_WIN10), 0)) {
        return;
    }
    #endif
    m->installEventFilter(this);
}

void MenuButton::addSeparator()
{
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    addAction(sep);
}

bool MenuButton::eventFilter(QObject *o, QEvent *e)
{
    if (QEvent::Show==e->type()) {
        if (qobject_cast<QMenu *>(o)) {
            static int shadowAdjust = -1;
            if (-1==shadowAdjust) {
                // Kvantum style adds its own shadows, which makes the menu appear in the wrong place.
                // However, Kvatum sill set a property on the style object to indicate the size of
                // the shadows. Use this to adjust positioning.
                QProxyStyle *proxy=qobject_cast<QProxyStyle *>(style());
                QStyle *check=proxy && proxy->baseStyle() ? proxy->baseStyle() : style();
                QList<int> shadows = check ? check->property("menu_shadow").value<QList<int> >() : QList<int>();
                shadowAdjust = 4 == shadows.length() ? shadows.at(2) : 0;
                if (shadowAdjust<0 || shadowAdjust>18) {
                    shadowAdjust = 0;
                }
            }
            QMenu *mnu=static_cast<QMenu *>(o);
            QPoint p=parentWidget()->mapToGlobal(pos());
            int newPos=isRightToLeft()
                    ? p.x()
                    : ((p.x()+width()+shadowAdjust)-mnu->width());

            if (newPos<0) {
                newPos=0;
            } else {
                QDesktopWidget *dw=QApplication::desktop();
                if (dw) {
                    QRect geo=dw->availableGeometry(this);
                    int maxWidth=geo.x()+geo.width();
                    if (maxWidth>0 && (newPos+mnu->width())>maxWidth) {
                        newPos=maxWidth-mnu->width();
                    }
                }
            }
            mnu->move(newPos, mnu->y());
        } else if (o==this) {
            setMinimumWidth(height());
            removeEventFilter(this);
        }
    }

    return ToolButton::eventFilter(o, e);
}

#include "moc_menubutton.cpp"
