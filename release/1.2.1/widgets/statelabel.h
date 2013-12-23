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

#ifndef STATELABEL_H
#define STATELABEL_H

#include "buddylabel.h"

class StateLabel : public BuddyLabel
{
public:
    StateLabel(QWidget *parent=0)
        : BuddyLabel(parent)
        , on(false) {
    }

    void setOn(bool o) {
        if (o!=on) {
            setStyleSheet(o ? QLatin1String("QLabel { color : red; }") : QString());
            on=o;
        }
    }

private:
    bool on;
};

#endif
