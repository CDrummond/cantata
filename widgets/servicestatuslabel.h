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

#ifndef SERVICE_STATUS_LABEL_H
#define SERVICE_STATUS_LABEL_H

#include <QLabel>

class ServiceStatusLabel : public QLabel
{
    Q_OBJECT

public:
    ServiceStatusLabel(QWidget *p);
    ~ServiceStatusLabel() override { }

    void setText(const QString &txt, const QString &name);
    void setStatus(bool on);

Q_SIGNALS:
    void clicked();

private:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;

private:
    bool pressed;
    QString onTooltip;
    QString offTooltip;
};

#endif
