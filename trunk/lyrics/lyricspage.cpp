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

#include "songinfoprovider.h"
#include "lyricspage.h"
#include "ultimatelyricsprovider.h"
#include "ultimatelyricsreader.h"
#include "network.h"
#include "settings.h"
#include "mainwindow.h"
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QSettings>
#include <QtCore/QtConcurrentRun>
#include <QTextBrowser>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KXMLGUIClient>
#include <KDE/KActionCollection>
#include <KDE/KMessageBox>
#else
#include <QtGui/QMessageBox>
#endif

static QString changeExt(const QString &f, const QString &newExt)
{
    QString newStr(f);
    int     dotPos(newStr.lastIndexOf('.'));

    if (-1==dotPos) {
        newStr+=newExt;
    } else {
        newStr.remove(dotPos, newStr.length());
        newStr+=newExt;
    }
    return newStr;
}

static const QLatin1String constLyricsDir("lyrics/");
const QLatin1String LyricsPage::constExtension(".lyrics");

static QString cacheFile(QString artist, QString title, bool createDir=false)
{
    title.replace("/", "_");
    artist.replace("/", "_");
    return QDir::toNativeSeparators(Network::cacheDir(constLyricsDir+artist+'/', createDir))+title+LyricsPage::constExtension;
}

typedef QList<UltimateLyricsProvider *> ProviderList;

bool CompareLyricProviders(const UltimateLyricsProvider* a, const UltimateLyricsProvider* b) {
    return a->relevance() < b->relevance();
}

LyricsPage::LyricsPage(MainWindow *p)
    : QWidget(p)
//     , reader(new UltimateLyricsReader(this))
    , currentRequest(0)
    , mode(Mode_Display)
{
    setupUi(this);

    providers=UltimateLyricsReader().Parse(QString(":lyrics/ultimate_providers.xml"));
    foreach (UltimateLyricsProvider* provider, providers) {
        connect(provider, SIGNAL(InfoReady(int, const QString &)), SLOT(resultReady(int, const QString &)));
    }

    // Parse the ultimate lyrics xml file in the background
//     QFuture<ProviderList> future = QtConcurrent::run(reader.data(), &UltimateLyricsReader::Parse,
//                                                      QString(":lyrics/ultimate_providers.xml"));
//     QFutureWatcher<ProviderList> *watcher = new QFutureWatcher<ProviderList>(this);
//     watcher->setFuture(future);
//     connect(watcher, SIGNAL(finished()), SLOT(ultimateLyricsParsed()));
    #ifdef ENABLE_KDE_SUPPORT
    refreshAction = p->actionCollection()->addAction("refreshlyrics");
    refreshAction->setText(i18n("Refresh"));
    editAction = p->actionCollection()->addAction("editlyrics");
    editAction->setText(i18n("Edit"));
    saveAction = p->actionCollection()->addAction("savelyrics");
    saveAction->setText(i18n("Save"));
    cancelAction = p->actionCollection()->addAction("canceleditlyrics");
    cancelAction->setText(i18n("Cancel"));
    delAction = p->actionCollection()->addAction("dellyrics");
    delAction->setText(i18n("Delete"));
    #else
    refreshAction = new QAction(tr("Refresh"), this);
    editAction = new QAction(tr("Refresh"), this);
    saveAction = new QAction(tr("Refresh"), this);
    cancelAction = new QAction(tr("Refresh"), this);
    delAction = new QAction(tr("Refresh"), this);
    #endif
    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    editAction->setIcon(QIcon::fromTheme("document-edit"));
    saveAction->setIcon(QIcon::fromTheme("document-save"));
    cancelAction->setIcon(QIcon::fromTheme("dialog-cancel"));
    delAction->setIcon(QIcon::fromTheme("edit-delete"));
    connect(refreshAction, SIGNAL(triggered()), SLOT(update()));
    connect(editAction, SIGNAL(triggered()), SLOT(edit()));
    connect(saveAction, SIGNAL(triggered()), SLOT(save()));
    connect(cancelAction, SIGNAL(triggered()), SLOT(cancel()));
    connect(delAction, SIGNAL(triggered()), SLOT(del()));
    MainWindow::initButton(refreshBtn);
    MainWindow::initButton(editBtn);
    MainWindow::initButton(saveBtn);
    MainWindow::initButton(cancelBtn);
    MainWindow::initButton(delBtn);
    refreshBtn->setDefaultAction(refreshAction);
    editBtn->setDefaultAction(editAction);
    saveBtn->setDefaultAction(saveAction);
    cancelBtn->setDefaultAction(cancelAction);
    delBtn->setDefaultAction(delAction);
    text->setZoom(Settings::self()->lyricsZoom());
    setMode(Mode_Blank);
}

