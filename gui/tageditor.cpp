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
#include <KDE/KMessageBox>
#else
#include <QtGui/QDialogButtonBox>
#endif
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#endif
#include <QtGui/QMenu>
#include <QtCore/QDir>

static bool equalTags(const Song &a, const Song &b, bool compareCommon)
{
    return (compareCommon || a.track==b.track) && a.year==b.year && a.disc==b.disc &&
           a.artist==b.artist && a.genre==b.genre && a.album==b.album &&
           a.albumartist==b.albumartist && (compareCommon || a.title==b.title);
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
    ButtonCodes buttons=Ok|Cancel|Reset|User3;
    if (songs.count()>1) {
        buttons|=User2|User1;
    }
    setButtons(buttons);

    setCaption(i18n("Tags"));
    if (songs.count()>1) {
        setButtonGuiItem(User2, KStandardGuiItem::back(KStandardGuiItem::UseRTL));
        setButtonText(User2, i18n("Previous"));
        setButtonGuiItem(User1, KStandardGuiItem::forward(KStandardGuiItem::UseRTL));
        setButtonText(User1, i18n("Next"));
        enableButton(User1, false);
        enableButton(User2, false);
    }
    setButtonGuiItem(Ok, KStandardGuiItem::save());
    setButtonGuiItem(User3, KGuiItem(i18n("Tools"), "tools-wizard"));
    QMenu *toolsMenu=new QMenu(this);
    toolsMenu->addAction(i18n("Apply \"Various Artists\" Workaround"), this, SLOT(applyVa()));
    toolsMenu->addAction(i18n("Revert \"Various Artists\" Workaround"), this, SLOT(revertVa()));
    toolsMenu->addAction(i18n("Capitalize"), this, SLOT(capitalise()));
    setButtonMenu(User3, toolsMenu, InstantPopup);
    enableButton(Ok, false);
    enableButton(Reset, false);
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
    trackName->view()->setTextElideMode(Qt::ElideLeft);
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
    connect(album, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(album, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(genre, SIGNAL(activated(int)), SLOT(checkChanged()));
    connect(track, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(disc, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(genre, SIGNAL(editTextChanged(const QString &)), SLOT(checkChanged()));
    connect(year, SIGNAL(valueChanged(int)), SLOT(checkChanged()));
    connect(trackName, SIGNAL(activated(int)), SLOT(setIndex(int)));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));

    if (original.count()>0) {
        saveable=original.at(0).file.startsWith('/') || QDir(Settings::self()->mpdDir()).isReadable();
    } else {
        saveable=false;
    }
}

void TagEditor::fillSong(Song &s, bool isAll, bool skipEmpty) const
{
    Song all=original.at(0);
    bool haveAll=original.count()>1;

    if (!isAll) {
        setString(s.title, title->text().trimmed(), skipEmpty);
    }
    setString(s.artist, artist->text().trimmed(), skipEmpty && (!haveAll || all.artist.isEmpty()));
    setString(s.album, album->text().trimmed(), skipEmpty && (!haveAll || all.album.isEmpty()));
    setString(s.albumartist, albumArtist->text().trimmed(), skipEmpty && (!haveAll || all.albumartist.isEmpty()));
    if (!isAll) {
        s.track=track->value();
    }
    s.disc=disc->value();
    setString(s.genre, genre->text().trimmed(), skipEmpty && (!haveAll || all.genre.isEmpty()));
    s.year=year->value();
}

void TagEditor::enableOkButton()
{
    if (!saveable) {
        return;
    }

    enableButton(Ok, (editedIndexes.count()>1) ||
                     (1==original.count() && 1==editedIndexes.count()) ||
                     (1==editedIndexes.count() && !editedIndexes.contains(0)) );
    enableButton(Reset, isButtonEnabled(Ok));
}

void TagEditor::setLabelStates()
{
    Song o=original.at(currentSongIndex);
    Song e=edited.at(currentSongIndex);
    bool isAll=0==currentSongIndex && original.count()>1;

    titleLabel->setOn(!isAll && o.title!=e.title);
    artistLabel->setOn(o.artist!=e.artist);
    albumArtistLabel->setOn(o.albumartist!=e.albumartist);
    albumLabel->setOn(o.album!=e.album);
    trackLabel->setOn(!isAll && o.track!=e.track);
    discLabel->setOn(o.disc!=e.disc);
    genreLabel->setOn(o.genre!=e.genre);
    yearLabel->setOn(o.year!=e.year);
}

void TagEditor::applyVa()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (KMessageBox::No==KMessageBox::questionYesNo(this, (isAll ? i18n("Apply \"Various Artists\" workaround to <b>all</b> tracks?")
                                                                  : i18n("Apply \"Various Artists\" workaround?"))+
                                                           QLatin1String("<br/><hr/><br/>")+
                                                           i18n("<i>This will set 'Album artist' and 'Artist' to "
                                                                "\"Various Artists\", and set 'Title' to "
                                                                "\"TrackArtist - TrackTitle\"</i>"))) {
        return;
    }

    if (isAll) {
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.fixVariousArtists()) {
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.fixVariousArtists()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
        }
    }
}

