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

#ifndef STREAMS_SETTINGS_H
#define STREAMS_SETTINGS_H

#include "ui_streamssettings.h"

class StreamsSettings : public QWidget, private Ui::StreamsSettings
{
    Q_OBJECT

public:
    StreamsSettings(QWidget *p);
    virtual ~StreamsSettings() { }

    void load();
    void save();

private Q_SLOTS:
    void currentCategoryChanged(int row);
    void install();
    void remove();

private:
    #ifndef Q_OS_WIN
    QListWidgetItem * get(const QString &name);
    #endif
};

#endif
