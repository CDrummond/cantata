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

#include "inputdialog.h"
#include "gtkstyle.h"
#include "spinbox.h"
#include "dialog.h"
#include <QLabel>
#include <QFormLayout>

class IntInputDialog : public Dialog
{
public:
    IntInputDialog(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, QWidget *parent)
        : Dialog(parent) {
        setButtons(Ok|Cancel);
        QWidget *wid=new QWidget(this);
        QFormLayout *layout=new QFormLayout(wid);

        spin=new SpinBox(wid);
        spin->setRange(minValue, maxValue);
        spin->setValue(value);
        spin->setSingleStep(step);
        spin->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        layout->setWidget(0, QFormLayout::LabelRole, new QLabel(label, wid));
        layout->setWidget(0, QFormLayout::FieldRole, spin);
        layout->setMargin(0);

        setCaption(caption);
        setMainWidget(wid);
        setButtons(Ok|Cancel);
    }

SpinBox *spin;
};

int InputDialog::getInteger(const QString &caption, const QString &label, int value, int minValue, int maxValue, int step, int base, bool *ok, QWidget *parent) {

    if (GtkStyle::mimicWidgets()) {
        IntInputDialog dlg(caption, label, value, minValue, maxValue, step, parent);
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
    } else {
        #ifdef ENABLE_KDE_SUPPORT
        return KInputDialog::getInteger(caption, label, value, minValue, maxValue, step, base, ok, parent);
        #else
        Q_UNUSED(base)
        return QInputDialog::getInt(parent, caption, label, value, minValue, maxValue, step, ok);
        #endif
    }
}
