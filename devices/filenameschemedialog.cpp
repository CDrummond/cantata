/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
 * General Public License for more detailexampleSong.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "filenameschemedialog.h"
#include "song.h"
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <QtGui/QWhatsThis>

FilenameSchemeDialog::FilenameSchemeDialog(QWidget *parent)
    : KDialog(parent)
{
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Filename Scheme"));
    setWindowModality(Qt::WindowModal);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    connect(pattern, SIGNAL(textChanged(const QString &)), this, SLOT(enableOkButton()));
    connect(pattern, SIGNAL(textChanged(const QString &)), this, SLOT(updateExample()));
    connect(help, SIGNAL(leftClickedUrl()), this, SLOT(showHelp()));
    connect(albumArtist, SIGNAL(clicked()), this, SLOT(insertAlbumArtist()));
    connect(albumTitle, SIGNAL(clicked()), this, SLOT(insertAlbumTitle()));
    connect(trackArtist, SIGNAL(clicked()), this, SLOT(insertTrackArtist()));
    connect(trackTitle, SIGNAL(clicked()), this, SLOT(insertTrackTitle()));
    connect(trackNo, SIGNAL(clicked()), this, SLOT(insertTrackNumber()));
    connect(cdNo, SIGNAL(clicked()), this, SLOT(insertCdNumber()));
    connect(genre, SIGNAL(clicked()), this, SLOT(insertGenre()));
    connect(year, SIGNAL(clicked()), this, SLOT(insertYear()));

    exampleSong.albumartist=i18nc("Example album artist", "Various Artists");
    exampleSong.artist=i18nc("Example artist", "Wibble");
    exampleSong.album=i18nc("Example album", "Now 5001");
    exampleSong.title=i18nc("Example song name", "Wobble");
    exampleSong.track=1;
    exampleSong.disc=2;
    exampleSong.year=2001;
    exampleSong.genre=i18nc("Example genre", "Dance");
    exampleSong.file="wibble.mp3";
}

void FilenameSchemeDialog::show(const Device::Options &opts)
{
    origOpts=opts;
    pattern->setText(opts.scheme);
    example->setText(origOpts.createFilename(exampleSong));
    KDialog::show();
    enableButtonOk(false);
}

void FilenameSchemeDialog::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok:
        emit scheme(pattern->text().trimmed());
        break;
    case KDialog::Cancel:
        reject();
        break;
    default:
        break;
    }

    if (KDialog::Ok==button) {
        accept();
    }

    KDialog::slotButtonClicked(button);
}

void FilenameSchemeDialog::showHelp()
{
  QWhatsThis::showText(help->mapToGlobal(help->geometry().topLeft()),
                          i18n("<p>The following variables will be replaced with their corresponding meaning for each track name.</p>"
                               "<p><table border=\"1\">"
                               "<tr><th><em>Variable</em></th><th><em>Description</em></th></tr>"
                               "<tr><td>%1</td><td>The artist of the album. For most albums, this will be the same as the <i>Track Artist.</i> "
                               "For compilations, this will often be <i>Various Artists.</i> </td></tr>"
                               "<tr><td>%2</td><td>The name of the album.</td></tr>"
                               "<tr><td>%3</td><td>The artist of each track.</td></tr>"
                               "<tr><td>%4</td><td>The track title.</td></tr>"
                               "<tr><td>%5</td><td>The track number.</td></tr>"
                               "<tr><td>%6</td><td>The album number of a multi-album album. Often compilations consist of several albums.</td></tr>"
                               "<tr><td>%7</td><td>The year of the album's release.</td></tr>"
                               "<tr><td>%8</td><td>The genre of the album.</td></tr>"
                               "</table></p>", Device::Options::constAlbumArtist, Device::Options::constAlbumTitle,
                               Device::Options::constTrackArtist, Device::Options::constTrackTitle,
                               Device::Options::constTrackNumber, Device::Options::constCdNumber, Device::Options::constYear,
                               Device::Options::constGenre), help);
}

void FilenameSchemeDialog::enableOkButton()
{
    QString scheme=pattern->text().trimmed();
    enableButtonOk(!scheme.isEmpty() && scheme!=origOpts.scheme);
}

void FilenameSchemeDialog::insertAlbumArtist()
{
    insert(Device::Options::constAlbumArtist);
}

void FilenameSchemeDialog::insertAlbumTitle()
{
    insert(Device::Options::constAlbumTitle);
}

void FilenameSchemeDialog::insertTrackArtist()
{
    insert(Device::Options::constTrackArtist);
}

void FilenameSchemeDialog::insertTrackTitle()
{
    insert(Device::Options::constTrackTitle);
}

void FilenameSchemeDialog::insertTrackNumber()
{
    insert(Device::Options::constTrackNumber);
}

void FilenameSchemeDialog::insertCdNumber()
{
    insert(Device::Options::constCdNumber);
}

void FilenameSchemeDialog::insertGenre()
{
    insert(Device::Options::constGenre);
}

void FilenameSchemeDialog::insertYear()
{
    insert(Device::Options::constYear);
}

void FilenameSchemeDialog::insert(const QString &str)
{
    QString text(pattern->text());
    text.insert(pattern->cursorPosition(), str);
    pattern->setText(text);
    updateExample();
}

void FilenameSchemeDialog::updateExample()
{
    QString saveScheme=origOpts.scheme;
    origOpts.scheme=pattern->text();
    example->setText(origOpts.createFilename(exampleSong)+QLatin1String(".mp3"));
    origOpts.scheme=saveScheme;
}
