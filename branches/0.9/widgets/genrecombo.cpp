/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

GenreCombo::GenreCombo(QWidget *p)
     : QComboBox(p)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
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
