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

#include "genrecombo.h"
#include "localize.h"
#include <QLineEdit>
#include <QAbstractItemView>
#include <QStyle>
#include <QStyleOption>

// Max number of items before we try to force a scrollbar in popup menu...
static const int constPopupItemCount=32;

GenreCombo::GenreCombo(QWidget *p)
     : QComboBox(p)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    update(QSet<QString>());
    setEditable(false);
}

void GenreCombo::showPopup()
{
    QStyleOptionComboBox opt;
    opt.init(this);
    bool usingPopup=style()->styleHint(QStyle::SH_ComboBox_Popup, &opt);

    // Hacky, but if we set the combobox as editable - the style gives the
    // popup a scrollbar. This is more convenient if we have lots of items!
    if (usingPopup && count()>constPopupItemCount) {
        setMaxVisibleItems(constPopupItemCount);
        setEditable(true);
        lineEdit()->setReadOnly(true);
    }
    QComboBox::showPopup();

//    // Also, if the size of the popup is more than required for 32 items, then
//    // restrict its height...
//    if (usingPopup && parentWidget() && view()->parentWidget()) {
//        int maxHeight=constPopupItemCount*view()->sizeHintForRow(0);
//        QRect geo(view()->parentWidget()->geometry());
//        if (geo.height()>maxHeight) {
//            view()->parentWidget()->setGeometry(QRect(geo.x(), geo.y()+(geo.height()-maxHeight), geo.width(), maxHeight));
//        }

//        // Hide scrollers - these look ugly...
//        foreach (QObject *c, view()->parentWidget()->children()) {
//            if (0==qstrcmp("QComboBoxPrivateScroller", c->metaObject()->className())) {
//                ((QWidget *)c)->setMaximumHeight(0);
//            }
//        }
//    }
}

void GenreCombo::hidePopup()
{
    // Unset editable...
    if (count()>constPopupItemCount) {
        QStyleOptionComboBox opt;
        opt.init(this);
        if (style()->styleHint(QStyle::SH_ComboBox_Popup, &opt)) {
            setEditable(false);
        }
    }
    QComboBox::hidePopup();
}

void GenreCombo::update(const QSet<QString> &g)
{
    if (count() && g==genres) {
        return;
    }

    genres=g;
    QStringList entries=g.toList();
    qSort(entries);
    entries.prepend(i18n("All Genres"));

    if (count()==entries.count()) {
        bool noChange=true;
        for (int i=0; i<count(); ++i) {
            if (itemText(i)!=entries.at(i)) {
                noChange=false;
                break;
            }
        }
        if (noChange) {
            return;
        }
    }

    QString currentFilter = currentIndex() ? currentText() : QString();

    clear();
    addItems(entries);
    if (0==genres.count()) {
        setCurrentIndex(0);
    } else {
        if (!currentFilter.isEmpty()) {
            bool found=false;
            for (int i=1; i<count() && !found; ++i) {
                if (itemText(i) == currentFilter) {
                    setCurrentIndex(i);
                    found=true;
                }
            }
            if (!found) {
                setCurrentIndex(0);
            }
        }
    }
}
