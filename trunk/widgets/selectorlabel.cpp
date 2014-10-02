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

#include "selectorlabel.h"
#include "support/osxstyle.h"
#include "support/utils.h"
#include <QAction>
#include <QEvent>
#include <QMouseEvent>

SelectorLabel::SelectorLabel(QWidget *p)
    : QLabel(p)
    , current(0)
    , useArrow(false)
    , menu(0)
{
    setAttribute(Qt::WA_Hover, true);
    menu=new QMenu(this);
    #ifdef Q_OS_MAC
    setStyleSheet(QString("QLabel:hover {color:%1;}").arg(OSXStyle::self()->viewPalette().highlight().color().name()));
    #else
    setStyleSheet(QLatin1String("QLabel:hover {color:palette(highlight);}"));
    #endif
    //setMargin(Utils::scaleForDpi(2));
}

static QString addMarkup(const QString &s, bool arrow)
{
    return QLatin1String("<b>")+s+(arrow ? QLatin1String("  ")+QChar(0x25BE) : QLatin1String("&nbsp;"))+QLatin1String("</b>");;
}

void SelectorLabel::addItem(const QString &text, const QString &data)
{
    menu->addAction(text, this, SLOT(itemSelected()))->setData(data);
    setText(addMarkup(text, useArrow));
    current=menu->actions().count();
}

void SelectorLabel::itemSelected()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        setCurrentIndex(menu->actions().indexOf(act));
    }
}

bool SelectorLabel::event(QEvent *e)
{
    if (!menu) {
        return QLabel::event(e);
    }
    QList<QAction *> actions=menu->actions();

    switch (e->type()) {
    case QEvent::MouseButtonPress:
        if (Qt::NoModifier==static_cast<QMouseEvent *>(e)->modifiers() && Qt::LeftButton==static_cast<QMouseEvent *>(e)->button()) {
            menu->exec(mapToGlobal(static_cast<QMouseEvent *>(e)->pos()));
        }
        break;
    case QEvent::Wheel: {
        int numDegrees = static_cast<QWheelEvent *>(e)->delta() / 8;
        int numSteps = numDegrees / 15;
        int newIndex = current;
        if (numSteps > 0) {
            for (int i = 0; i < numSteps; ++i) {
                newIndex++;
                if (newIndex>=actions.count()) {
                    newIndex=0;
                }
            }
        } else {
            for (int i = 0; i > numSteps; --i) {
                newIndex--;
                if (newIndex<0) {
                    newIndex=actions.count()-1;
                }
            }
        }
        setCurrentIndex(newIndex);
        break;
    }
    default:
        break;
    }
    return QLabel::event(e);
}

void SelectorLabel::setCurrentIndex(int v)
{
    if (!menu || v<0 || v==current) {
        return;
    }

    QList<QAction *> actions=menu->actions();

    if (v>=actions.count()) {
        return;
    }
    current=v;
    setText(addMarkup(Utils::strippedText(actions.at(current)->text()), useArrow));
    emit activated(current);
}

QString SelectorLabel::itemData(int index) const
{
    if (!menu) {
        return QString();
    }

    QList<QAction *> actions=menu->actions();

    if (index>=actions.count()) {
        return QString();
    }
    return actions.at(index)->data().toString();
}
