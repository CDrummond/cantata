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
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "dynamicruledialog.h"
#include "musiclibrarymodel.h"

DynamicRuleDialog::DynamicRuleDialog(QWidget *parent)
    #ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
    #else
    : QDialog(parent)
    #endif
{
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    #ifdef ENABLE_KDE_SUPPORT
    setMainWidget(mainWidet);
    setButtons(Ok|Cancel);
    setCaption(i18n("Dynamic Rule"));
    enableButton(Ok, false);
    #else
    setWindowTitle(tr("Dynamic Rule"));

    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    layout->addWidget(mainWidet);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
    layout->addWidget(buttonBox);
    #endif
    connect(artistText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(albumArtistText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(albumText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(titleText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), SLOT(enableOkButton()));

    QSet<QString> artists;
    QSet<QString> albumArtists;
    QSet<QString> albums;
    QSet<QString> genres;
    MusicLibraryModel::self()->getDetails(artists, albumArtists, albums, genres);

    QStringList strings=artists.toList();
    strings.sort();
    artistText->clear();
    artistText->insertItems(0, strings);

    strings=albumArtists.toList();
    strings.sort();
    albumArtistText->clear();
    albumArtistText->insertItems(0, strings);

    strings=albums.toList();
    strings.sort();
    albumText->clear();
    albumText->insertItems(0, strings);

    strings=genres.toList();
    strings.sort();
    genreCombo->clear();
    #ifdef ENABLE_KDE_SUPPORT
    strings.prepend(i18n("All Genres"));
    #else
    strings.prepend(tr("All Genres"));
    #endif
    genreCombo->insertItems(0, strings);
}

DynamicRuleDialog::~DynamicRuleDialog()
{
}

bool DynamicRuleDialog::edit(const Dynamic::Rule &rule)
{
    artistText->setText(rule["Artist"]);
    albumArtistText->setText(rule["AlbumArtist"]);
    albumText->setText(rule["Album"]);
    titleText->setText(rule["Title"]);
    QString genre=rule["Genre"];
    genreCombo->setCurrentIndex(0);
    if (!genre.isEmpty()) {
        for (int i=1; i<genreCombo->count(); ++i) {
            if (genre==genreCombo->itemText(i)) {
                genreCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    return QDialog::Accepted==exec();
}

Dynamic::Rule DynamicRuleDialog::rule() const
{
    Dynamic::Rule r;
    if (!artist().isEmpty()) {
        r.insert("Artist", artist());
    }
    if (!albumArtist().isEmpty()) {
        r.insert("AlbumArtist", albumArtist());
    }
    if (!album().isEmpty()) {
        r.insert("Album", album());
    }
    if (!title().isEmpty()) {
        r.insert("Title", title());
    }
    if (!genre().isEmpty()) {
        r.insert("Genre", genre());
    }
    return r;
}

void DynamicRuleDialog::enableOkButton()
{
    bool enable=!artist().isEmpty() || !albumArtist().isEmpty() || !album().isEmpty() || !title().isEmpty() || !genre().isEmpty();
    #ifdef ENABLE_KDE_SUPPORT
    enableButton(Ok, enable);
    #else
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
    #endif
}
