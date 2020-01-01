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

#include "selectorlabel.h"
#include "support/osxstyle.h"
#include "support/utils.h"
#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>

SelectorLabel::SelectorLabel(QWidget *p)
    : QLabel(p)
    , current(0)
    , useArrow(false)
    , bold(true)
    , menu(nullptr)
{
    textColor=palette().text().color();
    setAttribute(Qt::WA_Hover, true);
    menu=new QMenu(this);
    #ifdef Q_OS_MAC
    // Mac text seems to be 2px too high. margin-top fixes this
    setStyleSheet(QLatin1String("QLabel { margin-top: 2px} "));
    #endif
}

static QString addMarkup(const QString &s, bool arrow, bool bold)
{
    return QLatin1String(bold ? "<b>" : "<p>")+s+(arrow ? QLatin1String("&nbsp;")+QChar(0x25BE) : QString())+QLatin1String(bold ? "</b>" : "</p>");
}

void SelectorLabel::addItem(const QString &text, const QString &data, const QString &tt)
{
    QAction *act=menu->addAction(text, this, SLOT(itemSelected()));
    act->setData(data);
    if (!tt.isEmpty()) {
        act->setToolTip(tt);
    }
    setText(addMarkup(text, useArrow, bold));
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
    switch (e->type()) {
    case QEvent::MouseButtonPress:
        if (menu && Qt::NoModifier==static_cast<QMouseEvent *>(e)->modifiers() && Qt::LeftButton==static_cast<QMouseEvent *>(e)->button()) {
            menu->exec(mapToGlobal(QPoint(0, 0)));
            update();
            if (this != QApplication::widgetAt(QCursor::pos())) {
                setStyleSheet(QString("QLabel{color:%1;}").arg(textColor.name()));
            }
        }
        break;
    case QEvent::Wheel:
        if (menu) {
            QList<QAction *> actions=menu->actions();
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
        }
        break;
    case QEvent::HoverEnter:
        if (isEnabled()) {
            #ifdef Q_OS_MAC
            setStyleSheet(QString("QLabel{color:%1;}").arg(OSXStyle::self()->viewPalette().highlight().color().name()));
            #else
            setStyleSheet(QLatin1String("QLabel{color:palette(highlight);}"));
            #endif
        }
        break;
    case QEvent::HoverLeave:
        if (isEnabled()) {
            setStyleSheet(QString("QLabel{color:%1;}").arg(textColor.name()));
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
    setText(addMarkup(Utils::strippedText(actions.at(current)->text()), useArrow, bold));
    emit activated(current);
}

QString SelectorLabel::itemData(int index) const
{
    QAction *act=action(index);
    return act ? act->data().toString() : QString();
}

QAction * SelectorLabel::action(int index) const
{
    if (!menu) {
        return nullptr;
    }

    QList<QAction *> actions=menu->actions();

    if (index>=actions.count()) {
        return nullptr;
    }
    return actions.at(index);
}

#include "moc_selectorlabel.cpp"
