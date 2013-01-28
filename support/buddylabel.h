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

#ifndef BUDDYLABEL_H
#define BUDDYLABEL_H

#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include "spinbox.h"
#ifndef ENABLE_KDE_SUPPORT
#include "dirrequester.h"
#endif

class BuddyLabel : public QLabel
{
public:
    BuddyLabel(const QString &text, QWidget *p, QWidget *b=0)
        : QLabel(text, p) {
        if (b) {
            setBuddy(b);
        }
    }

    BuddyLabel(QWidget *p, QWidget *b=0)
        : QLabel(p) {
        if (b) {
            setBuddy(b);
        }
    }

    virtual ~BuddyLabel() {
    }

protected:
    void mouseReleaseEvent(QMouseEvent *) {
        if (buddy() && buddy()->isEnabled()) {
            #ifndef ENABLE_KDE_SUPPORT
            DirRequester *d=qobject_cast<DirRequester*>(buddy());
            if (d) {
                d->setFocus();
                return;
            }
            #endif
            #ifndef Q_WS_WIN
            SpinBox *sb=qobject_cast<SpinBox*>(buddy());
            if (sb) {
                sb->setFocus();
                return;
            }
            #endif

            buddy()->setFocus();

            QCheckBox *cb=qobject_cast<QCheckBox*>(buddy());
            if (cb) {
                cb->setChecked(!cb->isChecked());
                return;
            }
            QComboBox *combo=qobject_cast<QComboBox*>(buddy());
            if (combo) {
                combo->showPopup();
                return;
            }
        }
    }
};

#endif
