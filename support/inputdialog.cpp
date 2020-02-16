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

#include "inputdialog.h"
#include "gtkstyle.h"
#include "lineedit.h"
#include "dialog.h"
#include "utils.h"
#include <QLabel>
#include <QFormLayout>
#include <QSpinBox>
#include <algorithm>

enum InputType {
    Int,
    Text,
    Combo
};

InputDialog::InputDialog(const QString &caption, const QString &label, const QString &value, const QStringList &options, QLineEdit::EchoMode echo, QWidget *parent)
    : Dialog(parent)
    , spin(nullptr)
    , edit(nullptr)
    , combo(nullptr)
{
    init(options.isEmpty() ? Text : Combo, caption, label);
    if (options.isEmpty()) {
        edit->setText(value);
        edit->setEchoMode(echo);
        connect(edit, SIGNAL(textChanged(QString)), this, SLOT(enableOkButton()));
    } else {
        QStringList items = options;
        if (!value.isEmpty() && -1==items.indexOf(value)) {
            items.append(value);
        }
        std::sort(items.begin(), items.end());
        combo->addItems(items);
        combo->setCurrentText(value.isEmpty() ? QString() : value);
        connect(combo, SIGNAL(editTextChanged(QString)), this, SLOT(enableOkButton()));
    }
    enableOkButton();
}

InputDialog::InputDialog(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, QWidget *parent)
    : Dialog(parent)
    , spin(nullptr)
    , edit(nullptr)
    , combo(nullptr)
{
    init(Int, caption, label);
    spin->setRange(minValue, maxValue);
    spin->setValue(value);
    spin->setSingleStep(step);
}

void InputDialog::addExtraWidget(QWidget *w)
{
    w->setParent(mainWidget());
    QVBoxLayout *layout=qobject_cast<QVBoxLayout *>(mainWidget()->layout());
    layout->insertWidget(layout->count()-1, w);
}

void InputDialog::init(int type, const QString &caption, const QString &labelText)
{
    extra = nullptr;
    setButtons(Ok|Cancel);
    QWidget *wid=new QWidget(this);
    QVBoxLayout *layout=new QVBoxLayout(wid);
    layout->addWidget(new QLabel(labelText, wid));

    switch (type) {
    case Int:
        spin=new QSpinBox(wid);
        spin->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setMinimumWidth(Utils::scaleForDpi(300));
        layout->addWidget(spin);
        break;
    case Text:
        edit=new LineEdit(wid);
        edit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        edit->setMinimumWidth(Utils::scaleForDpi(350));
        layout->addWidget(edit);
        break;
    case Combo:
        combo=new ComboBox(wid);
        combo->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        combo->setEditable(true);
        combo->setMinimumWidth(Utils::scaleForDpi(350));
        layout->addWidget(combo);
        break;
    }
    layout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    layout->setMargin(0);
    setCaption(caption);
    setMainWidget(wid);
    setButtons(Ok|Cancel);
}

void InputDialog::enableOkButton()
{
    enableButton(Ok, (nullptr==edit || !edit->text().trimmed().isEmpty()) && (nullptr==combo || !combo->currentText().trimmed().isEmpty()));
}

int InputDialog::getInteger(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, int base, bool *ok, QWidget *parent)
{
    Q_UNUSED(base)
    InputDialog dlg(caption, label, value, minValue, maxValue, step, parent);
    if (QDialog::Accepted==dlg.exec()) {
        if (ok) {
            *ok=true;
        }
        return dlg.spin->value();
    } else {
        if (ok) {
            *ok=false;
        }
        return value;
    }
}

QString InputDialog::getText(const QString &caption, const QString &label, QLineEdit::EchoMode echoMode, const QString &value, const QStringList &options, bool *ok, QWidget *parent)
{
    InputDialog dlg(caption, label, value, options, echoMode, parent);
    if (QDialog::Accepted==dlg.exec()) {
        if (ok) {
            *ok=true;
        }
        return dlg.lineEdit()->text();
    } else {
        if (ok) {
            *ok=false;
        }
        return value;
    }
}

#include "moc_inputdialog.cpp"
