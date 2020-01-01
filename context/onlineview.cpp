/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "onlineview.h"
#include "gui/covers.h"
#include "online/podcastservice.h"

OnlineView::OnlineView(QWidget *p)
    : View(p)
{
    setStandardHeader(tr("Song Information"));
    int imageSize=fontMetrics().height()*18;
    setPicSize(QSize(imageSize, imageSize));
}

void OnlineView::update(const Song &song, bool force)
{
    if (force || song!=currentSong) {
        currentSong=song;
        if (!isVisible()) {
            needToUpdate=true;
            return;
        }
        setHeader(currentSong.describe().replace("<b>", "").replace("</b>", ""));
        QString descr = PodcastService::self()->episodeDescr(song);
        Covers::Image cImg = Covers::self()->requestImage(song, true);
        if (!cImg.img.isNull()) {
            setHtml(createPicTag(cImg.img, cImg.fileName)+(descr.isEmpty() ? QString() : ("<br>"+descr)));
        } else {
            setHtml(descr.isEmpty() ? QString() : descr);
        }
    }
}

#include "moc_onlineview.cpp"
