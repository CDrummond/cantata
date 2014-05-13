/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/localize.h"
#include <QAction>
#include <QMenu>
#include <QEvent>
#include <QApplication>
#include <QDesktopWidget>

MenuButton::MenuButton(QWidget *parent)
    : ToolButton(parent)
{
    setPopupMode(QToolButton::InstantPopup);
    setIcon(Icons::self()->menuIcon);
    setToolTip(i18n("Menu"));
}

void MenuButton::controlState()
{
    if (!menu()) {
        return;
    }
    foreach (QAction *a, menu()->actions()) {
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
    m->installEventFilter(this);
}

bool MenuButton::eventFilter(QObject *o, QEvent *e)
{
    if (QEvent::Show==e->type() && qobject_cast<QMenu *>(o)) {
        QMenu *mnu=static_cast<QMenu *>(o);
        QPoint p=parentWidget()->mapToGlobal(pos());
        int newPos=Qt::RightToLeft==layoutDirection()
                    ? p.x()
                    : ((p.x()+width())-mnu->width());

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
    }

    return ToolButton::eventFilter(o, e);
}
