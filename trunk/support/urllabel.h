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

#ifndef URLLABEL_H
#define URLLABEL_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KUrlLabel>
typedef KUrlLabel UrlLabel;
#else
#include <QLabel>
#include <QCursor>

class UrlLabel : public QLabel
{
    Q_OBJECT

public:
    UrlLabel(QWidget *p);
    virtual ~UrlLabel() { }

    void setText(const QString &t);
    void setProperty(const char *name, const QVariant &value);

Q_SIGNALS:
    void leftClickedUrl();

protected:
    void mousePressEvent(QMouseEvent *ev);
    void mouseReleaseEvent(QMouseEvent *);

private:
    bool pressed;
};
#endif

#endif // URLLABEL_H
