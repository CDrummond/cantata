/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "widgets/spacerwidget.h"
#include "support/localize.h"
#include <QHBoxLayout>

ScrobblingStatus::ScrobblingStatus(QWidget *p)
    : QWidget(p)
{
    btn = new ToolButton(this);
    btn->setCheckable(true);
    btn->setIcon(Icons::self()->lastFmIcon);
    connect(Scrobbler::self(), SIGNAL(authenticated(bool)), SLOT(setVisible(bool)));
    connect(Scrobbler::self(), SIGNAL(enabled(bool)), btn, SLOT(setChecked(bool)));
    connect(btn, SIGNAL(toggled(bool)), Scrobbler::self(), SLOT(setEnabled(bool)));
    setVisible(Scrobbler::self()->isAuthenticated());
    btn->setChecked(Scrobbler::self()->isEnabled());
    scrobblerChanged();

    QHBoxLayout *l=new QHBoxLayout(this);
    l->setMargin(0);
    l->setSpacing(0);
    l->addWidget(btn);
    l->addWidget(new SpacerWidget(this));
}

void ScrobblingStatus::scrobblerChanged()
{
    btn->setToolTip(i18n("%1: Scrobble Tracks", Scrobbler::self()->activeScrobbler()));
}
