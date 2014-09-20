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

#include "osxstyle.h"
#include "globalstatic.h"
#include "localize.h"
#include <QApplication>
#include <QStyle>
#include <QTreeWidget>
#include <QPainter>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>
#include <QMainWindow>

GLOBAL_STATIC(OSXStyle, instance)

OSXStyle::OSXStyle()
    : view(0)
    , windowMenu(0)
{
}

const QPalette & OSXStyle::viewPalette()
{
    return viewWidget()->palette();
}

void OSXStyle::drawSelection(QStyleOptionViewItemV4 opt, QPainter *painter, double opacity)
{
    opt.palette=viewPalette();
    if (opacity<0.999) {
        QColor col(opt.palette.highlight().color());
        col.setAlphaF(opacity);
        opt.palette.setColor(opt.palette.currentColorGroup(), QPalette::Highlight, col);
    }
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, viewWidget());
}

QColor OSXStyle::monoIconColor()
{
    return QColor(96, 96, 96);
}

void OSXStyle::initWindowMenu(QMainWindow *mw)
{
    if (!windowMenu && mw) {
        windowMenu=new QMenu(i18n("&Window"), mw);
        addWindow(mw);
        mw->menuBar()->addMenu(windowMenu);
    }
}

void OSXStyle::addWindow(QWidget *w)
{
    if (w && windowMenu) {
        QAction *action=windowMenu->addAction(w->windowTitle());
        connect(action, SIGNAL(triggered()), this, SLOT(showWindow()));
        connect(w, SIGNAL(windowTitleChanged(QString)), this, SLOT(windowTitleChanged()));
        actions.insert(w, action);
    }
}

void OSXStyle::removeWindow(QWidget *w)
{
    if (w && windowMenu) {
        if (actions.contains(w)) {
            QAction *act=actions.take(w);
            windowMenu->removeAction(act);
            disconnect(act, SIGNAL(triggered()), this, SLOT(showWindow()));
            disconnect(w, SIGNAL(windowTitleChanged(QString)), this, SLOT(windowTitleChanged()));
            act->deleteLater();
        }
    }
}

void OSXStyle::showWindow()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (!act) {
        return;
    }

    QMap<QWidget *, QAction *>::Iterator it=actions.begin();
    QMap<QWidget *, QAction *>::Iterator end=actions.end();

    for (; it!=end; ++it) {
        if (it.value()==act) {
            QWidget *w=it.key();
            w->showNormal();
            w->activateWindow();
            w->raise();
            return;
        }
    }
}

void OSXStyle::windowTitleChanged()
{
    QWidget *w=qobject_cast<QWidget *>(sender());

    if (!w) {
        return;
    }
    if (actions.contains(w)) {
        actions[w]->setText(w->windowTitle());
    }
}

QTreeWidget * OSXStyle::viewWidget()
{
    if (!view) {
        view=new QTreeWidget();
        view->ensurePolished();
    }
    return view;
}
