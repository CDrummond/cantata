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

#ifndef PLAYLIST_RULE_DIALOG_H
#define PLAYLIST_RULE_DIALOG_H

#include "config.h"
#include "support/dialog.h"
#include "ui_playlistrule.h"
#include "dynamicplaylists.h"

class PlaylistRuleDialog : public Dialog, Ui::PlaylistRule
{
    Q_OBJECT

public:
    PlaylistRuleDialog(QWidget *parent, bool isDynamic);
    ~PlaylistRuleDialog() override;

    void createNew() { edit(RulesPlaylists::Rule(), true); }
    bool edit(const RulesPlaylists::Rule &rule, bool isAdd=false);
    RulesPlaylists::Rule rule() const;

    QString artist() const { return artistText->text().trimmed(); }
    QString similarArtists() const { return similarArtistsText ? similarArtistsText->text().trimmed() : QString(); }
    QString albumArtist() const { return albumArtistText->text().trimmed(); }
    QString composer() const { return composerText->text().trimmed(); }
    QString comment() const { return commentText->text().trimmed(); }
    QString album() const { return albumText->text().trimmed(); }
    QString title() const { return titleText->text().trimmed(); }
    QString genre() const { return genreText->text().trimmed(); }
    QString filename() const { return filenameText->text().trimmed(); }

Q_SIGNALS:
    void addRule(const RulesPlaylists::Rule &r);

private Q_SLOTS:
    void enableOkButton();

private:
    void slotButtonClicked(int button) override;

private:
    bool addingRules;
};

#endif
