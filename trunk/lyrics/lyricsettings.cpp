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
#include "icon.h"
#include "config.h"
#include "settings.h"

LyricSettings::LyricSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    connect(up, SIGNAL(clicked()), SLOT(moveUp()));
    connect(down, SIGNAL(clicked()), SLOT(moveDown()));
    connect(providers, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            SLOT(currentItemChanged(QListWidgetItem*)));
    up->setIcon(Icon("arrow-up"));
    down->setIcon(Icon("arrow-down"));
}

void LyricSettings::load()
{
    const QList<UltimateLyricsProvider *> &lprov=UltimateLyrics::self()->getProviders();

    providers->clear();
    foreach (const UltimateLyricsProvider *provider, lprov) {
        QListWidgetItem *item = new QListWidgetItem(providers);
        QString name(provider->getName());
        name.replace("(POLISH)", i18n("(Polish Translations)"));
        name.replace("(PORTUGUESE)", i18n("(Portuguese Translations)"));
        item->setText(name);
        item->setData(Qt::UserRole, provider->getName());
        item->setCheckState(provider->isEnabled() ? Qt::Checked : Qt::Unchecked);
    }
}

void LyricSettings::save()
{
    QStringList enabled;
    for (int i=0 ; i<providers->count() ; ++i) {
        const QListWidgetItem* item = providers->item(i);
        if (Qt::Checked==item->checkState()) {
            enabled << item->data(Qt::UserRole).toString();
        }
    }
    UltimateLyrics::self()->setEnabled(enabled);
}

void LyricSettings::currentItemChanged(QListWidgetItem *item)
{
    if (!item) {
        up->setEnabled(false);
        down->setEnabled(false);
    } else {
        const int row = providers->row(item);
        up->setEnabled(row != 0);
        down->setEnabled(row != providers->count() - 1);
    }
}

void LyricSettings::moveUp()
{
    move(-1);
}

void LyricSettings::moveDown()
{
    move(+1);
}

void LyricSettings::move(int d)
{
    const int row = providers->currentRow();
    QListWidgetItem* item = providers->takeItem(row);
    providers->insertItem(row + d, item);
    providers->setCurrentRow(row + d);
}
