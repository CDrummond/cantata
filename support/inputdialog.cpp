/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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

InputDialog::InputDialog(const QString &caption, const QString &label, const QString &value, QLineEdit::EchoMode echo, QWidget *parent)
    : Dialog(parent)
    , spin(0)
{
    init(false, caption, label);
    edit->setText(value);
    edit->setEchoMode(echo);
    enableOkButton();
    connect(edit, SIGNAL(textChanged(QString)), this, SLOT(enableOkButton()));
}

InputDialog::InputDialog(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, QWidget *parent)
    : Dialog(parent)
    , edit(0)
{
    init(true, caption, label);
    spin->setRange(minValue, maxValue);
    spin->setValue(value);
    spin->setSingleStep(step);
}

void InputDialog::addExtraWidget(const QString &label, QWidget *w)
{
    w->setParent(mainWidget());
    qobject_cast<QFormLayout *>(mainWidget()->layout())->addRow(new QLabel(label, mainWidget()), w);
}

void InputDialog::init(bool intInput, const QString &caption, const QString &label)
{
    extra = 0;
    setButtons(Ok|Cancel);
    QWidget *wid=new QWidget(this);
    QFormLayout *layout=new QFormLayout(wid);

    if (intInput) {
        spin=new QSpinBox(wid);
        spin->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        setMinimumWidth(Utils::scaleForDpi(300));
    } else {
        edit=new LineEdit(wid);
        edit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        setMinimumWidth(Utils::scaleForDpi(350));
    }
    layout->addRow(new QLabel(label, wid), intInput ? static_cast<QWidget *>(spin) : static_cast<QWidget *>(edit));
    layout->setMargin(0);

    setCaption(caption);
    setMainWidget(wid);
    setButtons(Ok|Cancel);
}

void InputDialog::enableOkButton()
{
    enableButton(Ok, 0==edit || !edit->text().trimmed().isEmpty());
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

QString InputDialog::getText(const QString &caption, const QString &label, QLineEdit::EchoMode echoMode, const QString &value, bool *ok, QWidget *parent)
{
    InputDialog dlg(caption, label, value, echoMode, parent);
    if (QDialog::Accepted==dlg.exec()) {
        if (ok) {
            *ok=true;
        }
        return dlg.edit->text();
    } else {
        if (ok) {
            *ok=false;
        }
        return value;
    }
}
