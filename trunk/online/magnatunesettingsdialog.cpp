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

#include "magnatunesettingsdialog.h"
#include "localize.h"
#include <QtGui/QFormLayout>

MagnatuneSettingsDialog::MagnatuneSettingsDialog(QWidget *parent)
    : Dialog(parent)
{
    setButtons(Ok|Cancel);
    setCaption(i18n("Magnatune Settings"));
    QWidget *mw=new QWidget(this);
    QFormLayout *layout=new QFormLayout(mw);
    member=new QComboBox(mw);
    member->insertItem(0, i18n("None"));
    member->insertItem(1, i18n("Streaming"));
    user=new LineEdit(mw);
    pass=new LineEdit(mw);
    pass->setEchoMode(QLineEdit::Password);
    layout->addRow(i18n("Membership:"), member);
    layout->addRow(i18n("Username:"), user);
    layout->addRow(i18n("Password:"), pass);
    setMainWidget(mw);
    connect(member, SIGNAL(currentIndexChanged(int)), SLOT(membershipChanged(int)));
}

bool MagnatuneSettingsDialog::run(bool m, const QString &u, const QString &p)
{
    member->setCurrentIndex(m ? 1 : 0);
    user->setText(u);
    user->setEnabled(m);
    pass->setText(p);
    pass->setEnabled(m);
    return QDialog::Accepted==Dialog::exec();
}

void MagnatuneSettingsDialog::membershipChanged(int i)
{
    user->setEnabled(0!=i);
    pass->setEnabled(0!=i);
}
