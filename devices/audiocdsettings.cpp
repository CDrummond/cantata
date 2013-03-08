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

#include "audiocdsettings.h"
#include "settings.h"

AudioCdSettings::AudioCdSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
}

void AudioCdSettings::load()
{
    cddbAuto->setChecked(Settings::self()->cddbAuto());
    cddbHost->setText(Settings::self()->cddbHost());
    cddbPort->setValue(Settings::self()->cddbPort());
    paranoiaFull->setChecked(Settings::self()->paranoiaFull());
    paranoiaNeverSkip->setChecked(Settings::self()->paranoiaNeverSkip());
}

void AudioCdSettings::save()
{
    Settings::self()->saveCddbAuto(cddbAuto->isChecked());
    Settings::self()->saveCddbHost(cddbHost->text().trimmed());
    Settings::self()->saveCddbPort(cddbPort->value());
    Settings::self()->saveParanoiaFull(paranoiaFull->isChecked());
    Settings::self()->saveParanoiaNeverSkip(paranoiaNeverSkip->isChecked());
}
