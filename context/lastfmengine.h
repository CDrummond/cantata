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

#ifndef LASTFM_ENGINE_H
#define LASTFM_ENGINE_H

#include "contextengine.h"
#include <QStringList>

class LastFmEngine : public ContextEngine
{
    Q_OBJECT
    
public:
    static const QLatin1String constLang;
    static const QLatin1String constLinkPlaceholder;

    LastFmEngine(QObject *p);

    QStringList getLangs() const;
    virtual QString getPrefix(const QString &) const { return constLang; }
    QString translateLinks(QString text) const;

public Q_SLOTS:
    void search(const QStringList &query, Mode mode);

private Q_SLOTS:
    void parseResponse();
    
private:
    QString parseArtistResponse(const QByteArray &data);
    QString parseAlbumResponse(const QByteArray &data);
};

#endif
