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

#ifndef SERVERSETTINGS_H
#define SERVERSETTINGS_H

#include "ui_serversettings.h"
#include "mpd-interface/mpdconnection.h"
#include "devices/deviceoptions.h"

#include <QPushButton>

class ServerSettings : public QWidget, private Ui::ServerSettings
{
    Q_OBJECT

    struct Collection {
        Collection(const MPDConnectionDetails &d=MPDConnectionDetails(), const DeviceOptions &n=DeviceOptions())
            : details(d), namingOpts(n) { }
        MPDConnectionDetails details;
        DeviceOptions namingOpts;
    };

public:
    ServerSettings(QWidget *p);
    ~ServerSettings() override { }

    void load();
    void save();
    void cancel();

private Q_SLOTS:
    void showDetails(int index);
    void add();
    void remove();
    void nameChanged();
    void basicDirChanged();
    #ifdef AVAHI_FOUND
    void detectMPDs();
    void adoptServerSettings(QString ip, QString port);
    #endif

private:
    void setDetails(const MPDConnectionDetails &details);
    MPDConnectionDetails getDetails() const;
    QString generateName(int ignore=-1) const;

private:
    QList<Collection> collections;
    Collection prevBasic;
    bool haveBasicCollection;
    bool isCurrentConnection;
    bool allOptions;
    int prevIndex;
    #ifdef AVAHI_FOUND
    QPushButton *discoveryButton;
    #endif
};

#endif
