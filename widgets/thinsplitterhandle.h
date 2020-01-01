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

#ifndef THIN_SPLITTER_HANDLE_H
#define THIN_SPLITTER_HANDLE_H

#include <QSplitter>

class ThinSplitterHandle : public QSplitterHandle
{
public:
    ThinSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
    ~ThinSplitterHandle() override { }

    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    QSize sizeHint() const override { return QSize(0, 0); }
    void setHighlightUnderMouse() { highlightUnderMouse=true; }

private:
    void updateMask();

private:
    int sz;
    bool highlightUnderMouse;
    bool underMouse;
};

#endif

