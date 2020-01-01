/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef COMPLETION_COMBO_H
#define COMPLETION_COMBO_H

#include "support/combobox.h"
#include "support/lineedit.h"
#include <QAbstractItemView>

class CompletionCombo : public ComboBox
{
public:
    CompletionCombo(QWidget *p)
        : ComboBox(p) {
        setEditable(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
        view()->setTextElideMode(Qt::ElideRight);
    }

    void setText(const QString &text) {
        if (lineEdit()) qobject_cast<QLineEdit*>(lineEdit())->setText(text);
    }

    QString text() const {
        return lineEdit() ? qobject_cast<QLineEdit*>(lineEdit())->text() : QString();
    }

    void setPlaceholderText(const QString &text) {
        if (lineEdit()) qobject_cast<QLineEdit*>(lineEdit())->setPlaceholderText(text);
    }
};

#endif
