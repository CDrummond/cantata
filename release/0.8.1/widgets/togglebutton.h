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

#ifndef TOGGLE_BUTTON_H
#define TOGGLE_BUTTON_H

#include "config.h"

#ifdef ENABLE_KDE_SUPPORT
#include <QtGui/QToolButton>
typedef QToolButton ToggleButton;

#else // ENABLE_KDE_SUPPORT

#include <QtGui/QToolButton>
#ifdef CANTATA_ANDROID
class ToggleButton : public QToolButton
{
public:
    ToggleButton(QWidget *parent);
    virtual ~ToggleButton();
    void paintEvent(QPaintEvent *e);
};
#else // CANTATA_ANDROID
typedef QToolButton ToggleButton;
#endif // CANTATA_ANDROID

#endif // ENABLE_KDE_SUPPORT

#endif
