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
#include "mpdconnection.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    replayGain->addItem(i18n("None"), QVariant("off"));
    replayGain->addItem(i18n("Track"), QVariant("track"));
    replayGain->addItem(i18n("Album"), QVariant("album"));
    #else
    replayGain->addItem(tr("None"), QVariant("off"));
    replayGain->addItem(tr("Track"), QVariant("track"));
    replayGain->addItem(tr("Album"), QVariant("album"));
    #endif
    connect(MPDConnection::self(), SIGNAL(replayGain(const QString &)), this, SLOT(replayGainSetting(const QString &)));
    connect(this, SIGNAL(setReplayGain(const QString &)), MPDConnection::self(), SLOT(setReplayGain(const QString &)));
    connect(this, SIGNAL(setCrossFade(int)), MPDConnection::self(), SLOT(setCrossFade(int)));
    connect(this, SIGNAL(getReplayGain()), MPDConnection::self(), SLOT(getReplayGain()));
};

void PlaybackSettings::load()
{
    crossfading->setValue(MPDStatus::self()->crossFade());
    emit getReplayGain();
    stopOnExit->setChecked(Settings::self()->stopOnExit());
}

void PlaybackSettings::save()
{
    emit setCrossFade(crossfading->value());
    emit setReplayGain(replayGain->itemData(replayGain->currentIndex()).toString());
    Settings::self()->saveStopOnExit(stopOnExit->isChecked());
}

void PlaybackSettings::replayGainSetting(const QString &rg)
{
    replayGain->setCurrentIndex(0);

    for(int i=0; i<replayGain->count(); ++i) {
        if (replayGain->itemData(i).toString()==rg){
            replayGain->setCurrentIndex(i);
            break;
        }
    }
}

