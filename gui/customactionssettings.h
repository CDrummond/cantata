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

#ifndef CUSTOMACTIONS_SETTINGS_H
#define CUSTOMACTIONS_SETTINGS_H

#include "support/dialog.h"
#include "support/lineedit.h"
#include <QWidget>
#include <QTreeWidget>

class QPushButton;

class CustomActionDialog : public Dialog
{
    Q_OBJECT
public:
    CustomActionDialog(QWidget *p);
    bool create();
    bool edit(const QString &name, const QString &cmd);
    QString nameText() const { return nameEntry->text().trimmed(); }
    QString commandText() const { return commandEntry->text().trimmed(); }
private:
    LineEdit *nameEntry;
    LineEdit *commandEntry;
};

class CustomActionsSettings : public QWidget
{
    Q_OBJECT
public:
    CustomActionsSettings(QWidget *parent);
    ~CustomActionsSettings() override;

    void load();
    void save();

private Q_SLOTS:
    void controlButtons();
    void addCommand();
    void editCommand();
    void delCommand();

private:
    QTreeWidget *tree;
    QPushButton *add;
    QPushButton *edit;
    QPushButton *del;
    CustomActionDialog *dlg;
};

#endif
