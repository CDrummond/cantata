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

#include "cddbselectiondialog.h"
#include "localize.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>

CddbSelectionDialog::CddbSelectionDialog(QWidget *parent)
    : Dialog(parent)
{
    QWidget *wid = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(wid);

    combo=new QComboBox(wid);
    layout->addWidget(new QLabel(i18n("Multiple matches were found for the inserted disc. Please choose the relevant one from below:"), wid));
    layout->addWidget(combo);

    setCaption(i18n("Disc Selection"));
    setMainWidget(wid);
    setButtons(Ok);
    resize(400, 100);
}

int CddbSelectionDialog::select(const QList<CddbAlbum> &albums)
{
    combo->clear();
    foreach (const CddbAlbum &a, albums) {
        quint32 totalTime=0;
        foreach (const Song &s, a.tracks) {
            totalTime+=s.time;
        }

        if (a.disc>0) {
            combo->addItem(QString("%1 %2 %3 %4 (%5)").arg(a.artist).arg(a.name).arg(a.year).arg(i18n("Disc %1").arg(a.disc)).arg(Song::formattedTime(totalTime)));
        } else {
            combo->addItem(QString("%1 %2 %3 (%4)").arg(a.artist).arg(a.name).arg(a.year).arg(Song::formattedTime(totalTime)));
        }
    }
    
    exec();
    return combo->currentIndex();
}
