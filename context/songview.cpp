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
#include "contextengine.h"
#include "gui/settings.h"
#include "gui/covers.h"
#include "support/squeezedtextlabel.h"
#include "support/utils.h"
#include "support/messagebox.h"
#include "support/localize.h"
#ifdef TAGLIB_FOUND
#include "tags/tags.h"
#endif
#include "widgets/icons.h"
#include "support/utils.h"
#include "support/action.h"
#include "support/actioncollection.h"
#include "network/networkaccessmanager.h"
#include "widgets/textbrowser.h"
#include "gui/stdactions.h"
#include "mpd/mpdstatus.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QTextStream>
#include <QTimer>
#include <QScrollBar>
#include <QDesktopServices>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

const QLatin1String SongView::constLyricsDir("lyrics/");
const QLatin1String SongView::constExtension(".lyrics");
const QLatin1String SongView::constCacheDir("tracks/");
const QLatin1String SongView::constInfoExt(".html.gz");

static QString infoCacheFileName(const Song &song, const QString &lang, bool createDir)
{
    QString artist=song.artist;
    QString title=song.title;
    title.replace("/", "_");
    artist.replace("/", "_");
    QString dir=Utils::cacheDir(SongView::constCacheDir+Covers::encodeName(artist)+Utils::constDirSep, createDir);

    if (dir.isEmpty()) {
        return QString();
    }
    return dir+Covers::encodeName(title)+"."+lang+SongView::constInfoExt;
}

static QString lyricsCacheFileName(const Song &song, bool createDir=false)
{
    QString artist=song.artist;
    QString title=song.title;
    title.replace("/", "_");
    artist.replace("/", "_");
    QString dir=Utils::cacheDir(SongView::constLyricsDir+Covers::encodeName(artist)+Utils::constDirSep, createDir);

    if (dir.isEmpty()) {
        return QString();
    }
    return dir+Covers::encodeName(title)+SongView::constExtension;
}

static inline QString mpdFilePath(const QString &songFile)
{
    return Utils::changeExtension(MPDConnection::self()->getDetails().dir+songFile, SongView::constExtension);
}

static inline QString mpdFilePath(const Song &song)
{
    return mpdFilePath(song.filePath());
}

static inline QString fixNewLines(const QString &o)
{
    return QString(o).replace(QLatin1String("\n\n\n"), QLatin1String("\n\n")).replace("\n", "<br/>");
}

