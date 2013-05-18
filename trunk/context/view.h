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

#ifndef VIEW_H
#define VIEW_H

#include <QWidget>
#include <QSize>
#include "song.h"

class QImage;
class QLabel;
class QTextBrowser;
class Spinner;

class View : public QWidget
{
public:
    View(QWidget *p);
    
    void clear();
    void setStandardHeader(const QString &h) { stdHeader=h; }
    void setHeader(const QString &str);
    void setPicSize(const QSize &sz);
    void setPic(const QImage &img);
    void showEvent(QShowEvent *e);
    void showSpinner();
    void hideSpinner();
    void setEditable(bool e);

    virtual void update(const Song &s, bool force)=0;
    
protected:
    Song currentSong;
    QString stdHeader;
    QLabel *header;
    QLabel *pic;
    QTextBrowser *text;
    bool needToUpdate;
    QSize picSize;
    Spinner *spinner;
};

#endif

