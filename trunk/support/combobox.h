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

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>

#ifdef Q_OS_WIN
typedef QComboBox ComboBox;
#else
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KComboBox>
#endif
class ComboBox
    #ifdef ENABLE_KDE_SUPPORT
    : public KComboBox
    #else
    : public QComboBox
    #endif
{
public:
    ComboBox(QWidget *p);
    virtual ~ComboBox() { }

    void setEditable(bool editable);

private:
    void showPopup();
    void hidePopup();

private:
    bool toggleState;
};
#endif

#endif
