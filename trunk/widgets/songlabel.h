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

#ifndef SONG_LABEL_H
#define SONG_LABEL_H

#include <QLabel>
#include <QFontMetrics>
#include <QList>

class QResizeEvent;
class Song;

class SongLabel : public QLabel
{
public:
    SongLabel(QWidget *p);
    void update(const Song &song);

protected:
    QSize minimumSizeHint() const {
        QSize sh = QLabel::minimumSizeHint();
        sh.setWidth(-1);
        return sh;
    }

    QSize sizeHint() const { return QSize(boldMetrics.width(plainText), QLabel::sizeHint().height()); }
    void resizeEvent(QResizeEvent *) { elideText(); }

private:
    void calcTagPositions();
    void elideText();

private:
    QString text;
    QString plainText;
    QFontMetrics boldMetrics;
    Qt::TextElideMode elideMode;
    QList<int> open;
    QList<int> close;
};

#endif
