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

#include "backdropcreator.h"
#include "covers.h"
#include "thread.h"
#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <stdlib.h>
#ifdef Q_OS_WIN32
#include <time.h>
#endif

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void BackdropCreator::enableDebug()
{
    debugEnabled=true;
}

BackdropCreator::BackdropCreator()
    : QObject(0)
{
    #ifdef Q_OS_WIN32
    srand((unsigned int)time(0));
    #endif
    connect(Covers::self(), SIGNAL(cover(const Song &, const QImage &, const QString &)), SLOT(coverRetrieved(const Song &, const QImage &, const QString &)));
    imageSize=QApplication::fontMetrics().height()*12;
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

BackdropCreator::~BackdropCreator()
{
    thread->stop();
}

void BackdropCreator::create(const QString &artist, const QList<Song> &songs)
{
    DBUG << artist << songs.count();
    requestedArtist=artist;
    images.clear();
    requested.clear();
    foreach (const Song &s, songs) {
        Covers::Image img=Covers::self()->requestImage(s, true);
        if (!img.img.isNull()) {
            images.append(img.img.scaled(imageSize, imageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        } else {
            requested.insert(s);
        }
    }

    if (requested.isEmpty()) {
        createImage();
    }
}

void BackdropCreator::coverRetrieved(const Song &s, const QImage &img, const QString &file)
{
    DBUG << requestedArtist << s.albumArtist();
    Q_UNUSED(file)
    if (requested.contains(s)) {
        requested.remove(s);
        if (!img.isNull()) {
            images.append(img.scaled(imageSize, imageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        if (requested.isEmpty()) {
            createImage();
        }
    }
}

void BackdropCreator::createImage()
{
    DBUG << images.count();
    QImage backdrop;

    switch (images.count()) {
    case 0:
        break;
    case 1:
        backdrop=images.at(0);
        break;
    case 2: {
        backdrop=QImage(2*imageSize, 2*imageSize, QImage::Format_RGB32);
        QPainter p(&backdrop);
        p.drawImage(0, 0, images.at(0));
        p.drawImage(0, imageSize, images.at(1));
        p.drawImage(imageSize, imageSize, images.at(0));
        p.drawImage(imageSize, 0, images.at(1));
        break;
    }
    default: {
        static const int constHCount=10;
        static const int constVCount=5;
        backdrop=QImage(constHCount*imageSize, constVCount*imageSize, QImage::Format_RGB32);
        QPainter p(&backdrop);
        QList<QImage> toUse=images;
        QList<int> currentLine;
        QList<int> lastLine;

        for (int y=0; y<constVCount; ++y) {
            for (int x=0; x<constHCount; ++x) {
                #ifdef Q_OS_WIN32
                int index=rand()%toUse.count();
                #else
                int index=random()%toUse.count();
                #endif
                p.drawImage(x*imageSize, y*imageSize, toUse.takeAt(index));
                if (toUse.isEmpty()) {
                    toUse=images;
                }
                currentLine.append(index);
            }
            lastLine=currentLine;
            currentLine.clear();
        }
        break;
    }
    }

    emit created(requestedArtist, backdrop);
}