SongView::SongView(QWidget *p)
    : View(p, QStringList() << i18n("Lyrics:") << i18n("Information:"))
    , scrollTimer(0)
    , songPos(0)
    , currentProvider(-1)
    , currentRequest(0)
    , mode(Mode_Display)
    , job(0)
    , currentProv(0)
    , infoNeedsUpdating(true)
{
    scrollAction = ActionCollection::get()->createAction("scrolllyrics", i18n("Scroll Lyrics"), "go-down");
    refreshAction = ActionCollection::get()->createAction("refreshlyrics", i18n("Refresh Lyrics"), "view-refresh");
    editAction = ActionCollection::get()->createAction("editlyrics", i18n("Edit Lyrics"), Icons::self()->editIcon);
    saveAction = ActionCollection::get()->createAction("savelyrics", i18n("Save Lyrics"), "document-save");
    cancelEditAction = ActionCollection::get()->createAction("canceleditlyrics", i18n("Cancel Editing Lyrics"), Icons::self()->cancelIcon);
    delAction = ActionCollection::get()->createAction("dellyrics", i18n("Delete Lyrics File"), "edit-delete");

    scrollAction->setCheckable(true);
    scrollAction->setChecked(Settings::self()->contextAutoScroll());
    connect(scrollAction, SIGNAL(toggled(bool)), SLOT(toggleScroll()));
    connect(refreshAction, SIGNAL(triggered()), SLOT(update()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(saveAction, SIGNAL(triggered()), SLOT(save()));
    connect(cancelEditAction, SIGNAL(triggered()), SLOT(cancel()));
    connect(delAction, SIGNAL(triggered()), SLOT(del()));
    connect(UltimateLyrics::self(), SIGNAL(lyricsReady(int, QString)), SLOT(lyricsReady(int, QString)));

    engine=ContextEngine::create(this);
    refreshInfoAction = ActionCollection::get()->createAction("refreshtrack", i18n("Refresh Track Information"), "view-refresh");
    cancelInfoJobAction=new Action(Icons::self()->cancelIcon, i18n("Cancel"), this);
    cancelInfoJobAction->setEnabled(false);
    connect(refreshInfoAction, SIGNAL(triggered()), SLOT(refreshInfo()));
    connect(cancelInfoJobAction, SIGNAL(triggered()), SLOT(abortInfoSearch()));
    connect(engine, SIGNAL(searchResult(QString,QString)), this, SLOT(infoSearchResponse(QString,QString)));
    foreach (TextBrowser *t, texts) {
        connect(t, SIGNAL(anchorClicked(QUrl)), SLOT(showMoreInfo(QUrl)));
    }

    text->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(text, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
    texts.at(Page_Information)->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(texts.at(Page_Information), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showInfoContextMenu(QPoint)));
    connect(this, SIGNAL(viewChanged()), this, SLOT(curentViewChanged()));
    setMode(Mode_Blank);
    setStandardHeader(i18n("Track"));
    clear();
    toggleScroll();
    setCurrentView(Settings::self()->contextTrackView());
}

SongView::~SongView()
{
    UltimateLyrics::self()->release();
}

void SongView::update()
{
    QString mpdName=mpdFileName();
    QString cacheName=cacheFileName();
    bool mpdExists=!mpdName.isEmpty() && QFile::exists(mpdName);
    bool cacheExists=!cacheName.isEmpty() && QFile::exists(cacheName);

    if (mpdExists || cacheExists) {
        switch (MessageBox::warningYesNoCancel(this, i18n("Reload lyrics?\n\nReload from disk, or delete disk copy and download?"), i18n("Reload"),
                                               GuiItem(i18n("Reload From Disk")), GuiItem(i18n("Download")))) {
        case MessageBox::Yes:
            break;
        case MessageBox::No:
            if (mpdExists) {
                QFile::remove(mpdName);
            }
            if (cacheExists && QFile::exists(cacheName)) {
                QFile::remove(cacheName);
            }
            break;
        default:
            return;
        }
    }

    update(currentSong, true);
}

void SongView::saveConfig()
{
    Settings::self()->saveContextAutoScroll(scrollAction->isChecked());
    Settings::self()->saveContextTrackView(currentView());
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
    if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Delete lyrics file?"), i18n("Delete File"),
                                                 StdGuiItem::del(), StdGuiItem::cancel())) {
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
       menu->addAction(scrollAction);
       menu->addAction(refreshAction);
       menu->addAction(StdActions::self()->searchAction);
       menu->addSeparator();
       menu->addAction(editAction);
       menu->addAction(delAction);
       break;
   case Mode_Edit:
       menu->addSeparator();
       menu->addAction(saveAction);
       menu->addAction(cancelEditAction);
       break;
   }

   if (Mode_Edit!=mode && cancelJobAction->isEnabled()) {
       menu->addSeparator();
       menu->addAction(cancelJobAction);
   }

   menu->exec(text->mapToGlobal(pos));
   delete menu;
}

void SongView::showInfoContextMenu(const QPoint &pos)
{
    QMenu *menu = texts.at(Page_Information)->createStandardContextMenu();
    menu->addSeparator();
    if (cancelInfoJobAction->isEnabled()) {
        menu->addAction(cancelInfoJobAction);
    } else {
        menu->addAction(refreshInfoAction);
    }
    menu->exec(texts.at(Page_Information)->mapToGlobal(pos));
    delete menu;
}

void SongView::toggleScroll()
{
    if (scrollAction->isChecked()) {
        scrollTimer=new QTimer(this);
        scrollTimer->setSingleShot(false);
        scrollTimer->setInterval(1000);
        connect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(songPosition()));
        connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scroll()));
        scroll();
    } else {
        disconnect(MPDStatus::self(), SIGNAL(updated()), this, SLOT(songPosition()));
        if (scrollTimer) {
            connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scroll()));
            scrollTimer->stop();
        }
    }
}

void SongView::songPosition()
{
    if (scrollTimer) {
        songPos=MPDStatus::self()->timeElapsed();
        scroll();
    }
}

