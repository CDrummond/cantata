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

#include "songdialog.h"
#include "support/messagebox.h"
#include <QFile>

#include <QDebug>
static bool debugEnabled=false;
#define DBUG if (debugEnabled) qWarning() << metaObject()->className() << __FUNCTION__
void SongDialog::enableDebug()
{
    debugEnabled=true;
}

static const int constNumToCheck=10;

bool SongDialog::songsOk(const QList<Song> &songs, const QString &base, bool isMpd)
{
    QWidget *wid=isVisible() ? this : parentWidget();
    int checked=0;
    for (const Song &s: songs) {
        if (!s.isLocalFile()) {
            const QString sfile=s.filePath();
            const QString file=s.filePath(base);
            DBUG << "Checking dir:" << base << " song:" << sfile << " file:" << file;
            if (!QFile::exists(file)) {
                DBUG << QString(file) << "does not exist";
                if (isMpd) {
                    MessageBox::error(wid, tr("Cannot access song files!\n\n"
                                              "Please check Cantata's \"Music folder\" setting, and MPD's \"music_directory\" setting."));
                } else {
                    MessageBox::error(wid, tr("Cannot access song files!\n\n"
                                              "Please check that the device is still attached."));
                }
                deleteLater();
                return false;
            }
        }
        if (++checked>constNumToCheck) {
            break;
        }
    }
    DBUG << "Checked" << checked << "files";

    return true;
}

#include "moc_songdialog.cpp"
