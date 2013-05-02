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

#include "playbacksettings.h"
#include "settings.h"
#include "localize.h"

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    stopFadeDuration->setSpecialValueText(i18n("Do not fadeout"));
    stopFadeDuration->setSuffix(i18n(" ms"));
    stopFadeDuration->setRange(Settings::MinFade, Settings::MaxFade);
    stopFadeDuration->setSingleStep(100);
    stopAction->addItem(i18n("Stop immediately (or after fadout)"));
    stopAction->addItem(i18n("Stop after current track"));
}

void PlaybackSettings::load()
{
    stopOnExit->setChecked(Settings::self()->stopOnExit());
    stopFadeDuration->setValue(Settings::self()->stopFadeDuration());
    stopDynamizerOnExit->setChecked(Settings::self()->stopDynamizerOnExit());
    stopAction->setCurrentIndex(Settings::self()->stopAfterCurrent() ? 1 : 0);
}

void PlaybackSettings::save()
{
    Settings::self()->saveStopOnExit(stopOnExit->isChecked());
    Settings::self()->saveStopFadeDuration(stopFadeDuration->value());
    Settings::self()->saveStopDynamizerOnExit(stopDynamizerOnExit->isChecked());
    Settings::self()->saveStopAfterCurrent(1==stopAction->currentIndex());
}