LyricsPage::~LyricsPage()
{
    foreach (UltimateLyricsProvider* provider, providers) {
        delete provider;
    }
}

void LyricsPage::saveSettings()
{
    Settings::self()->saveLyricsZoom(text->zoom());
}

void LyricsPage::setEnabledProviders(const QStringList &providerList)
{
    foreach (UltimateLyricsProvider* provider, providers) {
        provider->set_enabled(false);
        provider->set_relevance(0xFFFF);
    }

    int relevance=0;
    foreach (const QString &p, providerList) {
        UltimateLyricsProvider *provider=providerByName(p);
        if (provider) {
            provider->set_enabled(true);
            provider->set_relevance(relevance++);
        }
    }

    qSort(providers.begin(), providers.end(), CompareLyricProviders);
}

void LyricsPage::update()
{
    QString mpdName=changeExt(Settings::self()->mpdDir()+currentSong.file, constExtension);
    bool mpdExists=!Settings::self()->mpdDir().isEmpty() && QFile::exists(mpdName);
    #ifdef ENABLE_KDE_SUPPORT
    if (Mode_Edit==mode && KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"))) {
        return;
    } else if(mpdExists && KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Delete saved copy of lyrics, and re-download?"))) {
        return;
    }
    #else
    if (Mode_Edit==mode && QMessageBox::No==QMessageBox::question(this, tr("Question"), tr("Abort editing of lyrics?"),  QMessageBox::Yes|QMessageBox::No)) {
        return;
    } else if(mpdExists && QMessageBox::No==QMessageBox::question(this, tr("Question"), tr("Delete saved copy of lyrics, and re-download?"))) {
        return;
    }
    #endif
    if (mpdExists) {
        QFile::remove(mpdName);
    }
    QString cacheName=cacheFile(currentSong.artist, currentSong.title);
    if (QFile::exists(cacheName)) {
        QFile::remove(cacheName);
    }
    update(currentSong, true);
}

void LyricsPage::edit()
{
    preEdit=text->toPlainText();
    setMode(Mode_Edit);
}

void LyricsPage::save()
{
    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Save updated lyrics?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::question(this, tr("Question"), tr("Save updated lyrics?"), QMessageBox::Yes|QMessageBox::No)) {
        return;
    }
    #endif

    QString mpdName=changeExt(Settings::self()->mpdDir()+currentSong.file, constExtension);
    QString cacheName=cacheFile(currentSong.artist, currentSong.title);
    if (saveFile(mpdName)) {
        if (QFile::exists(cacheName)) {
            QFile::remove(cacheName);
        }
    } else if (!saveFile(cacheName)) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::error(this, i18n("Failed to save lyrics."));
        #else
        QMessageBox::critical(this, tr("Error"), tr("Save updated lyrics?"));
        #endif
        return;
    }

    setMode(Mode_Display);
}

void LyricsPage::cancel()
{
    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Abort editing of lyrics?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::question(this, tr("Question"), tr("Abort editing of lyrics?"),  QMessageBox::Yes|QMessageBox::No)) {
        return;
    }
    #endif
    text->setText(preEdit);
    setMode(Mode_Display);
}

void LyricsPage::del()
{
    #ifdef ENABLE_KDE_SUPPORT
    if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Delete lyrics file?"))) {
        return;
    }
    #else
    if (QMessageBox::No==QMessageBox::question(this, tr("Question"), tr("Delete lyrics file?"),  QMessageBox::Yes|QMessageBox::No)) {
        return;
    }
    #endif

    QString mpdName=changeExt(Settings::self()->mpdDir()+currentSong.file, constExtension);
    QString cacheName=cacheFile(currentSong.artist, currentSong.title);
    if (QFile::exists(cacheName)) {
        QFile::remove(cacheName);
    }
    if (QFile::exists(mpdName)) {
        QFile::remove(mpdName);
    }
}

