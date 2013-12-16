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

#ifndef INTERFACESETTINGS_H
#define INTERFACESETTINGS_H

#include "ui_interfacesettings.h"

class QStringList;

class InterfaceSettings : public QWidget, private Ui::InterfaceSettings
{
    Q_OBJECT

    enum Views
    {
        V_Artists    = 0x01,
        V_Albums     = 0x02,
        V_Folders    = 0x04,
        V_Playlists  = 0x08,
        // V_Dynamic    = 0x10,
        V_Streams    = 0x20,
        V_Online     = 0x40,
        V_Devices    = 0x80,

        V_All        = 0xFF
    };

public:
    InterfaceSettings(QWidget *p, const QStringList &hiddenPages);
    virtual ~InterfaceSettings() { }

    void load();
    void save();

private Q_SLOTS:
    void libraryViewChanged();
    void libraryCoverSizeChanged();
    void albumsViewChanged();
    void albumsCoverSizeChanged();
    void playListsStyleChanged();
    void playQueueGroupedChanged();
    void forceSingleClickChanged();
    void enableStartupState();

private:
    int enabledViews;
};

#endif
