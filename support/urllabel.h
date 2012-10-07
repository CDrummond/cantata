/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QtGui/QLabel>
#include <QtGui/QFont>
class QResizeEvent;

class UrlLabel : public QLabel
{
    Q_OBJECT

public:
    UrlLabel(QWidget *p)
        : QLabel(p) {
        QFont f(font());
        f.setUnderline(true);
        setFont(f);
        QPalette pal(palette());
        pal.setColor(QPalette::Normal, QPalette::Foreground, pal.color(QPalette::Normal, QPalette::Highlight));
        pal.setColor(QPalette::Inactive, QPalette::Foreground, pal.color(QPalette::Inactive, QPalette::Highlight));
        pal.setColor(QPalette::Disabled, QPalette::Foreground, pal.color(QPalette::Disabled, QPalette::Highlight));
        setPalette(pal);
    }

    virtual ~UrlLabel() {
    }

Q_SIGNALS:
    void leftClickedUrl();

protected:
    void mouseReleaseEvent(QMouseEvent *) {
        emit leftClickedUrl();
    }
};
#endif

#endif // URLLABEL_H
