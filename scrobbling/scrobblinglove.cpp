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

#include "scrobblinglove.h"
#include "scrobbler.h"
#include "support/monoicon.h"
#include "support/utils.h"

ScrobblingLove::ScrobblingLove(QWidget *p)
    : ToolButton(p)
{
    love = MonoIcon::icon(FontAwesome::hearto, Utils::monoIconColor());
    loved = MonoIcon::icon(FontAwesome::heart, Utils::monoIconColor());
    setIcon(love);
    connect(Scrobbler::self(), SIGNAL(loveEnabled(bool)), SLOT(setVisible(bool)));
    connect(Scrobbler::self(), SIGNAL(songChanged(bool)), SLOT(songChanged(bool)));
    connect(Scrobbler::self(), SIGNAL(scrobblerChanged()), SLOT(scrobblerChanged()));
    setVisible(Scrobbler::self()->isLoveEnabled());
    connect(this, SIGNAL(clicked()), this, SLOT(sendLove()));
    songChanged(false);
}

void ScrobblingLove::sendLove()
{
    Scrobbler::self()->love();
    scrobblerChanged();
}

void ScrobblingLove::songChanged(bool valid)
{
    setEnabled(valid);
    scrobblerChanged();
}

void ScrobblingLove::scrobblerChanged()
{
    setIcon(isEnabled() && Scrobbler::self()->lovedTrack() ? loved : love);
    setToolTip(isEnabled() && Scrobbler::self()->lovedTrack()
                ? tr("%1: Loved Current Track").arg(Scrobbler::self()->activeScrobbler())
                : tr("%1: Love Current Track").arg(Scrobbler::self()->activeScrobbler()));
}

#include "moc_scrobblinglove.cpp"
