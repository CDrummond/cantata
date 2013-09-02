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

#ifndef MESSAGE_OVERLAY_H
#define MESSAGE_OVERLAY_H

#include <QWidget>

class ToolButton;
class QTimer;

class MessageOverlay : public QWidget
{
    Q_OBJECT

public:
    MessageOverlay(QObject *p);
    virtual ~MessageOverlay() { }

    void setWidget(QWidget *widget);
    void setText(const QString &txt, int timeout=-1, bool allowCancel=true);
    void paintEvent(QPaintEvent *);

Q_SIGNALS:
    void cancel();

private Q_SLOTS:
    void timeout();

private:
    bool eventFilter(QObject *o, QEvent *e);
    void setSizeAndPosition();

private:
    int spacing;
    QString text;
    ToolButton *cancelButton;
    QTimer *timer;
};

#endif

