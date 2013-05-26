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

#ifndef SERVERSETTINGS_H
#define SERVERSETTINGS_H

#include "ui_serversettings.h"
#include "mpdconnection.h"

class ServerSettings : public QWidget, private Ui::ServerSettings
{
    Q_OBJECT

public:
    ServerSettings(QWidget *p);
    virtual ~ServerSettings() { }

    void load();
    void save();

Q_SIGNALS:
    void connectTo(const MPDConnectionDetails &details);
    void disconnectFromMpd();

private Q_SLOTS:
    void showDetails(int index);
    void mpdConnectionStateChanged(bool c);
    void toggleConnection();
    void saveAs();
    void remove();

private:
    void setDetails(const MPDConnectionDetails &details);
    MPDConnectionDetails getDetails() const;
    void enableWidgets(bool e);

private:
    bool isCurrentConnection;
};

#endif
