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

#ifndef MAGNATUNE_SERVICE_H
#define MAGNATUNE_SERVICE_H

#include "models/sqllibrarymodel.h"
#include "onlinedbservice.h"
#include <QMap>

class MagnatuneXmlParser : public OnlineXmlParser
{
public:
    int parse(QXmlStreamReader &xml) override;
private:
    Song parseSong(QXmlStreamReader &xml);
private:
    QSet<QString> artists;
    QSet<QString> albumUrls;
};

class MagnatuneService : public OnlineDbService
{
    Q_OBJECT
public:
    enum MemberShip {
        MB_None,
        MB_Streaming,
        // MB_Download, // TODO: Magnatune downloads!

        MB_Count
    };

    enum DownloadType {
        DL_Mp3,
        DL_Mp3Vbr,
        DL_Ogg,
        DL_Flac,
        DL_Wav,

        DL_Count
    };

    static QString membershipStr(MemberShip f, bool trans=false);
    static QString downloadTypeStr(DownloadType f, bool trans=false);

    MagnatuneService(QObject *p);
    QVariant data(const QModelIndex &index, int role) const override;
    QString name() const override;
    QString title() const override;
    QString descr() const override;
    OnlineXmlParser * createParser() override;
    QUrl listingUrl() const override;
    void configure(QWidget *p) override;
    int averageSize() const override { return 10; }
private:
    Song & fixPath(Song &s) const override;

private:
    MemberShip membership;
    DownloadType download;
    QString username;
    QString password;
};

#endif
