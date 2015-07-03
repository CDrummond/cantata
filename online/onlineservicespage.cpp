/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "onlineservicespage.h"
#include "onlinedbwidget.h"
#include "jamendoservice.h"
#include "magnatuneservice.h"
#include "soundcloudservice.h"
#include "soundcloudwidget.h"
#include "podcastservice.h"
#include "podcastwidget.h"
#include "streams/streamspage.h"
#include "models/streamsmodel.h"

OnlineServicesPage::OnlineServicesPage(QWidget *p)
    : MultiPageWidget(p)
{
    addPage(StreamsModel::self()->name(), StreamsModel::self()->icon(), StreamsModel::self()->title(), StreamsModel::self()->descr(), new StreamsPage(this));

    JamendoService *jamendo=new JamendoService(this);
    addPage(jamendo->name(), jamendo->icon(), jamendo->title(), jamendo->descr(), new OnlineDbWidget(jamendo, this));
    connect(jamendo, SIGNAL(error(QString)), this, SIGNAL(error(QString)));

    MagnatuneService *magnatune=new MagnatuneService(this);
    addPage(magnatune->name(), magnatune->icon(), magnatune->title(), magnatune->descr(), new OnlineDbWidget(magnatune, this));
    connect(magnatune, SIGNAL(error(QString)), this, SIGNAL(error(QString)));

    SoundCloudService *soundcloud=new SoundCloudService(this);
    addPage(soundcloud->name(), soundcloud->icon(), soundcloud->title(), soundcloud->descr(), new SoundCloudWidget(soundcloud, this));

    podcast=new PodcastService(this);
    addPage(podcast->name(), podcast->icon(), podcast->title(), podcast->descr(), new PodcastWidget(podcast, this));
    connect(podcast, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
}

OnlineServicesPage::~OnlineServicesPage()
{
}

bool OnlineServicesPage::isDownloading()
{
    return podcast->isDownloading();
}

void OnlineServicesPage::cancelAll()
{
    podcast->cancelAll();
}
