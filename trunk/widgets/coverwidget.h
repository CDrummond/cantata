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

#ifndef COVERWIDGET_H
#define COVERWIDGET_H

#include <QLabel>
#include <QImage>
#include "song.h"

class QPixmap;

class CoverWidget : public QLabel
{
    Q_OBJECT

public:
    static const int constBorder;

    CoverWidget(QWidget *p);
    virtual ~CoverWidget();

    void update(const Song &s);
    const Song & song() const { return current; }
    bool isEmpty() const { return empty; }
    bool isValid() const { return !empty && valid; }
    const QString & fileName() const { return coverFileName; }
    const QImage &image() const;

Q_SIGNALS:
    void coverImage(const QImage &img);
    void coverFile(const QString &name);
    void clicked();

private Q_SLOTS:
    void init();
    void coverRetreived(const Song &s, const QImage &img, const QString &file);

private:
    const QPixmap & stdPixmap(bool stream);
    void update(const QImage &i);
    void update(const QPixmap &pix);
    bool eventFilter(QObject *object, QEvent *event);
    void resizeEvent(QResizeEvent *e);
//     void paintEvent(QPaintEvent *e);

private:
    bool empty;
    bool valid;
    bool pressed;
    Song current;
    QString tipText;
    mutable QImage img;
//     mutable QPixmap bgnd;
    QString coverFileName;
    QPixmap noStreamCover;
    QPixmap noCover;
    QString noStreamCoverFileName;
    QString noCoverFileName;
};

#endif
