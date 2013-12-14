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

#ifndef TEXTBROWSER_H
#define TEXTBROWSER_H

#include <QTextBrowser>

class TextBrowser : public QTextBrowser
{
public:
    TextBrowser(QWidget *p);
    QVariant loadResource(int type, const QUrl &name);

    void setZoom(int diff) { if (diff) zoomIn(diff);  }
    int zoom() const { return font().pointSize()-origZoomValue; }
    void setPicSize(const QSize &p) { pSize=p; }
    QSize picSize() const { return pSize; }
    void setPal(const QPalette &pal);

private:
    int origZoomValue;
    QSize pSize;
};

#endif
