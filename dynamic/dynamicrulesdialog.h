/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDialog>
#else
#include <QtGui/QDialog>
class QDialogButtonBox;
class QAbstractButton;
class QPushButton;
#endif
#include "ui_dynamicrules.h"

class DynamicRuleDialog;
class QStandardItemModel;
class RulesSort;

#ifdef ENABLE_KDE_SUPPORT
class DynamicRulesDialog : public KDialog, Ui::DynamicRules
#else
class DynamicRulesDialog : public QDialog, Ui::DynamicRules
#endif
{
    Q_OBJECT

public:
    DynamicRulesDialog(QWidget *parent);
    virtual ~DynamicRulesDialog();

    void edit(const QString &name);

private:
    #ifdef ENABLE_KDE_SUPPORT
    void slotButtonClicked(int button);
    #endif
    bool save();

private Q_SLOTS:
    void enableOkButton();
    #ifndef ENABLE_KDE_SUPPORT
    void buttonPressed(QAbstractButton *button);
    #endif
    void controlButtons();
    void add();
    void edit();
    void remove();
    void showAbout();

private:
    RulesSort *proxy;
    QStandardItemModel *model;
    QString origName;
    DynamicRuleDialog *dlg;
    #ifndef ENABLE_KDE_SUPPORT
    QDialogButtonBox *buttonBox;
    #endif
};

#endif