void LyricsPage::update(const Song &song, bool force)
{
    if (Mode_Edit==mode && !force) {
        return;
    }

    if (force || song.artist!=currentSong.artist || song.title!=currentSong.title) {
        setMode(Mode_Blank);
        currentRequest++;
        currentSong=song;
        currentProvider=-1;
        if (song.title.isEmpty() || song.artist.isEmpty()) {
            text->setText(QString());
            return;
        }

        if (!Settings::self()->mpdDir().isEmpty()) {
            // Check for MPD file...
            QString mpdLyrics=changeExt(Settings::self()->mpdDir()+song.file, constExtension);

//             if (force && QFile::exists(mpdLyrics)) {
//                 QFile::remove(mpdLyrics);
//             }

            // Stop here if we found lyrics in the cache dir.
            if (setLyricsFromFile(mpdLyrics)) {
                lyricsFile=mpdLyrics;
                setMode(Mode_Display);
                return;
            }
        }

        // Check for cached file...
        QString file=cacheFile(song.artist, song.title);

        /*if (force && QFile::exists(file)) {
            // Delete the cached lyrics file when the user
            // is force-fully re-fetching the lyrics.
            // Afterwards we'll simply do getLyrics() to get
            // the new ones.
            QFile::remove(file);
        } else */if (setLyricsFromFile(file)) {
           // We just wanted a normal update without
           // explicit re-fetching. We can return
           // here because we got cached lyrics and
           // we don't want an explicit re-fetch.
           lyricsFile=file;
           setMode(Mode_Display);
           return;
        }

        getLyrics();
    }
}

void LyricsPage::resultReady(int id, const QString &lyrics)
{
    if (id != currentRequest)
        return;

    if (lyrics.isEmpty()) {
        getLyrics();
    } else {
        text->setText(lyrics);
        lyricsFile=QString();
        if (! ( Settings::self()->storeLyricsInMpdDir() &&
                saveFile(changeExt(Settings::self()->mpdDir()+currentSong.file, constExtension))) ) {
            saveFile(cacheFile(currentSong.artist, currentSong.title, true));
        }
        setMode(Mode_Display);
    }
}

bool LyricsPage::saveFile(const QString &fileName)
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

UltimateLyricsProvider* LyricsPage::providerByName(const QString &name) const
{
    foreach (UltimateLyricsProvider* provider, providers) {
        if (provider->name() == name) {
            return provider;
        }
    }
    return 0;
}

void LyricsPage::getLyrics()
{
    for(;;) {
        currentProvider++;
        if (currentProvider<providers.count()) {
            UltimateLyricsProvider *prov=providers.at(currentProvider++);
            if (prov && prov->is_enabled()) {
#ifdef ENABLE_KDE_SUPPORT
                text->setText(i18n("Fetching lyrics via %1", prov->name()));
#else
                text->setText(tr("Fetching lyrics via %1").arg(prov->name()));
#endif
                prov->FetchInfo(currentRequest, currentSong);
                return;
            }
        } else {
#ifdef ENABLE_KDE_SUPPORT
           text->setText(i18n("No lyrics found"));
#else
           text->setText(tr("No lyrics found"));
#endif
           currentProvider=-1;
           setMode(Mode_Blank);
           return;
        }
    }
}

void LyricsPage::setMode(Mode m)
{
    if (mode==m) {
        return;
    }
    mode=m;
    bool editable=Mode_Display==m && !lyricsFile.isEmpty() && QFileInfo(lyricsFile).isWritable();
    saveAction->setEnabled(Mode_Edit==m);
    cancelAction->setEnabled(Mode_Edit==m);
    editAction->setEnabled(editable);
    delAction->setEnabled(editable && !Settings::self()->mpdDir().isEmpty() && QFile::exists(changeExt(Settings::self()->mpdDir()+currentSong.file, constExtension)));
    text->setReadOnly(Mode_Edit!=m);
}

bool LyricsPage::setLyricsFromFile(const QString &filePath) const
{
    bool success = false;

    QFile f(filePath);

    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        // Read the file using a QTextStream so we get
        // automatic UTF8 detection.
        QTextStream inputStream(&f);

        text->setText(inputStream.readAll());
        f.close();

        success = true;
    }

    return success;
}

// void LyricsPage::ultimateLyricsParsed()
// {
//     QFutureWatcher<ProviderList>* watcher = static_cast<QFutureWatcher<ProviderList>*>(sender());
//     QStringList names;
//
//     foreach (UltimateLyricsProvider* provider, watcher->result()) {
//         providers.append(provider);
//         connect(provider, SIGNAL(InfoReady(int, const QString &)), SLOT(resultReady(int, const QString &)));
//     }
//
//     watcher->deleteLater();
//     reader.reset();
//     emit providersUpdated();
// }
