/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

static const char * constVariableProperty="cantata-var";
FilenameSchemeDialog::FilenameSchemeDialog(QWidget *parent)
    : Dialog(parent)
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
    connect(albumArtist, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(composer, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(albumTitle, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(trackArtist, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(trackTitle, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(trackArtistAndTitle, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(trackNo, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(cdNo, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(genre, SIGNAL(clicked()), this, SLOT(insertVariable()));
    connect(year, SIGNAL(clicked()), this, SLOT(insertVariable()));
    albumArtist->setProperty(constVariableProperty, DeviceOptions::constAlbumArtist);
    albumTitle->setProperty(constVariableProperty, DeviceOptions::constAlbumTitle);
    composer->setProperty(constVariableProperty, DeviceOptions::constComposer);
    trackArtist->setProperty(constVariableProperty, DeviceOptions::constTrackArtist);
    trackTitle->setProperty(constVariableProperty, DeviceOptions::constTrackTitle);
    trackArtistAndTitle->setProperty(constVariableProperty, DeviceOptions::constTrackArtistAndTitle);
    trackNo->setProperty(constVariableProperty, DeviceOptions::constTrackNumber);
    cdNo->setProperty(constVariableProperty, DeviceOptions::constCdNumber);
    year->setProperty(constVariableProperty, DeviceOptions::constYear);
    genre->setProperty(constVariableProperty, DeviceOptions::constGenre);
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
    adjustSize();
    setMaximumHeight(height());
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

static QString tableEntry(const QAbstractButton *widget)
{
    return QLatin1String("<tr><td>")+widget->property(constVariableProperty).toString()+
           QLatin1String("</td><td>")+stripAccelerator(widget->text())+
           QLatin1String("</td><td>")+widget->toolTip()+
           QLatin1String("</td></tr>");
}

void FilenameSchemeDialog::showHelp()
{
    MessageBox::information(this,
                           i18n("<p>The following variables will be replaced with their corresponding meaning for each track name.</p>")+
                           QLatin1String("<p><table border=\"1\">")+
                           i18n("<tr><th><em>Variable</em></th><th><em>Button</em></th><th><em>Description</em></th></tr>")+
                           tableEntry(albumArtist)+tableEntry(albumTitle)+tableEntry(composer)+tableEntry(trackArtist)+
                           tableEntry(trackTitle)+tableEntry(trackArtistAndTitle)+tableEntry(trackNo)+tableEntry(cdNo)+
                           tableEntry(year)+tableEntry(genre)+
                           QLatin1String("</table></p>"));
}

void FilenameSchemeDialog::enableOkButton()
{
    QString scheme=pattern->text().trimmed();
    enableButtonOk(!scheme.isEmpty() && scheme!=origOpts.scheme);
}

void FilenameSchemeDialog::insertVariable()
{
    QString text(pattern->text());
    text.insert(pattern->cursorPosition(), sender()->property(constVariableProperty).toString());
    pattern->setText(text);
    updateExample();
}

void FilenameSchemeDialog::updateExample()
{
    QString saveScheme=origOpts.scheme;
    origOpts.scheme=pattern->text();
    QString exampleStr=origOpts.createFilename(exampleSong);
    if (!exampleStr.endsWith(QLatin1String(".mp3"))) {
        exampleStr+=QLatin1String(".mp3");
    }
    example->setText(exampleStr);
    origOpts.scheme=saveScheme;
}
