/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef DIRVIEWITEMFILE_H
#define DIRVIEWITEMFILE_H

#include "dirviewitem.h"
#include "mpd/mpdconnection.h"

class DirViewItemFile : public DirViewItem
{
public:
    enum FileType {
        Audio,
        Playlist,
        CueSheet
    };

    DirViewItemFile(const QString &name, const QString &p, DirViewItem *parent) : DirViewItem(name, parent), path(p) {
        fType=MPDConnection::isPlaylist(name)
                ? name.endsWith(QLatin1String(".cue"), Qt::CaseInsensitive)
                    ? CueSheet
                    : Playlist
                : Audio;
    }
    virtual ~DirViewItemFile() { }
    Type type() const { return Type_File;  }
    FileType fileType() const { return fType; }
    QString fullName() const { return path.isEmpty() ? DirViewItem::fullName() : path; }
    const QString & filePath() const { return path; }

private:
    FileType fType;
    QString path;
};

#endif
