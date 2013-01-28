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

#ifndef CAPACITY_BAR_H
#define CAPACITY_BAR_H

#ifdef ENABLE_KDE_SUPPORT
#include <kcapacitybar.h>

class CapacityBar : public KCapacityBar
{
public:
    CapacityBar(QWidget *p)
        : KCapacityBar(KCapacityBar::DrawTextInline, p) {
    }

    void update(const QString &text, double value) {
        setText(text);
        setValue(value);
        setDrawTextMode(KCapacityBar::DrawTextInline);
    }
};

#else
#include <QProgressBar>
class CapacityBar : public QProgressBar
{
public:
    CapacityBar(QWidget *p)
        : QProgressBar(p) {
    }

    void update(const QString &text, double value) {
        setFormat(text);
        setRange(0, 100*100);
        setValue(value*100);
    }
};
#endif

#endif
