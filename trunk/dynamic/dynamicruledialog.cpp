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

static const int constMinDate=1800;
static const int constMaxDate=2100;
static const QChar constDateSep('-');

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
    connect(dateFromSpin, SIGNAL(valueChanged(int)), SLOT(enableOkButton()));
    connect(dateToSpin, SIGNAL(valueChanged(int)), SLOT(enableOkButton()));

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
    dateFromSpin->setRange(constMinDate-1, constMaxDate);
    dateToSpin->setRange(constMinDate-1, constMaxDate);
}

DynamicRuleDialog::~DynamicRuleDialog()
{
}

bool DynamicRuleDialog::edit(const Dynamic::Rule &rule)
{
    artistText->setText(rule[Dynamic::constArtistKey]);
    albumArtistText->setText(rule[Dynamic::constAlbumArtistKey]);
    albumText->setText(rule[Dynamic::constAlbumKey]);
    titleText->setText(rule[Dynamic::constTitleKey]);
    QString genre=rule[Dynamic::constGenreKey];
    genreCombo->setCurrentIndex(0);
    if (!genre.isEmpty()) {
        for (int i=1; i<genreCombo->count(); ++i) {
            if (genre==genreCombo->itemText(i)) {
                genreCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    QString date=rule[Dynamic::constDateKey];
    int dateFrom=0;
    int dateTo=0;
    if (!date.isEmpty()) {
        int idx=date.indexOf(constDateSep);
        if (-1==idx) {
            dateFrom=date.toInt();
        } else {
            dateFrom=date.left(idx).toInt();
            dateTo=date.mid(idx+1).toInt();
        }
    }

    if (dateFrom<constMinDate || dateFrom>constMaxDate) {
        dateFrom=constMinDate-1;
    }
    if (dateTo<constMinDate || dateTo>constMaxDate) {
        dateTo=constMinDate-1;
    }
    dateFromSpin->setValue(dateFrom);
    dateToSpin->setValue(dateTo);
    return QDialog::Accepted==exec();
}

Dynamic::Rule DynamicRuleDialog::rule() const
{
    Dynamic::Rule r;
    if (!artist().isEmpty()) {
        r.insert(Dynamic::constArtistKey, artist());
    }
    if (!albumArtist().isEmpty()) {
        r.insert(Dynamic::constAlbumArtistKey, albumArtist());
    }
    if (!album().isEmpty()) {
        r.insert(Dynamic::constAlbumKey, album());
    }
    if (!title().isEmpty()) {
        r.insert(Dynamic::constTitleKey, title());
    }
    if (!genre().isEmpty()) {
        r.insert(Dynamic::constGenreKey, genre());
    }
    int dateFrom=dateFromSpin->value();
    int dateTo=dateToSpin->value();
    bool haveFrom=dateFrom>=constMinDate && dateFrom<=constMaxDate;
    bool haveTo=dateTo>=constMinDate && dateTo<=constMaxDate && dateTo!=dateFrom;

    if (haveFrom && haveTo) {
        r.insert(Dynamic::constDateKey, QString::number(dateFrom)+constDateSep+QString::number(dateTo));
    } else if (haveFrom) {
        r.insert(Dynamic::constDateKey, QString::number(dateFrom));
    } else if (haveTo) {
        r.insert(Dynamic::constDateKey, QString::number(dateTo));
    }
    return r;
}

void DynamicRuleDialog::enableOkButton()
{
    int dateFrom=dateFromSpin->value();
    int dateTo=dateToSpin->value();
    bool haveFrom=dateFrom>=constMinDate && dateFrom<=constMaxDate;
    bool haveTo=dateTo>=constMinDate && dateTo<=constMaxDate && dateTo!=dateFrom;
    bool enable=(!haveFrom || !haveTo || haveTo>=haveFrom) &&
                (haveFrom || haveTo || !artist().isEmpty() || !albumArtist().isEmpty() || !album().isEmpty() || !title().isEmpty() || !genre().isEmpty());
    #ifdef ENABLE_KDE_SUPPORT
    enableButton(Ok, enable);
    #else
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
    #endif
}
