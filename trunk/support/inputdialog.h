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

#include <QtGui/QInputDialog>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KInputDialog>
#endif
#include "localize.h"

namespace InputDialog
{
    inline QString getText(const QString &caption, const QString &label, const QString &value=QString(), bool *ok=0, QWidget *parent=0) {
        #ifdef ENABLE_KDE_SUPPORT
        return KInputDialog::getText(caption, label, value, ok, parent=0);
        #else
        return QInputDialog::getText(parent, caption, label, QLineEdit::Normal, value, ok);
        #endif
    }

    inline int getInteger(const QString &caption, const QString &label, int value=0, int minValue=INT_MIN, int maxValue=INT_MAX,
                          int step=1, int base=10, bool *ok=0, QWidget *parent=0) {
        #ifdef ENABLE_KDE_SUPPORT
        return KInputDialog::getInteger(caption, label, value, minValue, maxValue, step, base, ok, parent);
        #else
        Q_UNUSED(base)
        return QInputDialog::getInt(parent, caption, label, value, minValue, maxValue, step, ok);
        #endif
    }

    inline QString getPassword(const QString &value=QString(), bool *ok=0, QWidget *parent=0) {
        return QInputDialog::getText(parent, i18n("Password"), i18n("Please enter password:"), QLineEdit::Password, value, ok);
    }
};

#endif // INPUT_DIALOG_H
