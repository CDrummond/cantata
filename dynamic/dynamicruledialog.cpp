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

#include "dynamicruledialog.h"
#include "musiclibrarymodel.h"
#include "localize.h"

static const int constMinDate=1800;
static const int constMaxDate=2100;
static const QChar constDateSep('-');

DynamicRuleDialog::DynamicRuleDialog(QWidget *parent)
    : Dialog(parent)
{
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);
    setCaption(i18n("Dynamic Rule"));

    connect(artistText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(similarArtistsText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(albumArtistText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(albumText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(titleText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(genreText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
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

    similarArtistsText->clear();
    similarArtistsText->insertItems(0, strings);

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
    genreText->clear();
    genreText->insertItems(0, strings);

    dateFromSpin->setRange(constMinDate-1, constMaxDate);
    dateToSpin->setRange(constMinDate-1, constMaxDate);
    artistText->setFocus();
    errorLabel->setVisible(false);
    errorLabel->setStyleSheet(QLatin1String("QLabel{color:red;}"));
}

DynamicRuleDialog::~DynamicRuleDialog()
{
}

bool DynamicRuleDialog::edit(const Dynamic::Rule &rule)
{
    typeCombo->setCurrentIndex(QLatin1String("true")==rule[Dynamic::constExcludeKey] ? 1 : 0);
    artistText->setText(rule[Dynamic::constArtistKey]);
    similarArtistsText->setText(rule[Dynamic::constSimilarArtistsKey]);
    albumArtistText->setText(rule[Dynamic::constAlbumArtistKey]);
    albumText->setText(rule[Dynamic::constAlbumKey]);
    titleText->setText(rule[Dynamic::constTitleKey]);
    genreText->setText(rule[Dynamic::constGenreKey]);

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
    exactCheck->setChecked(QLatin1String("false")!=rule[Dynamic::constExactKey]);
    errorLabel->setVisible(false);
    enableOkButton();
    return QDialog::Accepted==exec();
}

Dynamic::Rule DynamicRuleDialog::rule() const
{
    Dynamic::Rule r;
    if (!artist().isEmpty()) {
        r.insert(Dynamic::constArtistKey, artist());
    }
    if (!similarArtists().isEmpty()) {
        r.insert(Dynamic::constSimilarArtistsKey, similarArtists());
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

    if (!exactCheck->isChecked()) {
        r.insert(Dynamic::constExactKey, QLatin1String("false"));
    }
    if (1==typeCombo->currentIndex()) {
        r.insert(Dynamic::constExcludeKey, QLatin1String("true"));
    }
    return r;
}

void DynamicRuleDialog::enableOkButton()
{
    static const int constMaxDateRange=20;

    int dateFrom=dateFromSpin->value();
    int dateTo=dateToSpin->value();
    bool haveFrom=dateFrom>=constMinDate && dateFrom<=constMaxDate;
    bool haveTo=dateTo>=constMinDate && dateTo<=constMaxDate && dateTo!=dateFrom;
    bool enable=(!haveFrom || !haveTo || (dateTo>=dateFrom && (dateTo-dateFrom)<=constMaxDateRange)) &&
                (haveFrom || haveTo || !artist().isEmpty() || !similarArtists().isEmpty() || !albumArtist().isEmpty() ||
                 !album().isEmpty() || !title().isEmpty() || !genre().isEmpty());

    errorLabel->setVisible(false);
    if (!enable && haveFrom && haveTo) {
        if (dateTo<dateFrom) {
            errorLabel->setText(i18n("<i><b>ERROR</b>: 'From Year' should be less than 'To Year'</i>"));
            errorLabel->setVisible(true);
        } else if (dateTo-dateFrom>constMaxDateRange) {
            errorLabel->setText(i18n("<i><b>ERROR:</b> Date range is too large (can only be a maximum of %1 years)</i>").arg(constMaxDateRange));
            errorLabel->setVisible(true);
        }
    }
    enableButton(Ok, enable);
}
