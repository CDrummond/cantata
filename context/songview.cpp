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

#include "songview.h"
#include "lyricsdialog.h"
#include "ultimatelyricsprovider.h"
#include "ultimatelyrics.h"
#include "settings.h"
#include "squeezedtextlabel.h"
#include "utils.h"
#include "messagebox.h"
#include "localize.h"
#ifdef TAGLIB_FOUND
#include "tags.h"
#endif
#include "icons.h"
#include "utils.h"
#include "action.h"
#include "actioncollection.h"
#include "networkaccessmanager.h"
#include "textbrowser.h"
#include "stdactions.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTextStream>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

const QLatin1String SongView::constLyricsDir("lyrics/");
const QLatin1String SongView::constExtension(".lyrics");

static QString cacheFile(QString artist, QString title, bool createDir=false)
{
    title.replace("/", "_");
    artist.replace("/", "_");
    return QDir::toNativeSeparators(Utils::cacheDir(SongView::constLyricsDir+artist+'/', createDir))+title+SongView::constExtension;
}

static inline QString fixNewLines(const QString &o)
{
    return QString(o).replace(QLatin1String("\n\n\n"), QLatin1String("\n\n")).replace("\n", "<br/>");
}

SongView::SongView(QWidget *p)
    : View(p)
    , currentProvider(-1)
    , currentRequest(0)
    , mode(Mode_Display)
    , job(0)
{
    refreshAction = ActionCollection::get()->createAction("refreshlyrics", i18n("Refresh Lyrics"), "view-refresh");
    editAction = ActionCollection::get()->createAction("editlyrics", i18n("Edit Lyrics"), Icons::editIcon);
    saveAction = ActionCollection::get()->createAction("savelyrics", i18n("Save Lyrics"), "document-save");
    cancelAction = ActionCollection::get()->createAction("canceleditlyrics", i18n("Cancel Editing Lyrics"), Icons::cancelIcon);
    delAction = ActionCollection::get()->createAction("dellyrics", i18n("Delete Lyrics File"), "edit-delete");

    connect(refreshAction, SIGNAL(triggered()), SLOT(update()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(saveAction, SIGNAL(triggered()), SLOT(save()));
    connect(cancelAction, SIGNAL(triggered()), SLOT(cancel()));
    connect(delAction, SIGNAL(triggered()), SLOT(del()));
    connect(UltimateLyrics::self(), SIGNAL(lyricsReady(int, QString)), SLOT(lyricsReady(int, QString)));

    text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(text, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    setMode(Mode_Blank);
    setStandardHeader(i18n("Lyrics"));
    clear();
}

SongView::~SongView()
{
    UltimateLyrics::self()->release();
}

void SongView::update()
{
    QString mpdName=mpdFileName();
    bool mpdExists=!mpdName.isEmpty() && QFile::exists(mpdName);
    if (Mode_Edit==mode && MessageBox::No==MessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"), i18n("Abort Editing"),
                                                                    GuiItem(i18n("Abort")), StdGuiItem::cont())) {
        return;
    } else if (mpdExists && MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete saved copy of lyrics, and re-download?"), i18n("Re-download"),
                                                                     GuiItem(i18n("Re-download")), StdGuiItem::cancel())) {
        return;
    }
    if (mpdExists) {
        QFile::remove(mpdName);
    }
    QString cacheName=cacheFileName();
    if (!cacheName.isEmpty() && QFile::exists(cacheName)) {
        QFile::remove(cacheName);
    }
    update(currentSong, true);
}

void SongView::search()
{
    if (Mode_Edit==mode && MessageBox::No==MessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"), i18n("Abort Editing"),
                                                                    GuiItem(i18n("Abort")), StdGuiItem::cont())) {
        return;
    }
    setMode(Mode_Display);

    Song song=currentSong;
    LyricsDialog dlg(currentSong, this);
    if (QDialog::Accepted==dlg.exec()) {
        if ((song.artist!=currentSong.artist || song.title!=currentSong.title) &&
                MessageBox::No==MessageBox::warningYesNo(this, i18n("Current playing song has changed, still perform search?"), i18n("Song Changed"),
                                                         GuiItem(i18n("Perform Search")), StdGuiItem::cancel())) {
            return;
        }
        QString mpdName=mpdFileName();
        if (!mpdName.isEmpty() && QFile::exists(mpdName)) {
            QFile::remove(mpdName);
        }
        QString cacheName=cacheFileName();
        if (!cacheName.isEmpty() && QFile::exists(cacheName)) {
            QFile::remove(cacheName);
        }
        update(dlg.song(), true);
    }
}