void TagEditor::revertVa()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (KMessageBox::No==KMessageBox::questionYesNo(this, (isAll ? i18n("Revert \"Various Artists\" workaround on <b>all</b> tracks?")
                                                                  : i18n("Revert \"Various Artists\" workaround"))+
                                                           QLatin1String("<br/><hr/><br/>")+
                                                           i18n("<i>Where the 'Album artist' is the same as 'Artist' "
                                                                "and the 'Title' is of the format \"TrackArtist - TrackTitle\", "
                                                                "'Artist' will be taken from 'Title' and 'Title' itself will be "
                                                                "set to just the title. e.g. <br/><br/>"
                                                                "If 'Title' is \"Wibble - Wobble\", then 'Artist' will be set to "
                                                                "\"Wibble\" and 'Title' will be set to \"Wobble\"</i>"))) {
        return;
    }

    if (isAll) {
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.revertVariousArtists()) {
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.revertVariousArtists()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
        }
    }
}

void TagEditor::capitalise()
{
    bool isAll=0==currentSongIndex && original.count()>1;

    if (KMessageBox::No==KMessageBox::questionYesNo(this, isAll ? i18n("Capitalize the first letter of 'Title', 'Artist', 'Album artist', and "
                                                                        "'Album' of <b>all</b> tracks?")
                                                                 : i18n("Capitalize the first letter of 'Title', 'Artist', 'Album artist', and "
                                                                        "'Album'"))) {
        return;
    }

    if (isAll) {
        for (int i=0; i<edited.count(); ++i) {
            Song s=edited.at(i);
            if (s.capitalise()) {
                edited.replace(i, s);
                updateEditedStatus(i);
                if (i==currentSongIndex) {
                    setSong(s);
                }
            }
        }
    } else {
        Song s=edited.at(currentSongIndex);
        if (s.capitalise()) {
            edited.replace(currentSongIndex, s);
            updateEditedStatus(currentSongIndex);
            setSong(s);
        }
    }
}

void TagEditor::checkChanged()
{
    if (updating) {
        return;
    }

    bool allWasEdited=editedIndexes.contains(0);

    updateEdited();
    enableOkButton();
    setLabelStates();

    bool allEdited=editedIndexes.contains(0);
    bool isAll=0==currentSongIndex && original.count()>1;

    if (isAll && (allEdited || allWasEdited)) {
        int save=currentSongIndex;
        for (int i=1; i<edited.count(); ++i) {
            currentSongIndex=i;
            updateEdited(true);
        }
        currentSongIndex=save;
    }
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

void TagEditor::updateEditedStatus(int index)
{
    bool isAll=0==index && original.count()>1;
    Song s=edited.at(index);
    if (equalTags(s, original.at(index), isAll)) {
        if (editedIndexes.contains(index)) {
            editedIndexes.remove(index);
            updateTrackName(index, false);
            edited.replace(index, s);
        }
    } else {
        if (!editedIndexes.contains(index)) {
            editedIndexes.insert(index);
            updateTrackName(index, true);
        }
        edited.replace(index, s);
    }
}

void TagEditor::updateEdited(bool isFromAll)
{
    Song s=edited.at(currentSongIndex);
    bool isAll=0==currentSongIndex && original.count()>1;
    fillSong(s, isFromAll || isAll, /*isFromAll*/false);

    if (!isAll && isFromAll && original.count()>1) {
        Song all=original.at(0);
        Song o=original.at(currentSongIndex);
        if (all.artist.isEmpty() && s.artist.isEmpty() && !o.artist.isEmpty()) {
            s.artist=o.artist;
        }
        if (all.albumartist.isEmpty() && s.albumartist.isEmpty() && !o.albumartist.isEmpty()) {
            s.albumartist=o.albumartist;
        }
        if (all.album.isEmpty() && s.album.isEmpty() && !o.album.isEmpty()) {
            s.album=o.album;
        }
        if (all.genre.isEmpty() && s.genre.isEmpty() && !o.genre.isEmpty()) {
            s.genre=o.genre;
        }
    }

    if (equalTags(s, original.at(currentSongIndex), isFromAll || isAll)) {
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
        enableButton(User1, !isMultiple && idx<(original.count()-1)); // Next
        enableButton(User2, !isMultiple && idx>1); // Prev
    }
    enableOkButton();
    trackName->setCurrentIndex(idx);
    setLabelStates();
    updating=false;
}

void TagEditor::applyUpdates()
{
    bool updated=false;
    bool skipFirst=original.count()>1;
    QStringList failed;
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
            switch(Tags::update(orig.file, orig, edit)) {
            case Tags::Update_Modified:
                DevicesModel::self()->updateSong(orig, edit);
                break;
            case Tags::Update_Failed:
                failed.append(orig.file);
                break;
            default:
                break;
            }
        } else
        #endif
        switch(Tags::update(Settings::self()->mpdDir()+orig.file, orig, edit)) {
        case Tags::Update_Modified:
            MusicLibraryModel::self()->removeSongFromList(orig);
            MusicLibraryModel::self()->addSongToList(edit);
            updated=true;
            break;
        case Tags::Update_Failed:
            failed.append(orig.file);
            break;
        default:
            break;
        }
    }

    if (failed.count()) {
        KMessageBox::errorList(this, i18n("Failed to update the tags of the following tracks:"), failed);
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
    case Reset: // Reset
        if (0==currentSongIndex && original.count()>1) {
            for (int i=0; i<original.count(); ++i) {
                edited.replace(i, original.at(i));
                updateTrackName(i, false);
            }
            editedIndexes.clear();
            setSong(original.at(currentSongIndex));
        } else {
            setSong(original.at(currentSongIndex));
        }
        enableOkButton();
        break;
    case User1: // Next
        setIndex(currentSongIndex+1);
        break;
    case User2: // Prev
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
