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

#include "shortcuthandler.h"
#include <QMenu>
#include <QMenuBar>
#include <QEvent>
#include <QKeyEvent>

ShortcutHandler::ShortcutHandler(QObject *parent)
               : QObject(parent)
               , altDown(false)
{
}

ShortcutHandler::~ShortcutHandler()
{
}

bool ShortcutHandler::hasSeenAlt(const QWidget *widget) const
{
    if (widget && !widget->isEnabled()) {
        return false;
    }

    if (qobject_cast<const QMenu *>(widget)) {
        return openMenus.count() && openMenus.last()==widget;
    } else {
        return openMenus.isEmpty() && seenAlt.contains((QWidget *)(widget->window()));
    }
}

bool ShortcutHandler::showShortcut(const QWidget *widget) const
{
    return altDown && hasSeenAlt(widget);
}

void ShortcutHandler::widgetDestroyed(QObject *o)
{
    updated.remove(static_cast<QWidget *>(o));
    openMenus.removeAll(static_cast<QWidget *>(o));
}

void ShortcutHandler::updateWidget(QWidget *w)
{
    if (!updated.contains(w)) {
        updated.insert(w);
        w->update();
        connect(w, SIGNAL(destroyed(QObject *)), this, SLOT(widgetDestroyed(QObject *)));
    }
}

bool ShortcutHandler::eventFilter(QObject *o, QEvent *e)
{
    if (!o->isWidgetType()) {
        return QObject::eventFilter(o, e);
    }

    QWidget *widget = qobject_cast<QWidget*>(o);
    switch(e->type())  {
        case QEvent::KeyPress:
            if (Qt::Key_Alt==static_cast<QKeyEvent *>(e)->key()) {
                altDown = true;
                if (qobject_cast<QMenu *>(widget)) {
                    seenAlt.insert(widget);
                    updateWidget(widget);
                    if (widget->parentWidget() && widget->parentWidget()->window()) {
                        seenAlt.insert(widget->parentWidget()->window());
                    }
                } else {
                    widget = widget->window();
                    seenAlt.insert(widget);
                    QList<QWidget *> l = widget->findChildren<QWidget*>();
                    for (int pos=0 ; pos < l.size() ; ++pos)  {
                        QWidget *w = l.at(pos);
                        if (!(w->isWindow() || !w->isVisible())) { // || w->style()->styleHint(QStyle::SH_UnderlineShortcut, 0, w))) 
                            updateWidget(w);
                        }
                    }

                    QList<QMenuBar *> m = widget->findChildren<QMenuBar*>();
                    for (int i = 0; i < m.size(); ++i) {
                        updateWidget(m.at(i));
                    }
                }
            }
            break;
        case QEvent::WindowDeactivate:
        case QEvent::KeyRelease:
            if (QEvent::WindowDeactivate==e->type() || Qt::Key_Alt==static_cast<QKeyEvent*>(e)->key()) {
                altDown = false;
                QSet<QWidget *>::ConstIterator it(updated.constBegin());
                QSet<QWidget *>::ConstIterator end(updated.constEnd());
                                           
                for (; it!=end; ++it) {
                    (*it)->update();
                }
                if (!updated.contains(widget)) {
                    widget->update();
                }
                seenAlt.clear();
                updated.clear();
            }
            break;
        case QEvent::Show:
            if (qobject_cast<QMenu *>(widget)) {
                QWidget *prev=openMenus.count() ? openMenus.last() : 0L;
                openMenus.append(widget);
                if(altDown && prev) {
                    prev->update();
                }
                connect(widget, SIGNAL(destroyed(QObject *)), this, SLOT(widgetDestroyed(QObject *)));
            }
            break;
        case QEvent::Hide:
            if (qobject_cast<QMenu *>(widget)) {
                seenAlt.remove(widget);
                updated.remove(widget);
                openMenus.removeAll(widget);
                if (altDown) {
                    if (openMenus.count()) {
                        openMenus.last()->update();
                    } else if(widget->parentWidget() && widget->parentWidget()->window()) {
                        widget->parentWidget()->window()->update();
                    }
                }
            }
            break;
        case QEvent::Close:
            // Reset widget when closing
            seenAlt.remove(widget);
            updated.remove(widget);
            seenAlt.remove(widget->window());
            openMenus.removeAll(widget);
            if (altDown) {
                if(openMenus.count()) {
                    openMenus.last()->update();
                } else if(widget->parentWidget() && widget->parentWidget()->window()) {
                    widget->parentWidget()->window()->update();
                }
            }
            break;
        default:
            break;
    }
    return QObject::eventFilter(o, e);
}

