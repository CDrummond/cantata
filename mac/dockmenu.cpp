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

#include "dockmenu.h"
#include "gui/stdactions.h"
#include "mpd-interface/mpdstatus.h"

DockMenu::DockMenu(QWidget *p)
    : QMenu(p)
{
    setAsDockMenu();
    addAction(StdActions::self()->prevTrackAction);
    playPauseAction=addAction(tr("Play"));
    addAction(StdActions::self()->stopPlaybackAction);
    addAction(StdActions::self()->stopAfterCurrentTrackAction);
    addAction(StdActions::self()->nextTrackAction);
    connect(playPauseAction, SIGNAL(triggered()), StdActions::self()->playPauseTrackAction, SIGNAL(triggered()));
}

void DockMenu::update(MPDStatus * const status)
{
    playPauseAction->setEnabled(status->playlistLength()>0);
    playPauseAction->setText(MPDState_Playing==status->state() ? tr("Pause") : tr("Play"));
}

#include "moc_dockmenu.cpp"
