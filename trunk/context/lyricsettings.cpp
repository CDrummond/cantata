/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "lyricsettings.h"
#include "ultimatelyricsprovider.h"
#include "ultimatelyrics.h"
#include "ui_lyricsettings.h"
#include "localize.h"
#include "config.h"
#include "settings.h"

LyricSettings::LyricSettings(QWidget *p)
    : ToggleList(p)
{
    label->setText(i18n("Choose the websites you want to use when searching for lyrics."));
}

void LyricSettings::load()
{
    const QList<UltimateLyricsProvider *> &lprov=UltimateLyrics::self()->getProviders();

    available->clear();
    selected->clear();
    foreach (const UltimateLyricsProvider *provider, lprov) {
        QListWidgetItem *item = new QListWidgetItem(provider->isEnabled() ? selected : available);
        QString name(provider->getName());
        name.replace("(POLISH)", i18n("(Polish Translations)"));
        name.replace("(PORTUGUESE)", i18n("(Portuguese Translations)"));
        item->setText(name);
        item->setData(Qt::UserRole, provider->getName());
    }
}

void LyricSettings::save()
{
    QStringList enabled;
    for (int i=0; i<selected->count(); ++i) {
        enabled.append(selected->item(i)->data(Qt::UserRole).toString());
    }

    UltimateLyrics::self()->setEnabled(enabled);
}
