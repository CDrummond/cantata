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

#ifndef PLAYLIST_RULES_DIALOG_H
#define PLAYLIST_RULES_DIALOG_H

#include "config.h"
#include "support/dialog.h"
#include "ui_playlistrules.h"
#include "dynamicplaylists.h"

class PlaylistRuleDialog;
class QStandardItemModel;
class QStandardItem;
class RulesSort;

class PlaylistRulesDialog : public Dialog, Ui::PlaylistRules
{
    Q_OBJECT

public:
    PlaylistRulesDialog(QWidget *parent, RulesPlaylists *m);
    ~PlaylistRulesDialog() override;

    void edit(const QString &name);

private:
    void slotButtonClicked(int button) override;
    bool save();
    int indexOf(QStandardItem *item, bool diff=false);

private Q_SLOTS:
    void saved(bool s);
    void enableOkButton();
    void controlButtons();
    void add();
    void addRule(const RulesPlaylists::Rule &rule);
    void edit();
    void remove();
    void showAbout();
    void setOrder();
    void ratingChanged(int value);

private:
    RulesPlaylists *rules;
    RulesSort *proxy;
    QStandardItemModel *model;
    QString origName;
    PlaylistRuleDialog *dlg;
};

#endif
