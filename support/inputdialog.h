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

#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include "dialog.h"
#include "combobox.h"

class LineEdit;

class InputDialog : public Dialog
{
    Q_OBJECT

public:
    InputDialog(const QString &caption, const QString &label, const QString &value, const QStringList &options, QLineEdit::EchoMode echo, QWidget *parent);
    InputDialog(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, QWidget *parent);

    static QString getText(const QString &caption, const QString &label, QLineEdit::EchoMode echoMode, const QString &value=QString(),
                           const QStringList &options=QStringList(), bool *ok=nullptr, QWidget *parent=nullptr);

    static int getInteger(const QString &caption, const QString &label, int value=0, int minValue=INT_MIN, int maxValue=INT_MAX,
                          int step=1, int base=10, bool *ok=nullptr, QWidget *parent=nullptr);

    static QString getText(const QString &caption, const QString &label, const QString &value=QString(),
                           const QStringList &options=QStringList(), bool *ok=nullptr, QWidget *parent=nullptr) {
        return getText(caption, label, QLineEdit::Normal, value, options, ok, parent);
    }

    static QString getText(const QString &caption, const QString &label, QLineEdit::EchoMode echoMode, const QString &value,
                           bool *ok, QWidget *parent) {
        return getText(caption, label, echoMode, value, QStringList(), ok, parent);
    }

    static QString getText(const QString &caption, const QString &label, const QString &value, bool *ok, QWidget *parent) {
        return getText(caption, label, QLineEdit::Normal, value, QStringList(), ok, parent);
    }

    static QString getPassword(const QString &value=QString(), bool *ok=nullptr, QWidget *parent=nullptr) {
        return getText(tr("Password"), tr("Please enter password:"), QLineEdit::Password, value, QStringList(), ok, parent);
    }

    void addExtraWidget(QWidget *w);
    QSpinBox *spinBox() {
        return spin;
    }
    QLineEdit *lineEdit() {
        return edit ? (QLineEdit *)edit : combo ? combo->lineEdit() : nullptr;
    }
    ComboBox *comboBox() {
        return combo;
    }

private:
    void init(int type, const QString &caption, const QString &labelText);

private Q_SLOTS:
    void enableOkButton();

private:
    QSpinBox *spin;
    LineEdit *edit;
    ComboBox *combo;
    QWidget *extra;
};

#endif // INPUT_DIALOG_H
