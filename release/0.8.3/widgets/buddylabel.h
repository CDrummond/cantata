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

#ifndef BUDDYLABEL_H
#define BUDDYLABEL_H

#include <QtGui/QLabel>
#include <QtGui/QCheckBox>
#include <QtGui/QComboBox>

class BuddyLabel : public QLabel
{
public:
    BuddyLabel(QWidget *p)
        : QLabel(p) {
    }

    virtual ~BuddyLabel() {
    }

protected:
    void mouseReleaseEvent(QMouseEvent *) {
        if (buddy() && buddy()->isEnabled()) {
            buddy()->setFocus();

            QCheckBox *cb=qobject_cast<QCheckBox*>(buddy());
            if (cb) {
                cb->setChecked(!cb->isChecked());
            } else {
                QComboBox *combo=qobject_cast<QComboBox*>(buddy());
                if (combo) {
                    combo->showPopup();
                }
            }
        }
    }
};

#endif