void SongView::edit()
{
    preEdit=text->toPlainText();
    setMode(Mode_Edit);
}

void SongView::save()
{
    if (preEdit!=text->toPlainText()) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Save updated lyrics?"), i18n("Save"),
                                                     StdGuiItem::save(), StdGuiItem::discard())) {
            return;
        }

        QString mpdName=mpdFileName();
        QString cacheName=cacheFileName();
        if (!mpdName.isEmpty() && saveFile(mpdName)) {
            if (QFile::exists(cacheName)) {
                QFile::remove(cacheName);
            }
            Utils::setFilePerms(mpdName);
        } else if (cacheName.isEmpty() || !saveFile(cacheName)) {
            MessageBox::error(this, i18n("Failed to save lyrics."));
            return;
        }
    }

    setMode(Mode_Display);
}

void SongView::cancel()
{
    if (preEdit!=text->toPlainText()) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"), i18n("Abort Editing"),
                                                     GuiItem(i18n("Abort")), StdGuiItem::cont())) {
            return;
        }
    }
    text->setText(preEdit);
    setMode(Mode_Display);
}

void SongView::del()
{
    if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete lyrics file?"), i18n("Cancel"),
                                                 StdGuiItem::del(), StdGuiItem::discard())) {
        return;
    }

    QString mpdName=mpdFileName();
    QString cacheName=cacheFileName();
    if (!cacheName.isEmpty() && QFile::exists(cacheName)) {
        QFile::remove(cacheName);
    }
    if (!mpdName.isEmpty() && QFile::exists(mpdName)) {
        QFile::remove(mpdName);
    }
}

void SongView::showContextMenu(const QPoint &pos)
{
   QMenu *menu = text->createStandardContextMenu();
   switch (mode) {
   case Mode_Blank:
       break;
   case Mode_Display:
       menu->addSeparator();
       menu->addAction(refreshAction);
       menu->addAction(StdActions::self()->searchAction);
       menu->addSeparator();
       menu->addAction(editAction);
       menu->addAction(delAction);
       break;
   case Mode_Edit:
       menu->addSeparator();
       menu->addAction(saveAction);
       menu->addAction(cancelAction);
       break;
   }

   menu->exec(text->mapToGlobal(pos));
   delete menu;
}

void SongView::update(const Song &s, bool force)
{
    if (Mode_Edit==mode && !force) {
        return;
    }

    if (s.isEmpty()) {
        currentSong=s;
        clear();
        return;
    }

    Song song(s);
    bool songChanged = song.artist!=currentSong.artist || song.title!=currentSong.title;

    if (!isVisible()) {
        if (songChanged) {
            needToUpdate=true;
        }
        currentSong=song;
        return;
    }

    if (force || songChanged) {
        setMode(Mode_Blank);
//        controls->setVisible(true);
        currentRequest++;
        currentSong=song;

        if (song.title.isEmpty() || song.artist.isEmpty()) {
            clear();
            return;
        }

        setHeader(song.title);
        // Only reset the provider if the refresh was an automatic one or if the song has
        // changed. Otherwise we'll keep the provider so the user can cycle through the lyrics
        // offered by the various providers.
        if (!force || songChanged) {
            currentProvider=-1;
        }

        if (!MPDConnection::self()->getDetails().dir.isEmpty() && !song.file.isEmpty() && !song.isStream()) {
            QString songFile=song.file;

            if (song.isCantataStream()) {
                #if QT_VERSION < 0x050000
                QUrl u(songFile);
                #else
                QUrl qu(songFile);
                QUrlQuery u(qu);
                #endif
                songFile=u.hasQueryItem("file") ? u.queryItemValue("file") : QString();
            }

            QString mpdLyrics=Utils::changeExtension(MPDConnection::self()->getDetails().dir+songFile, constExtension);

            if (MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http:/"))) {
                QUrl url(mpdLyrics);
                job=NetworkAccessManager::self()->get(url);
                job->setProperty("file", songFile);
                connect(job, SIGNAL(finished()), SLOT(downloadFinished()));
                return;
            } else {
                #ifdef TAGLIB_FOUND
                QString tagLyrics=Tags::readLyrics(MPDConnection::self()->getDetails().dir+songFile);

                if (!tagLyrics.isEmpty()) {
                    text->setText(fixNewLines(tagLyrics));
                    hideSpinner();
                    setMode(Mode_Display);
//                    controls->setVisible(false);
                    return;
                }
                #endif
                // Stop here if we found lyrics in the cache dir.
                if (setLyricsFromFile(mpdLyrics)) {
                    lyricsFile=mpdLyrics;
                    setMode(Mode_Display);
                    return;
                }
            }
        }

        // Check for cached file...
        QString file=cacheFile(song.artist, song.title);

        /*if (force && QFile::exists(file)) {
            // Delete the cached lyrics file when the user is force-fully re-fetching the lyrics.
            // Afterwards we'll simply do getLyrics() to get the new ones.
            QFile::remove(file);
        } else */if (setLyricsFromFile(file)) {
           // We just wanted a normal update without explicit re-fetching. We can return
           // here because we got cached lyrics and we don't want an explicit re-fetch.
           lyricsFile=file;
           setMode(Mode_Display);
           return;
        }

        getLyrics();
    }
}

