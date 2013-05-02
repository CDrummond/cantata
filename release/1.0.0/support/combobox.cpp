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

#include "combobox.h"
#include "lineedit.h"
#include <QAbstractItemView>
#include <QStyle>
#include <QStyleOption>
#include <QDesktopWidget>
#include <QApplication>
#include "gtkstyle.h"

// Max number of items before we try to force a scrollbar in popup menu...
static int maxPopupItemCount=-1;

ComboBox::ComboBox(QWidget *p)
    #ifdef ENABLE_KDE_SUPPORT
    : KComboBox(p)
    #else
    : QComboBox(p)
    #endif
    , toggleState(false)
{
    if (-1==maxPopupItemCount) {
        if (QApplication::desktop()) {
            maxPopupItemCount=((QApplication::desktop()->height()/(QApplication::fontMetrics().height()*1.5))*0.75)+0.5;
        } else {
            maxPopupItemCount=32;
        }
    }
}

void ComboBox::setEditable(bool editable)
{
    QComboBox::setEditable(editable);
    if (editable) {
        LineEdit *edit = new LineEdit(this);
        setLineEdit(edit);
    }
}

void ComboBox::showPopup()
{
    if (GtkStyle::isActive() && count()>(maxPopupItemCount-2)) {
        toggleState=!isEditable();

        // Hacky, but if we set the combobox as editable - the style gives the
        // popup a scrollbar. This is more convenient if we have lots of items!
        if (toggleState) {
            setMaxVisibleItems(maxPopupItemCount);
            QComboBox::setEditable(true);
            lineEdit()->setReadOnly(true);
        }
    }
    QComboBox::showPopup();

    if (GtkStyle::isActive() && parentWidget() && view()->parentWidget() && count()>maxPopupItemCount) {
        // Also, if the size of the popup is more than required for 32 items, then
        // restrict its height...
        int maxHeight=maxPopupItemCount*view()->sizeHintForRow(0);
        QRect geo(view()->parentWidget()->geometry());
        QRect r(view()->parentWidget()->rect());
        if (geo.height()>maxHeight) {
            geo=QRect(geo.x(), geo.y()+(geo.height()-maxHeight), geo.width(), maxHeight);
            r=QRect(r.x(), r.y()+(r.height()-maxHeight), r.width(), maxHeight);
        }
        QPoint popupBot=view()->parentWidget()->mapToGlobal(r.bottomLeft());
        QPoint bot=mapToGlobal(rect().bottomLeft());

        if (popupBot.y()<bot.y()) {
            geo=QRect(geo.x(), geo.y()+(bot.y()-popupBot.y()), geo.width(), geo.height());
        } else {
            QPoint popupTop=view()->parentWidget()->mapToGlobal(r.topLeft());
            QPoint top=mapToGlobal(rect().topLeft());
            if (popupTop.y()>top.y()) {
                geo=QRect(geo.x(), geo.y()-(popupTop.y()-top.y()), geo.width(), geo.height());
            }
        }
        view()->parentWidget()->setGeometry(geo);

        #ifndef ENABLE_KDE_SUPPORT
        // Hide scrollers - these look ugly...
        foreach (QObject *c, view()->parentWidget()->children()) {
            if (0==qstrcmp("QComboBoxPrivateScroller", c->metaObject()->className())) {
                ((QWidget *)c)->setMaximumHeight(0);
            }
        }
        #endif
    }
}

void ComboBox::hidePopup()
{
    if (GtkStyle::isActive() && count()>(maxPopupItemCount-2) && toggleState) {
        QComboBox::setEditable(false);
    }
    QComboBox::hidePopup();
}
