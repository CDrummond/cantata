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

#include "tageditor.h"
#include "tags.h"
#include "musiclibrarymodel.h"
#include "mpdconnection.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KStandardGuiItem>
#else
#include <QtGui/QDialogButtonBox>
#endif
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#endif

static bool equalTags(const Song &a, const Song &b, bool isAllTracks)
{
    return (isAllTracks || a.track==b.track) && a.year==b.year && a.disc==b.disc &&
           a.artist==b.artist && a.genre==b.genre && a.album==b.album &&
           a.albumartist==b.albumartist && (isAllTracks || a.title==b.title);
}

static void setString(QString &str, const QString &v, bool skipEmpty) {
    if (!skipEmpty || !v.isEmpty()) {
        str=v;
    }
}

TagEditor::TagEditor(QWidget *parent, const QList<Song> &songs,
                     const QSet<QString> &existingArtists, const QSet<QString> &existingAlbumArtists,
                     const QSet<QString> &existingAlbums, const QSet<QString> &existingGenres)
#ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
#else
    : QDialog(parent)
#endif
    , currentSongIndex(-1)
    , updating(false)
{
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    #ifdef ENABLE_KDE_SUPPORT
    setMainWidget(mainWidet);
    ButtonCodes buttons=Ok|Cancel|User1;
    if (songs.count()>1) {
        buttons|=User3|User2;
    }
    setButtons(buttons);

    setCaption(i18n("Tags"));
    if (songs.count()>1) {
        setButtonGuiItem(User3, KStandardGuiItem::back(KStandardGuiItem::UseRTL));
        setButtonText(User3, i18n("Previous"));
        setButtonGuiItem(User2, KStandardGuiItem::forward(KStandardGuiItem::UseRTL));
        setButtonText(User2, i18n("Next"));
        enableButton(User2, false);
        enableButton(User3, false);
    }
    setButtonGuiItem(Ok, KStandardGuiItem::save());
    setButtonGuiItem(User1, KStandardGuiItem::reset());
    enableButton(Ok, false);
    enableButton(User1, false);
    #else
    setWindowTitle(tr("Tags"));

    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    layout->addWidget(mainWidet);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Apply,
                                     Qt::Horizontal, this);
    layout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    #endif
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);

    QStringList strings=existingArtists.toList();
    strings.sort();
    artist->clear();
    artist->insertItems(0, strings);

    strings=existingAlbumArtists.toList();
    strings.sort();
    albumArtist->clear();
    albumArtist->insertItems(0, strings);

    strings=existingAlbums.toList();
    strings.sort();
    album->clear();
    album->insertItems(0, strings);

    strings=existingGenres.toList();
    strings.sort();
    genre->clear();
    genre->insertItems(0, strings);

    trackName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    trackName->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    resize(500, 200);

    original=songs;
    if (original.count()>1) {
        QSet<QString> songArtists;
        QSet<QString> songAlbumArtists;
        QSet<QString> songAlbums;
        QSet<QString> songGenres;
        QSet<int> songYears;
        QSet<int> songDiscs;

        foreach (const Song &s, songs) {
            songArtists.insert(s.artist);
            songAlbumArtists.insert(s.albumartist);
            songAlbums.insert(s.album);
            songGenres.insert(s.genre);
            songYears.insert(s.year);
            songDiscs.insert(s.disc);
            if (songArtists.count()>1 && songAlbumArtists.count()>1 && songAlbums.count()>1 && songGenres.count()>1 && songYears.count()>1 && songDiscs.count()>1) {
                break;
            }
        }
        Song all;
        all.file.clear();
        all.title.clear();
        all.track=0;
        all.artist=1==songArtists.count() ? *(songArtists.begin()) : QString();
        all.albumartist=1==songAlbumArtists.count() ? *(songAlbumArtists.begin()) : QString();
        all.album=1==songAlbums.count() ? *(songAlbums.begin()) : QString();
        all.genre=1==songGenres.count() ? *(songGenres.begin()) : QString();
        all.year=1==songYears.count() ? *(songYears.begin()) : 0;
        all.disc=1==songDiscs.count() ? *(songDiscs.begin()) : 0;
        original.prepend(all);
        artist->setFocus();
    } else {
        title->setFocus();
    }
    edited=original;
    setIndex(0);
    bool first=original.count()>1;
    foreach (const Song &s, original) {
        if (first) {
            #ifdef ENABLE_KDE_SUPPORT
            trackName->insertItem(trackName->count(), i18n("All tracks"));
            #else
            trackName->insertItem(trackName->count(), tr("All tracks"));
            #endif
            first=false;
        } else {
            trackName->insertItem(trackName->count(), s.file);
        }
    }
    connect(title, SIGNAL(textChanged(const QString &)), SLOT(checkChanged()));
    connect(artist, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(artist, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(albumArtist, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(albumArtist, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(genre, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(track, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(disc, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(genre, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(year, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(trackName, SIGNAL(activated(int)), SLOT(setIndex(int)));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));
}

void TagEditor::fillSong(Song &s, bool skipEmpty) const
{
    setString(s.title, title->text().trimmed(), skipEmpty);
    setString(s.artist, artist->text().trimmed(), skipEmpty);
    setString(s.albumartist, albumArtist->text().trimmed(), skipEmpty);
    s.track=track->value();
    s.disc=disc->value();
    setString(s.genre, genre->text().trimmed(), skipEmpty);
    s.year=year->value();
}

void TagEditor::enableOkButton()
{
    bool isAll=0==currentSongIndex && original.count()>1;
    bool allEdited=isAll && editedIndexes.contains(0);

    enableButton(Ok, (isAll && allEdited) ||
                     (!isAll && ((allEdited && editedIndexes.count()>1) || (!allEdited && editedIndexes.count()>0))));
    enableButton(User1, isButtonEnabled(Ok));
}

void TagEditor::checkChanged()
{
    if (updating) {
        return;
    }

    updateEdited();
    enableOkButton();
}

void TagEditor::updateTrackName(int index, bool edited)
{
    bool isAll=0==index && original.count()>1;

    if (edited) {
        if (isAll) {
            #ifdef ENABLE_KDE_SUPPORT
            trackName->setItemText(index, i18n("All tracks [modified]"));
            #else
            trackName->setItemText(index, tr("All tracks [modified]"));
            #endif
        } else {
            #ifdef ENABLE_KDE_SUPPORT
            trackName->setItemText(index, i18n("%1 [modified]", original.at(index).file));
            #else
            trackName->setItemText(index, tr("%1 [modified]").arg(original.at(index).file));
            #endif
        }
    } else {
        if (isAll) {
            #ifdef ENABLE_KDE_SUPPORT
            trackName->setItemText(index, i18n("All tracks"));
            #else
            trackName->setItemText(index, tr("All tracks"));
            #endif
        } else {
            trackName->setItemText(index, original.at(index).file);
        }
    }
}

void TagEditor::updateEdited(bool skipEmpty)
{
    Song s=edited.at(currentSongIndex);
    bool isAll=0==currentSongIndex && original.count()>1;
    fillSong(s, skipEmpty);
    if (equalTags(s, original.at(currentSongIndex), isAll)) {
        if (editedIndexes.contains(currentSongIndex)) {
            editedIndexes.remove(currentSongIndex);
            updateTrackName(currentSongIndex, false);
            edited.replace(currentSongIndex, s);
        }
    } else {
        if (!editedIndexes.contains(currentSongIndex)) {
            editedIndexes.insert(currentSongIndex);
            updateTrackName(currentSongIndex, true);
        }
        edited.replace(currentSongIndex, s);
    }
}

void TagEditor::setSong(const Song &s)
{
    title->setText(s.title);
    artist->setText(s.artist);
    albumArtist->setText(s.albumartist);
    album->setText(s.album);
    track->setValue(s.track);
    disc->setValue(s.disc);
    genre->setText(s.genre);
    year->setValue(s.year);
}

void TagEditor::setIndex(int idx)
{
    if (currentSongIndex==idx || idx>(original.count()-1)) {
        return;
    }
    updating=true;

    bool haveMultiple=original.count()>1;

    if (haveMultiple && currentSongIndex>=0) {
        updateEdited();
    }
    Song s=edited.at(!haveMultiple || idx==0 ? 0 : idx);
    setSong(s);
    currentSongIndex=idx;

    bool isMultiple=haveMultiple && 0==idx;
    title->setEnabled(!isMultiple);
    titleLabel->setEnabled(!isMultiple);
    track->setEnabled(!isMultiple);
    trackLabel->setEnabled(!isMultiple);
    if (isMultiple) {
        title->setText(QString());
        track->setValue(0);
    }

    if (original.count()>1) {
        enableButton(User2, !isMultiple && idx<(original.count()-1)); // Next
        enableButton(User3, !isMultiple && idx>1); // Prev
    }
    enableOkButton();
    trackName->setCurrentIndex(idx);
    updating=false;
}

void TagEditor::applyUpdates()
{
    bool allEdited=editedIndexes.contains(0);
    bool isAll=0==currentSongIndex && original.count()>1;

    if (isAll && allEdited) {
        for (int i=1; i<edited.count(); ++i) {
            currentSongIndex=i;
            updateEdited(true);
        }
    }

    bool updated=false;
    bool skipFirst=original.count()>1;
    foreach (int idx, editedIndexes) {
        if (skipFirst && 0==idx) {
            continue;
        }
        Song orig=original.at(idx);
        Song edit=edited.at(idx);
        if (equalTags(orig, edit, false)) {
            continue;
        }

        #ifdef ENABLE_DEVICES_SUPPORT
        if (orig.file.startsWith('/')) {  // Device paths are complete, MPD paths are not :-)
            if (Tags::update(orig.file, orig, edit)) {
                DevicesModel::self()->updateSong(orig, edit);
            }
        } else
        #endif
        if (Tags::update(Settings::self()->mpdDir()+orig.file, orig, edit)) {
            MusicLibraryModel::self()->removeSongFromList(orig);
            MusicLibraryModel::self()->addSongToList(edit);
            updated=true;
        }
    }

    if (updated) {
        MusicLibraryModel::self()->removeCache();
        emit update();
    }
}

#ifdef ENABLE_KDE_SUPPORT
void TagEditor::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        applyUpdates();
        accept();
        break;
    }
    case User1: // Reset
        setSong(original.at(currentSongIndex));
        break;
    case User2: // Next
        setIndex(currentSongIndex+1);
        break;
    case User3: // Prev
        setIndex(currentSongIndex-1);
        break;
    case Cancel:
        reject();
        break;
    default:
        break;
    }

    KDialog::slotButtonClicked(button);
}
#else
void TagEditor::buttonPressed(QAbstractButton *button)
{
//     switch (buttonBox->buttonRole(button)) {
//     case QDialogButtonBox::AcceptRole:
//     case QDialogButtonBox::ApplyRole:
//         writeSettings();
//         break;
//     case QDialogButtonBox::RejectRole:
//         break;
//     default:
//         break;
//     }
}
#endif
