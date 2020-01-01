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

#include "combobox.h"
#include "lineedit.h"
#include "utils.h"
#include <QAbstractItemView>
#include <QDesktopWidget>
#include <QApplication>
#include <QStyleOptionComboBox>

// Max number of items before we try to force a scrollbar in popup menu...
static int maxPopupItemCount=-1;

ComboBox::ComboBox(QWidget *p)
    : QComboBox(p)
    , toggleState(false)
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    if (-1==maxPopupItemCount) {
        if (QApplication::desktop()) {
            maxPopupItemCount=((QApplication::desktop()->height()/(QApplication::fontMetrics().height()*1.5))*0.75)+0.5;
        } else {
            maxPopupItemCount=32;
        }
    }
    #endif
    connect(this, SIGNAL(editTextChanged(QString)), this, SIGNAL(textChanged(QString)));
}

void ComboBox::setEditable(bool editable)
{
    QComboBox::setEditable(editable);
    if (editable) {
        LineEdit *edit = new LineEdit(this);
        setLineEdit(edit);
    }
}

#if !defined Q_OS_WIN && !defined Q_OS_MAC
void ComboBox::showPopup()
{
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    toggleState=false;
    bool hack=0!=style()->styleHint(QStyle::SH_ComboBox_Popup, &opt, this, nullptr);
    if (hack && count()>(maxPopupItemCount-2)) {
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

    if (hack && view()->parentWidget() && count()>maxPopupItemCount) {
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

        // Hide scrollers - these look ugly...
        for (QObject *c: view()->parentWidget()->children()) {
            if (0==qstrcmp("QComboBoxPrivateScroller", c->metaObject()->className())) {
                static_cast<QWidget *>(c)->setMaximumHeight(0);
            }
        }
    }
}

void ComboBox::hidePopup()
{
    if (toggleState) {
        QComboBox::setEditable(false);
        toggleState=false;
    }
    QComboBox::hidePopup();
}
#endif

#include "moc_combobox.cpp"
