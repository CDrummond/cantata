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
#include "localize.h"

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

AudioCdSettings::AudioCdSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    #if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
    cdLookup->addItem(i18n("CDDB"), true);
    cdLookup->addItem(i18n("MusicBrainz"), false);
    #else
    REMOVE(cdLookup)
    REMOVE(cdLookupLabel)
    #endif

    #if !defined CDDB_FOUND
    REMOVE(cddbHost)
    REMOVE(cddbHostLabel)
    REMOVE(cddbPort)
    REMOVE(cddbPortLabel)
    #endif
    #if !defined CDDB_FOUND
    REMOVE(playbackOptions)
    #endif
}

void AudioCdSettings::load()
{
    cdAuto->setChecked(Settings::self()->cdAuto());
    #if defined CDDB_FOUND
    cddbHost->setText(Settings::self()->cddbHost());
    cddbPort->setValue(Settings::self()->cddbPort());
    #endif
    #if defined LAME_FOUND
    cdMp3->setChecked(Settings::self()->cdMp3());
    #endif
    paranoiaFull->setChecked(Settings::self()->paranoiaFull());
    paranoiaNeverSkip->setChecked(Settings::self()->paranoiaNeverSkip());
    #if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
    for (int i=0; i<cdLookup->count(); ++i) {
        if (cdLookup->itemData(i).toBool()==Settings::self()->useCddb()) {
            cdLookup->setCurrentIndex(i);
            break;
        }
    }
    #endif
}

void AudioCdSettings::save()
{
    Settings::self()->saveCdAuto(cdAuto->isChecked());
    #if defined CDDB_FOUND
    Settings::self()->saveCddbHost(cddbHost->text().trimmed());
    Settings::self()->saveCddbPort(cddbPort->value());
    #endif
    Settings::self()->saveParanoiaFull(paranoiaFull->isChecked());
    Settings::self()->saveParanoiaNeverSkip(paranoiaNeverSkip->isChecked());
    #if defined CDDB_FOUND && defined MUSICBRAINZ5_FOUND
    Settings::self()->saveUseCddb(cdLookup->itemData(cdLookup->currentIndex()).toBool());
    #endif
    #if defined LAME_FOUND
    Settings::self()->saveCdMp3(cdMp3->isChecked());
    #endif
}