void SongView::scroll()
{
    if (Mode_Display==mode && scrollAction->isChecked() && scrollTimer) {
        QScrollBar *bar=text->verticalScrollBar();

        if (bar && bar->isSliderDown()) {
            scrollTimer->stop();
            return;
        }
        if (MPDStatus::self()->timeTotal()<=0) {
            scrollTimer->stop();
            return;
        }
        if (MPDState_Playing==MPDStatus::self()->state()) {
            if (!scrollTimer->isActive()) {
                scrollTimer->start();
            }
        } else {
            scrollTimer->stop();
        }

        if (MPDStatus::self()->guessedElapsed()>=MPDStatus::self()->timeTotal()) {
            if (scrollTimer) {
                scrollTimer->stop();
            }
            return;
        }

        if (bar->isVisible()) {
            int newSliderPosition =
                        MPDStatus::self()->guessedElapsed() * (bar->maximum() + bar->pageStep()) / MPDStatus::self()->timeTotal() -
                        bar->pageStep() / 2;
            bar->setSliderPosition(newSliderPosition);
        }
    }
}

void SongView::curentViewChanged()
{
    if (infoNeedsUpdating) {
        loadInfo();
    }
}

void SongView::loadInfo()
{
    infoNeedsUpdating=false;
    foreach (const QString &lang, engine->getLangs()) {
        QString prefix=engine->getPrefix(lang);
        QString cachedFile=infoCacheFileName(currentSong, prefix, false);
        if (QFile::exists(cachedFile)) {
            QFile f(cachedFile);
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::ReadOnly)) {
                QByteArray data=compressor.readAll();

                if (!data.isEmpty()) {
                    infoSearchResponse(QString::fromUtf8(data), QString());
                    Utils::touchFile(cachedFile);
                    return;
                }
            }
        }
    }
    searchForInfo();
}

void SongView::refreshInfo()
{
    if (currentSong.isEmpty()) {
        return;
    }
    foreach (const QString &lang, engine->getLangs()) {
        QFile::remove(infoCacheFileName(currentSong, engine->getPrefix(lang), false));
    }
    searchForInfo();
}

void SongView::searchForInfo()
{
    cancelInfoJobAction->setEnabled(true);
    engine->search(QStringList() << currentSong.artist << currentSong.title, ContextEngine::Track);
    showSpinner(false);
}

void SongView::infoSearchResponse(const QString &resp, const QString &lang)
{
    cancelInfoJobAction->setEnabled(false);
    hideSpinner();

    if (!resp.isEmpty()) {
        QString str=engine->translateLinks(resp);
        if (!lang.isEmpty()) {
            QFile f(infoCacheFileName(currentSong, lang, true));
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::WriteOnly)) {
                compressor.write(resp.toUtf8().constData());
            }
        }
        setHtml(str, Page_Information);
    }
}

void SongView::abortInfoSearch()
{
    if (cancelInfoJobAction->isEnabled()) {
        cancelInfoJobAction->setEnabled(false);
        engine->cancel();
        hideSpinner();
    }
}

