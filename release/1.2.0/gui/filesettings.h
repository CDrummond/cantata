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

#ifndef FILESETTINGS_H
#define FILESETTINGS_H

#include "ui_filesettings.h"

class FileSettings : public QWidget, private Ui::FileSettings
{
    Q_OBJECT

public:
    FileSettings(QWidget *p);
    virtual ~FileSettings() { }

    void load();
    void save();

Q_SIGNALS:
    void reloadStreams();

private Q_SLOTS:
    void streamLocationChanged();
};

#endif
