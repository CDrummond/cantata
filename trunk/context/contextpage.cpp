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
 
#include "contextpage.h"
#include "artistview.h"
#include "albumview.h"
#include "songview.h"
#include "song.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QPainter>
#include <QRadialGradient>
#include <QPalette>

ContextPage::ContextPage(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    artist = new ArtistView(this);
    album = new AlbumView(this);
    song = new SongView(this);

    int m=layout->margin();
    layout->setMargin(0);
    layout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(artist);
    layout->addWidget(album);
    layout->addWidget(song);
    layout->addItem(new QSpacerItem(m, m, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->setStretch(1, 1);
    layout->setStretch(2, 1);
    layout->setStretch(3, 1);
    setLayout(layout);
    connect(artist, SIGNAL(findArtist(QString)), this, SIGNAL(findArtist(QString)));
    connect(album, SIGNAL(playSong(QString)), this, SIGNAL(playSong(QString)));
}

void ContextPage::update(const Song &s)
{
    artist->update(s);
    album->update(s);
    song->update(s);
}
