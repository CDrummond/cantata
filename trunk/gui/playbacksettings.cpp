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
    #ifdef CANTATA_ANDROID
    stopOnExit->setVisible(false);
    stopOnExitLabel->setVisible(false);
    #endif
    #if defined Q_OS_WIN || defined CANTATA_ANDROID
    stopDynamizerOnExit->setVisible(false);
    stopDynamizerOnExitLabel->setVisible(false);
    #endif
};

void PlaybackSettings::load()
{
    stopOnExit->setChecked(Settings::self()->stopOnExit());
    #ifndef CANTATA_ANDROID
    stopFadeDuration->setValue(Settings::self()->stopFadeDuration());
    #endif
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    stopDynamizerOnExit->setChecked(Settings::self()->stopDynamizerOnExit());
    #endif
}

void PlaybackSettings::save()
{
    Settings::self()->saveStopOnExit(stopOnExit->isChecked());
    #ifndef CANTATA_ANDROID
    Settings::self()->saveStopFadeDuration(stopFadeDuration->value());
    #endif
    #if !defined Q_OS_WIN && !defined CANTATA_ANDROID
    Settings::self()->saveStopDynamizerOnExit(stopDynamizerOnExit->isChecked());
    #endif
}