void SongView::showMoreInfo(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void SongView::hideSpinner()
{
    if (!cancelInfoJobAction->isEnabled() && !cancelJobAction->isEnabled()) {
        View::hideSpinner();
    }
}

void SongView::abort()
{
    if (job) {
        job->cancelAndDelete();
        job=0;
    }
    currentProvider=-1;
    if (currentProv) {
        currentProv->abort();
        currentProv=0;

        text->setText(QString());
        // Set lyrics file anyway - so that editing is enabled!
        lyricsFile=Settings::self()->storeLyricsInMpdDir() && !currentSong.isNonMPD()
                ? mpdFilePath(currentSong)
                : lyricsCacheFileName(currentSong);
        setMode(Mode_Display);
    }
    cancelJobAction->setEnabled(false);
    hideSpinner();
}

void SongView::update(const Song &s, bool force)
{
    if (Mode_Edit==mode && !force) {
        return;
    }

    if (s.isEmpty() || s.title.isEmpty() || s.artist.isEmpty()) {
        currentSong=s;
        infoNeedsUpdating=false;
        clear();
        abort();
        return;
    }

    Song song(s);
    bool songChanged = song.artist!=currentSong.artist || song.title!=currentSong.title;

    if (songChanged) {
        abort();
    }
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
        if (Page_Information==currentView()) {
            loadInfo();
        } else {
            infoNeedsUpdating=true;
        }

        // Only reset the provider if the refresh was an automatic one or if the song has
        // changed. Otherwise we'll keep the provider so the user can cycle through the lyrics
        // offered by the various providers.
        if (!force || songChanged) {
            currentProvider=-1;
        }

        if (!MPDConnection::self()->getDetails().dir.isEmpty() && !song.file.isEmpty() && !song.isNonMPD()) {
            QString songFile=song.filePath();

            if (song.isCantataStream()) {
                #if QT_VERSION < 0x050000
                QUrl u(songFile);
                #else
                QUrl qu(songFile);
                QUrlQuery u(qu);
                #endif
                songFile=u.hasQueryItem("file") ? u.queryItemValue("file") : QString();
            }

            QString mpdLyrics=mpdFilePath(songFile);

            if (MPDConnection::self()->getDetails().dir.startsWith(QLatin1String("http:/"))) {
                QUrl url(mpdLyrics);
                job=NetworkAccessManager::self()->get(url);
                job->setProperty("file", song.file);
                connect(job, SIGNAL(finished()), this, SLOT(downloadFinished()));
                return;
            } else {
                #ifdef TAGLIB_FOUND
                QString tagLyrics=Tags::readLyrics(MPDConnection::self()->getDetails().dir+songFile);

                if (!tagLyrics.isEmpty()) {
                    text->setText(fixNewLines(tagLyrics));
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
        QString file=lyricsCacheFileName(song);

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
    NetworkJob *reply=qobject_cast<NetworkJob *>(sender());
    if (reply) {
        reply->deleteLater();
        if (job==reply) {
            if (reply->ok()) {
                QString file=reply->property("file").toString();
                if (!file.isEmpty() && file==currentSong.file) {
                    QTextStream str(reply->actualJob());
                    QString lyrics=str.readAll();
                    if (!lyrics.isEmpty()) {
                        text->setText(fixNewLines(lyrics));
                        cancelJobAction->setEnabled(false);
                        hideSpinner();
                        return;
                    }
                }
            }
            job=0;
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
        cancelJobAction->setEnabled(false);
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
            if (! ( Settings::self()->storeLyricsInMpdDir() && !currentSong.isNonMPD() &&
                    saveFile(mpdFilePath(currentSong))) ) {
                saveFile(lyricsCacheFileName(currentSong, true));
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
    return currentSong.file.isEmpty() || MPDConnection::self()->getDetails().dir.isEmpty() || currentSong.isNonMPD()
            ? QString() : mpdFilePath(currentSong);
}

QString SongView::cacheFileName() const
{
    return currentSong.artist.isEmpty() || currentSong.title.isEmpty() ? QString() : lyricsCacheFileName(currentSong);
}

void SongView::getLyrics()
{
    currentProv=UltimateLyrics::self()->getNext(currentProvider);
    if (currentProv) {
        text->setText(i18n("Fetching lyrics via %1", currentProv->displayName()));
        currentProv->fetchInfo(currentRequest, currentSong);
        showSpinner();
    } else {
        text->setText(QString());
        currentProvider=-1;
        // Set lyrics file anyway - so that editing is enabled!
        lyricsFile=Settings::self()->storeLyricsInMpdDir() && !currentSong.isNonMPD()
                ? mpdFilePath(currentSong)
                : lyricsCacheFileName(currentSong);
        setMode(Mode_Display);
    }
}

void SongView::setMode(Mode m)
{
    if (Mode_Display==m) {
        cancelJobAction->setEnabled(false);
        hideSpinner();
    }
    if (mode==m) {
        return;
    }
    mode=m;
    bool editable=Mode_Display==m && !lyricsFile.isEmpty() && (!QFile::exists(lyricsFile) || QFileInfo(lyricsFile).isWritable());
    saveAction->setEnabled(Mode_Edit==m);
    cancelEditAction->setEnabled(Mode_Edit==m);
    editAction->setEnabled(editable);
    delAction->setEnabled(editable && !MPDConnection::self()->getDetails().dir.isEmpty() && QFile::exists(mpdFilePath(currentSong)));
    refreshAction->setEnabled(editable);
    setEditable(Mode_Edit==m);
    if (scrollAction->isChecked()) {
        if (Mode_Display==mode) {
            scroll();
        } else if (scrollTimer) {
            scrollTimer->stop();
        }
    }
}

bool SongView::setLyricsFromFile(const QString &filePath)
{
    QFile f(filePath);

    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        // Read the file using a QTextStream so we get automatic UTF8 detection.
        QTextStream inputStream(&f);

        text->setText(fixNewLines(inputStream.readAll()));
        cancelJobAction->setEnabled(false);
        hideSpinner();
        f.close();

        return true;
    }

    return false;
}
