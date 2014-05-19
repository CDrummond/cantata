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

#include "scrobblingstatus.h"
#include "scrobbler.h"
#include "widgets/icons.h"
#include "support/localize.h"

ScrobblingStatus::ScrobblingStatus(QWidget *p)
    : ToolButton(p)
{
    setCheckable(true);
    setIcon(Icons::self()->lastFmIcon);
    connect(Scrobbler::self(), SIGNAL(authenticated(bool)), SLOT(setVisible(bool)));
    connect(Scrobbler::self(), SIGNAL(enabled(bool)), SLOT(setChecked(bool)));
    connect(this, SIGNAL(toggled(bool)), Scrobbler::self(), SLOT(setEnabled(bool)));
    setVisible(Scrobbler::self()->isAuthenticated());
    setChecked(Scrobbler::self()->isScrobblingEnabled());
    setToolTip(i18n("Scrobbling"));
}