void SongView::downloadFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        reply->deleteLater();
        if (QNetworkReply::NoError==reply->error()) {
            QString file=reply->property("file").toString();
            if (!file.isEmpty() && file==currentSong.file) {
                QTextStream str(reply);
                QString lyrics=str.readAll();
                if (!lyrics.isEmpty()) {
                    text->setText(fixNewLines(lyrics));
                    hideSpinner();
                    return;
                }
            }
        }
    }
    getLyrics();
}

void SongView::lyricsReady(int id, QString lyrics)
{
    if (id != currentRequest) {
        return;
    }
    lyrics=lyrics.trimmed();

    if (lyrics.isEmpty()) {
        getLyrics();
    } else {
        hideSpinner();
        QString before=text->toHtml();
        text->setText(fixNewLines(lyrics));
        // Remove formatting, as we dont save this anyway...
        QString plain=text->toPlainText().trimmed();

        if (plain.isEmpty()) {
            text->setText(before);
            getLyrics();
        } else {
            text->setText(fixNewLines(plain));
            lyricsFile=QString();
            if (! ( Settings::self()->storeLyricsInMpdDir() &&
                    saveFile(Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension))) ) {
                saveFile(cacheFile(currentSong.artist, currentSong.title, true));
            }
            setMode(Mode_Display);
        }
    }
}

bool SongView::saveFile(const QString &fileName)
{
    QFile f(fileName);

    if (f.open(QIODevice::WriteOnly)) {
        QTextStream(&f) << text->toPlainText();
        f.close();
        lyricsFile=fileName;
        return true;
    }

    return false;
}

QString SongView::mpdFileName() const
{
    return currentSong.file.isEmpty() || MPDConnection::self()->getDetails().dir.isEmpty() || currentSong.isStream()
            ? QString() : Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension);
}

QString SongView::cacheFileName() const
{
    return currentSong.artist.isEmpty() || currentSong.title.isEmpty() ? QString() : cacheFile(currentSong.artist, currentSong.title);
}

void SongView::getLyrics()
{
    UltimateLyricsProvider *prov=UltimateLyrics::self()->getNext(currentProvider);
    if (prov) {
        text->setText(i18n("Fetching lyrics via %1").arg(prov->getName()));
        prov->fetchInfo(currentRequest, currentSong);
        showSpinner();
    } else {
        text->setText(QString());
        currentProvider=-1;
        hideSpinner();
        // Set lyrics file anyway - so that editing is enabled!
        lyricsFile=Settings::self()->storeLyricsInMpdDir()
                ? Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension)
                : cacheFile(currentSong.artist, currentSong.title);
        setMode(Mode_Display);
    }
}

void SongView::setMode(Mode m)
{
    if (mode==m) {
        return;
    }
    mode=m;
    bool editable=Mode_Display==m && !lyricsFile.isEmpty() && (!QFile::exists(lyricsFile) || QFileInfo(lyricsFile).isWritable());
    saveAction->setEnabled(Mode_Edit==m);
    cancelAction->setEnabled(Mode_Edit==m);
    editAction->setEnabled(editable);
    delAction->setEnabled(editable && !MPDConnection::self()->getDetails().dir.isEmpty() && QFile::exists(Utils::changeExtension(MPDConnection::self()->getDetails().dir+currentSong.file, constExtension)));
    setEditable(Mode_Edit==m);
}

bool SongView::setLyricsFromFile(const QString &filePath)
{
    QFile f(filePath);

    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        // Read the file using a QTextStream so we get automatic UTF8 detection.
        QTextStream inputStream(&f);

        text->setText(fixNewLines(inputStream.readAll()));
        hideSpinner();
        f.close();

        return true;
    }

    return false;
}
