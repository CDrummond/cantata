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
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QTreeWidget>

CddbSelectionDialog::CddbSelectionDialog(QWidget *parent)
    : Dialog(parent)
{
    QWidget *wid = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(wid);

    combo=new QComboBox(wid);
    QLabel *label=new QLabel(i18n("Multiple matches were found. "
                                  "Please choose the relevant one from below:"), wid);

    tracks = new QTreeWidget(wid);
    tracks->setAlternatingRowColors(true);
    tracks->setRootIsDecorated(false);
    tracks->setUniformRowHeights(true);
    tracks->setItemsExpandable(false);
    tracks->setAllColumnsShowFocus(true);
    tracks->setHeaderLabels(QStringList() << i18n("Artist") << i18n("Title"));
    tracks->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addWidget(tracks);

    setCaption(i18n("Disc Selection"));
    setMainWidget(wid);
    setButtons(Ok);
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(updateTracks()));
}

int CddbSelectionDialog::select(const QList<CdAlbum> &albums)
{
    combo->clear();
    albumDetails=albums;
    foreach (const CdAlbum &a, albums) {
        if (a.disc>0) {
            combo->addItem(QString("%1 -%2 %3 (%4)").arg(a.artist).arg(a.name).arg(i18n("Disc %1").arg(a.disc)).arg(a.year));
        } else {
            combo->addItem(QString("%1 - %2 (%3)").arg(a.artist).arg(a.name).arg(a.year));
        }
    }

    updateTracks();
    exec();
    return combo->currentIndex();
}

void CddbSelectionDialog::updateTracks()
{
    tracks->clear();
    bool sameArtist=true;
    const CdAlbum &a=albumDetails.at(combo->currentIndex());
    foreach (const Song &s, a.tracks) {
        new QTreeWidgetItem(tracks, QStringList() << s.artist << s.title);
        if (s.artist!=a.artist) {
            sameArtist=false;
        }
    }
    tracks->setColumnHidden(0, sameArtist);
}
