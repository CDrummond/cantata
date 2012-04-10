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

#include <QtGui/QFormLayout>
#include <QtGui/QLabel>
#include <QtGui/QIcon>
#include "lyricsdialog.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#include <KDE/KIconDialog>
#else
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>
#endif

LyricsDialog::LyricsDialog(const Song &s, QWidget *parent)
    #ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
    #else
    : QDialog(parent)
    #endif
    , prev(s)
{
    QWidget *mw=new QWidget(this);
    QGridLayout *mainLayout=new QGridLayout(mw);
    QWidget *wid = new QWidget(mw);
    QFormLayout *layout = new QFormLayout(wid);
    int row=0;
    #ifdef ENABLE_KDE_SUPPORT
    QLabel *lbl=new QLabel(i18n("If Cantata has failed to find lyrics, or has found the wrong ones, "
                                "use this dialog to enter new search details. For example, the current "
                                "song may actually be a cover-version - if so, then searching for "
                                "lyrics by the original artist might help.\n\nIf this search does find "
                                "new lyrics, these will still be associated with the original song title "
                                "and artist as displayed in Cantata."), mw);
    #else
    QLabel *lbl=new QLabel(tr("If Cantata has failed to find lyrics, or has found the wrong ones, "
                              "use this dialog to enter new search details. For example, the current "
                              "song may actually be a cover-version - if so, then searching for "
                              "lyrics by the original artist might help.\n\nIf this search does find "
                              "new lyrics, these will still be associated with the original song title "
                              "and artist as displayed in Cantata."), mw);
    #endif
    lbl->setWordWrap(true);
    QLabel *icn=new QLabel(mw);
    icn->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    int iconSize=48;
    icn->setMinimumSize(iconSize, iconSize);
    icn->setMaximumSize(iconSize, iconSize);
    icn->setPixmap(QIcon::fromTheme("dialog-information").pixmap(iconSize, iconSize));
    mainLayout->setMargin(0);
    layout->setMargin(0);
    mainLayout->addWidget(icn, 0, 0, 1, 1);
    mainLayout->addWidget(lbl, 0, 1, 1, 1);
    mainLayout->addItem(new QSpacerItem(8, 4, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
    mainLayout->addWidget(wid, 2, 0, 1, 2);
    titleEntry = new LineEdit(wid);
    artistEntry = new LineEdit(wid);

    #ifdef ENABLE_KDE_SUPPORT
    layout->setWidget(row, QFormLayout::LabelRole, new QLabel(i18n("Title:"), wid));
    layout->setWidget(row++, QFormLayout::FieldRole, titleEntry);
    layout->setWidget(row, QFormLayout::LabelRole, new QLabel(i18n("Artist:"), wid));
    layout->setWidget(row++, QFormLayout::FieldRole, artistEntry);
    setMainWidget(mw);
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Search For Lyrics"));
    enableButton(KDialog::Ok, false);
    #else
    layout->setWidget(row, QFormLayout::LabelRole, new QLabel(tr("Title:"), wid));
    layout->setWidget(row++, QFormLayout::FieldRole, titleEntry);
    layout->setWidget(row, QFormLayout::LabelRole, new QLabel(tr("Artist:"), wid));
    layout->setWidget(row++, QFormLayout::FieldRole, artistEntry);
    setWindowTitle(tr("Search For Lyrics"));
    QBoxLayout *dlgLayout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    dlgLayout->addWidget(mw);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
    dlgLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    #endif
    connect(titleEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(artistEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    titleEntry->setFocus();
    titleEntry->setText(s.title);
    artistEntry->setText(s.artist);
}

Song LyricsDialog::song() const
{
    Song s=prev;
    s.artist=artistEntry->text().trimmed();
    s.title=titleEntry->text().trimmed();
    return s;
}

void LyricsDialog::changed()
{
    Song s=song();

    #ifdef ENABLE_KDE_SUPPORT
    enableButton(KDialog::Ok, !s.artist.isEmpty() && !s.title.isEmpty() && (s.artist!=prev.artist || s.title!=prev.title));
    #else
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!s.artist.isEmpty() && !s.title.isEmpty() && (s.artist!=prev.artist || s.title!=prev.title));
    #endif
}
