/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef DYNAMIC_RULES_DIALOG_H
#define DYNAMIC_RULES_DIALOG_H

#include "config.h"
#include "dialog.h"
#include "ui_dynamicrules.h"
#include "dynamic.h"

class DynamicRuleDialog;
class QStandardItemModel;
class QStandardItem;
class RulesSort;

class DynamicRulesDialog : public Dialog, Ui::DynamicRules
{
    Q_OBJECT

public:
    DynamicRulesDialog(QWidget *parent);
    virtual ~DynamicRulesDialog();

    void edit(const QString &name);

private:
    void slotButtonClicked(int button);
    bool save();
    int indexOf(QStandardItem *item, bool diff=false);

private Q_SLOTS:
    void saved(bool s);
    void enableOkButton();
    void controlButtons();
    void add();
    void addRule(const Dynamic::Rule &rule);
    void edit();
    void remove();
    void showAbout();

private:
    RulesSort *proxy;
    QStandardItemModel *model;
    QString origName;
    DynamicRuleDialog *dlg;
};

#endif
