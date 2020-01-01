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

#ifndef EMPTYSPINBOX_H
#define EMPTYSPINBOX_H

#include <QSpinBox>
#include <QFontMetrics>

class EmptySpinBox : public QSpinBox
{
public:
    EmptySpinBox(QWidget *parent)
        : QSpinBox(parent)
        {
        setKeyboardTracking(true);
        setMaximum(3000);
    }

    QSize sizeHint() const override {
        return QSpinBox::sizeHint()+QSize(fontMetrics().height()/2, 0);
    }

protected:
    QValidator::State validate(QString &input, int &pos) const override {
        return input.isEmpty() ? QValidator::Acceptable : QSpinBox::validate(input, pos);
    }

    int valueFromText(const QString &text) const override {
        return text.isEmpty() ? minimum() : QSpinBox::valueFromText(text);
    }

    QString textFromValue(int val) const override {
        return val==minimum() ? QString() : QSpinBox::textFromValue(val);
    }
};

#endif
