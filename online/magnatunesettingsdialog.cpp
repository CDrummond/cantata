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
#include "magnatuneservice.h"
#include "localize.h"
#include "buddylabel.h"
#include <QFormLayout>

MagnatuneSettingsDialog::MagnatuneSettingsDialog(QWidget *parent)
    : Dialog(parent)
{
    setButtons(Ok|Cancel);
    setCaption(i18n("Magnatune Settings"));
    QWidget *mw=new QWidget(this);
    QFormLayout *layout=new QFormLayout(mw);
    member=new QComboBox(mw);
    for (int i=0; i<MagnatuneService::MB_Count; ++i) {
        member->addItem(MagnatuneService::membershipStr((MagnatuneService::MemberShip)i, true));
    }
    user=new LineEdit(mw);
    pass=new LineEdit(mw);
    pass->setEchoMode(QLineEdit::Password);
    dl=new QComboBox(mw);
    for (int i=0; i<=MagnatuneService::DL_Count; ++i) {
        dl->addItem(MagnatuneService::downloadTypeStr((MagnatuneService::DownloadType)i, true));
    }

    layout->setWidget(0, QFormLayout::LabelRole, new BuddyLabel(i18n("Membership:"), mw, member));
    layout->setWidget(0, QFormLayout::FieldRole, member);
    layout->setWidget(1, QFormLayout::LabelRole, new BuddyLabel(i18n("Username:"), mw, user));
    layout->setWidget(1, QFormLayout::FieldRole, user);
    layout->setWidget(2, QFormLayout::LabelRole, new BuddyLabel(i18n("Password:"), mw, pass));
    layout->setWidget(2, QFormLayout::FieldRole, pass);
    BuddyLabel *dlLabel=new BuddyLabel(i18n("Downloads:"), mw, dl);
    layout->setWidget(3, QFormLayout::LabelRole, dlLabel);
    layout->setWidget(3, QFormLayout::FieldRole, dl);
    layout->setMargin(0);
    dlLabel->setVisible(false); // TODO: Magnatune downloads!
    dl->setVisible(false); // TODO: Magnatune downloads!

    setMainWidget(mw);
    connect(member, SIGNAL(currentIndexChanged(int)), SLOT(membershipChanged(int)));
}

bool MagnatuneSettingsDialog::run(int m, int d, const QString &u, const QString &p)
{
    member->setCurrentIndex(m);
    dl->setCurrentIndex(d);
    user->setText(u);
    user->setEnabled(m);
    pass->setText(p);
    pass->setEnabled(m);
    // dl->setEnabled(MagnatuneService::MB_Download==m); // TODO: Magnatune downloads!
    return QDialog::Accepted==Dialog::exec();
}

void MagnatuneSettingsDialog::membershipChanged(int i)
{
    user->setEnabled(0!=i);
    pass->setEnabled(0!=i);
    // dl->setEnabled(MagnatuneService::MB_Download==i); // TODO: Magnatune downloads!
}
