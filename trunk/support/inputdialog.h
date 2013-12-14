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

#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#include <QLineEdit>
#include "localize.h"
#include "dialog.h"

class SpinBox;
class LineEdit;

class InputDialog : public Dialog
{
    Q_OBJECT

public:
    InputDialog(const QString &caption, const QString &label, const QString &value, QLineEdit::EchoMode echo, QWidget *parent);
    InputDialog(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, QWidget *parent);

    static QString getText(const QString &caption, const QString &label, QLineEdit::EchoMode echoMode, const QString &value=QString(),
                           bool *ok=0, QWidget *parent=0);
    static int getInteger(const QString &caption, const QString &label, int value=0, int minValue=INT_MIN, int maxValue=INT_MAX,
                          int step=1, int base=10, bool *ok=0, QWidget *parent=0);

    static QString getText(const QString &caption, const QString &label, const QString &value=QString(), bool *ok=0, QWidget *parent=0) {
        return getText(caption, label, QLineEdit::Normal, value, ok, parent);
    }
    static QString getPassword(const QString &value=QString(), bool *ok=0, QWidget *parent=0) {
        return getText(i18n("Password"), i18n("Please enter password:"), QLineEdit::Password, value, ok, parent);
    }

private:
    void init(bool intInput, const QString &caption, const QString &label);

private Q_SLOTS:
    void enableOkButton();

private:
    SpinBox *spin;
    LineEdit *edit;
};

#endif // INPUT_DIALOG_H
