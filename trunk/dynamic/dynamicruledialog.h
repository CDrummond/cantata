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

#ifndef DYNAMIC_RULE_DIALOG_H
#define DYNAMIC_RULE_DIALOG_H

#include "config.h"
#include "dialog.h"
#include "ui_dynamicrule.h"
#include "dynamic.h"

class DynamicRuleDialog : public Dialog, Ui::DynamicRule
{
    Q_OBJECT

public:
    DynamicRuleDialog(QWidget *parent);
    virtual ~DynamicRuleDialog();

    bool edit(const Dynamic::Rule &rule);
    Dynamic::Rule rule() const;

    QString artist() const { return artistText->text().trimmed(); }
    QString similarArtists() const { return similarArtistsText->text().trimmed(); }
    QString albumArtist() const { return albumArtistText->text().trimmed(); }
    QString album() const { return albumText->text().trimmed(); }
    QString title() const { return titleText->text().trimmed(); }
    QString genre() const { return genreText->text().trimmed(); }

private Q_SLOTS:
    void enableOkButton();
};

#endif
