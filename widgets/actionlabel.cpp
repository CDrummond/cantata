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

#include "actionlabel.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIconLoader>
#else
#include <QIcon>
#endif
#include <QLabel>
#include <QTimer>
#include <QPixmap>
#include <QMatrix>

// Borrowed from kolourpaint...
static QMatrix matrixWithZeroOrigin(const QMatrix &matrix, int width, int height)
{
    QRect newRect(matrix.mapRect(QRect(0, 0, width, height)));

    return QMatrix(matrix.m11(), matrix.m12(), matrix.m21(), matrix.m22(),
                   matrix.dx() - newRect.left(), matrix.dy() - newRect.top());
}

static QMatrix rotateMatrix(int width, int height, double angle)
{
    QMatrix matrix;
    matrix.translate(width/2, height/2);
    matrix.rotate(angle);

    return matrixWithZeroOrigin(matrix, width, height);
}

static const int constNumIcons=8;
static int theUsageCount;
static QPixmap *theIcons[constNumIcons];

ActionLabel::ActionLabel(QWidget *parent)
    : QLabel(parent)
{
    static const int constIconSize(64);

    setMinimumSize(constIconSize, constIconSize);
    setMaximumSize(constIconSize, constIconSize);
    setAlignment(Qt::AlignCenter);

    if(0==theUsageCount++) {
        #ifdef ENABLE_KDE_SUPPORT
        QImage img(KIconLoader::global()->loadIcon("audio-x-generic", KIconLoader::NoGroup, 48).toImage());
        #else
        QImage img(QIcon::fromTheme("audio-x-generic").pixmap(48, 48).toImage());
        #endif
        double increment=360.0/constNumIcons;

        for(int i=0; i<constNumIcons; ++i) {
            theIcons[i]=new QPixmap(QPixmap::fromImage(0==i ? img
                                                            : img.transformed(rotateMatrix(img.width(), img.height(), increment*i),
                                                                              Qt::SmoothTransformation)));
        }
    }

    setPixmap(*theIcons[0]);
    timer=new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(rotateIcon()));
}

ActionLabel::~ActionLabel()
{
    if (0==--theUsageCount) {
        for (int i=0; i<constNumIcons; ++i) {
            delete theIcons[i];
            theIcons[i]=NULL;
        }
    }
}

void ActionLabel::startAnimation()
{
    count=0;
    setPixmap(*theIcons[0]);
    timer->start(2000/constNumIcons);
}

void ActionLabel::stopAnimation()
{
    timer->stop();
    count=0;
    setPixmap(*theIcons[count]);
}

void ActionLabel::rotateIcon()
{
    if (++count==constNumIcons) {
        count=0;
    }

    setPixmap(*theIcons[count]);
}
