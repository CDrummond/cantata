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

#ifndef MAGNATUNE_SETTINGS_DIALOG_H
#define MAGNATUNE_SETTINGS_DIALOG_H

#include "dialog.h"
#include "lineedit.h"
#include <QtGui/QComboBox>

class MagnatuneSettingsDialog : public Dialog
{
    Q_OBJECT

public:
    MagnatuneSettingsDialog(QWidget *parent);

    bool run(bool m, const QString &u, const QString &p);
    int membership() const { return member->currentIndex(); }
    QString username() const { return user->text().trimmed(); }
    QString password() const { return pass->text().trimmed(); }

private Q_SLOTS:
    void membershipChanged(int i);

private:
    QComboBox *member;
    LineEdit *user;
    LineEdit *pass;
};

#endif
