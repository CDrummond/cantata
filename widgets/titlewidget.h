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

#ifndef TITLE_WIDGET_H
#define TITLE_WIDGET_H

#include <QListView>
#include "mpd-interface/song.h"

class QImage;
class QIcon;
class SqueezedTextLabel;
class QLabel;

class TitleWidget : public QListView
{
    Q_OBJECT
public:
    TitleWidget(QWidget *p);
    ~TitleWidget() override { }
    void update(const Song &sng, const QIcon &icon, const QString &text, const QString &sub, bool showControls=false);
    bool eventFilter(QObject *obj, QEvent *event) override;

Q_SIGNALS:
    void clicked();
    void addToPlayQueue();
    void replacePlayQueue();

private Q_SLOTS:
    void coverRetrieved(const Song &s, const QImage &img, const QString &file);

private:
    void setImage(const QImage &img);

private:
    Song song;
    bool pressed;
    QLabel *back;
    QLabel *image;
    QWidget *controls;
    SqueezedTextLabel *mainText;
    SqueezedTextLabel *subText;
};

#endif
