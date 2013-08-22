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
 * General Public License for more detailexampleSong.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "filenameschemedialog.h"
#include "song.h"
#include "localize.h"
#include "messagebox.h"

FilenameSchemeDialog::FilenameSchemeDialog(QWidget *parent)
    : Dialog(parent, "FilenameSchemeDialog")
{
    setButtons(Ok|Cancel);
    setCaption(i18n("Filename Scheme"));
    setWindowModality(Qt::WindowModal);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    connect(pattern, SIGNAL(textChanged(const QString &)), this, SLOT(enableOkButton()));
    connect(pattern, SIGNAL(textChanged(const QString &)), this, SLOT(updateExample()));
    connect(help, SIGNAL(leftClickedUrl()), this, SLOT(showHelp()));
    connect(albumArtist, SIGNAL(clicked()), this, SLOT(insertAlbumArtist()));
    connect(composer, SIGNAL(clicked()), this, SLOT(insertComposer()));
    connect(albumTitle, SIGNAL(clicked()), this, SLOT(insertAlbumTitle()));
    connect(trackArtist, SIGNAL(clicked()), this, SLOT(insertTrackArtist()));
    connect(trackTitle, SIGNAL(clicked()), this, SLOT(insertTrackTitle()));
    connect(trackArtistAndTitle, SIGNAL(clicked()), this, SLOT(insertTrackArtistAndTitle()));
    connect(trackNo, SIGNAL(clicked()), this, SLOT(insertTrackNumber()));
    connect(cdNo, SIGNAL(clicked()), this, SLOT(insertCdNumber()));
    connect(genre, SIGNAL(clicked()), this, SLOT(insertGenre()));
    connect(year, SIGNAL(clicked()), this, SLOT(insertYear()));

    exampleSong.albumartist=i18nc("Example album artist", "Various Artists");
    exampleSong.artist=i18nc("Example artist", "Wibble");
    exampleSong.composer=i18nc("Example composer", "Vivaldi");
    exampleSong.album=i18nc("Example album", "Now 5001");
    exampleSong.title=i18nc("Example song name", "Wobble");
    exampleSong.genre=i18nc("Example genre", "Dance");
    exampleSong.track=1;
    exampleSong.disc=2;
    exampleSong.year=2001;
    exampleSong.file="wibble.mp3";
}

void FilenameSchemeDialog::show(const DeviceOptions &opts)
{
    origOpts=opts;
    pattern->setText(opts.scheme);
    example->setText(origOpts.createFilename(exampleSong));
    Dialog::show();
    enableButtonOk(false);
}

void FilenameSchemeDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        emit scheme(pattern->text().trimmed());
        break;
    case Cancel:
        reject();
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}

static QString stripAccelerator(QString str)
{
    return str.replace("&&", "____").replace("&", "").replace("____", "&");
}

void FilenameSchemeDialog::showHelp()
{
    MessageBox::information(this,
                           i18n("<p>The following variables will be replaced with their corresponding meaning for each track name.</p>")+
                           QLatin1String("<p><table border=\"1\">")+
                           i18n("<tr><th><em>Button</em></th><th><em>Variable</em></th><th><em>Description</em></th></tr>")+
                           i18n("<tr><td>%albumartist%</td><td>%1</td><td>The artist of the album. For most albums, this will be the same as the <i>Track Artist.</i> "
                                "For compilations, this will often be <i>Various Artists.</i> </td></tr>", stripAccelerator(albumArtist->text()))+
                           i18n("<tr><td>%album%</td><td>%1</td><td>The name of the album.</td></tr>", stripAccelerator(albumTitle->text()))+
                           i18n("<tr><td>%composer%</td><td>%1</td><td>The composer.</td></tr>", stripAccelerator(composer->text()))+
                           i18n("<tr><td>%artist%</td><td>%1</td><td>The artist of each track.</td></tr>", stripAccelerator(trackArtist->text()))+
                           i18n("<tr><td>%title%</td><td>%1</td><td>The track title (without <i>Track Artist</i>).</td></tr>", stripAccelerator(trackTitle->text()))+
                           i18n("<tr><td>%artistandtitle%</td><td>%1</td><td>The track title (with <i>Track Artist</i>, if different to <i>Album Artist</i>).</td></tr>", stripAccelerator(trackArtistAndTitle->text()))+
                           i18n("<tr><td>%track%</td><td>%1</td><td>The track number.</td></tr>", stripAccelerator(trackNo->text()))+
                           i18n("<tr><td>%discnumber%</td><td>%1</td><td>The album number of a multi-album album. Often compilations consist of several albums.</td></tr>", stripAccelerator(cdNo->text()))+
                           i18n("<tr><td>%year%</td><td>%1</td><td>The year of the album's release.</td></tr>", stripAccelerator(year->text()))+
                           i18n("<tr><td>%genre%</td><td>%1</td><td>The genre of the album.</td></tr>", stripAccelerator(genre->text()))+
                           QLatin1String("</table></p>"));
}

void FilenameSchemeDialog::enableOkButton()
{
    QString scheme=pattern->text().trimmed();
    enableButtonOk(!scheme.isEmpty() && scheme!=origOpts.scheme);
}

void FilenameSchemeDialog::insertAlbumArtist()
{
    insert(DeviceOptions::constAlbumArtist);
}

void FilenameSchemeDialog::insertComposer()
{
    insert(DeviceOptions::constComposer);
}

void FilenameSchemeDialog::insertAlbumTitle()
{
    insert(DeviceOptions::constAlbumTitle);
}

void FilenameSchemeDialog::insertTrackArtist()
{
    insert(DeviceOptions::constTrackArtist);
}

void FilenameSchemeDialog::insertTrackTitle()
{
    insert(DeviceOptions::constTrackTitle);
}

void FilenameSchemeDialog::insertTrackArtistAndTitle()
{
    insert(DeviceOptions::constTrackArtistAndTitle);
}

void FilenameSchemeDialog::insertTrackNumber()
{
    insert(DeviceOptions::constTrackNumber);
}

void FilenameSchemeDialog::insertCdNumber()
{
    insert(DeviceOptions::constCdNumber);
}

void FilenameSchemeDialog::insertGenre()
{
    insert(DeviceOptions::constGenre);
}

void FilenameSchemeDialog::insertYear()
{
    insert(DeviceOptions::constYear);
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
