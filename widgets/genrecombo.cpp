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

#include "genrecombo.h"
#include "toolbutton.h"
#include "support/actioncollection.h"
#include "support/action.h"
#include <QEvent>
#include <algorithm>

static Action *action=nullptr;

GenreCombo::GenreCombo(QWidget *p)
     : ComboBox(p)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    update(QSet<QString>());
    setEditable(false);
    setFocusPolicy(Qt::NoFocus);
    if (!action) {
        action=ActionCollection::get()->createAction("genrefilter", tr("Filter On Genre"), nullptr);
        action->setShortcut(Qt::ControlModifier+Qt::Key_G);
    }
    addAction(action);
    connect(action, SIGNAL(triggered()), SLOT(showEntries()));
}

void GenreCombo::update(const QSet<QString> &g)
{
    if (count() && g==genres) {
        return;
    }

    QSet<QString> mg=g;
    mg.remove(QString());
    if (mg.count()!=g.count() && count() && mg==genres) {
        return;
    }

    genres=mg;
    QStringList entries=g.toList();
    std::sort(entries.begin(), entries.end());
    entries.prepend(tr("All Genres"));

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
    setEnabled(count()>1);
    // If we are 'hidden' then we need to ingore mouse events - so that these get passed to parent widget.
    // The Oxygen's window drag still functions...
    setAttribute(Qt::WA_TransparentForMouseEvents, count()<2);
}

void GenreCombo::showEntries()
{
    if (isVisible()) {
        showPopup();
    }
}

void GenreCombo::paintEvent(QPaintEvent *e)
{
    if (count()>1) {
        ComboBox::paintEvent(e);
    } else {
        QWidget::paintEvent(e);
    }
}

bool GenreCombo::event(QEvent *event)
{
    if (QEvent::ToolTip==event->type() && toolTip()!=action->toolTip()) {
        setToolTip(action->toolTip());
    }
    return ComboBox::event(event);
}

#include "moc_genrecombo.cpp"
