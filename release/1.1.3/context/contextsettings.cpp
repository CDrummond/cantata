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

#include "contextsettings.h"
#include "wikipediasettings.h"
#include "lyricsettings.h"
#include "othersettings.h"
#include "localize.h"

ContextSettings::ContextSettings(QWidget *p)
    : QTabWidget(p)
{
    wiki=new WikipediaSettings(this);
    lyrics=new LyricSettings(this);
    other=new OtherSettings(this);
    addTab(lyrics, i18n("Lyrics Providers"));
    addTab(wiki, i18n("Wikipedia Languages"));
    addTab(other, i18n("Other"));
}

void ContextSettings::load()
{
    wiki->load();
    lyrics->load();
    other->load();
}

void ContextSettings::save()
{
    wiki->save();
    lyrics->save();
    other->save();
}
