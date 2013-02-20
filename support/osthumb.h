/* liboverlay-scrollbar-qt
 *
 * Copyright © 2011 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authored by Romeo Calota (kicsyromy@gmail.com)
 */

#ifndef OSTHUMB_H
#define OSTHUMB_H

#include <QWidget>

class OsThumbPrivate;

class OsThumb : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool hidden READ hidden WRITE setHidden)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)

public:
    explicit OsThumb(Qt::Orientation = Qt::Vertical, QWidget *parent = 0);
    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation);
    int minimum() const;
    int maximum() const;
    void setMinimum(int minimum);
    void setMaximum(int maximum);
    bool isVisible() const;

protected:
    OsThumbPrivate * const d_ptr;

    void paintEvent(QPaintEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);

Q_SIGNALS:
    void thumbDragged(QPoint coordinates);
    void pageUp();
    void pageDown();
    void hidding();
    void showing();

public Q_SLOTS:
    void show();
    void hide();

private:
    Q_DECLARE_PRIVATE(OsThumb);
    bool hidden() const;
    void setHidden(bool hidden);
};

#endif // OSTHUMB_H
