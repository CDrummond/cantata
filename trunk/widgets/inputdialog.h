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

#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KInputDialog>
#define InputDialog KInputDialog
#else
#include <QtGui/QInputDialog>

namespace InputDialog
{
    inline QString getText(const QString &caption, const QString &label, const QString &value=QString(), bool *ok=0, QWidget *parent=0) {
        return QInputDialog::getText(parent, caption, label, QLineEdit::Normal, value, ok);
    }

    inline int getInteger(const QString &caption, const QString &label, int value=0, int minValue=INT_MIN, int maxValue=INT_MAX,
                          int step=1, int base=10, bool *ok=0, QWidget *parent=0) {
        Q_UNUSED(base)
        return QInputDialog::getInt(parent, caption, label, value, minValue, maxValue, step, ok);
    }
};
#endif

#endif // URLLABEL_H
