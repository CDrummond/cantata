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
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QSettings>
#include <QtCore/QtConcurrentRun>
#include <QTextBrowser>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KXMLGUIClient>
#include <KDE/KActionCollection>
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
static const QLatin1String constExtension(".lyrics");

static QString cacheFile(QString artist, QString title, bool createDir=false)
{
    title.replace("/", "_");
    artist.replace("/", "_");
    return QDir::toNativeSeparators(Network::cacheDir(constLyricsDir+artist+'/', createDir))+title+constExtension;
}

typedef QList<UltimateLyricsProvider *> ProviderList;

bool CompareLyricProviders(const UltimateLyricsProvider* a, const UltimateLyricsProvider* b) {
    return a->relevance() < b->relevance();
}

LyricsPage::LyricsPage(QWidget *parent)
    : QWidget(parent)
//     , reader(new UltimateLyricsReader(this))
    , currentRequest(0)
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
    KXMLGUIClient *client=dynamic_cast<KXMLGUIClient *>(parent);
    if (client) {
        refreshAction = client->actionCollection()->addAction("refreshlyrics");
    } else {
        refreshAction = new KAction(this);
    }
    refreshAction->setText(i18n("Refresh"));
#else
    refreshAction = new QAction(tr("Refresh"), this);
#endif
    refreshAction->setIcon(QIcon::fromTheme("view-refresh"));
    connect(refreshAction, SIGNAL(triggered()), SLOT(update()));
    refresh->setAutoRaise(true);
    refresh->setDefaultAction(refreshAction);
}

LyricsPage::~LyricsPage()
{
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
    update(currentSong, true);
}

void LyricsPage::update(const Song &song, bool force)
{
    if (force || song.artist!=currentSong.artist || song.title!=currentSong.title) {
        currentRequest++;
        currentSong=song;
        currentProvider=-1;
        if (song.title.isEmpty() || song.artist.isEmpty()) {
            text->setText(QString());
            return;
        }

        if (!mpdDir.isEmpty()) {
            // Check for MPD file...
            QString mpdLyrics=changeExt(mpdDir+song.file, constExtension);

            if (QFile::exists(mpdLyrics)) {
                QFile f(mpdLyrics);

                if (f.open(QIODevice::ReadOnly)) {
                    text->setText(f.readAll());
                    f.close();
                    return;
                }
            }
        }

        // Check for cached file...
        QString file=cacheFile(song.artist, song.title);

        if (QFile::exists(file)) {
            if (force) {
                QFile::remove(file);
            } else {
                QFile f(file);

                if (f.open(QIODevice::ReadOnly)) {
                    text->setText(f.readAll());
                    f.close();
                    return;
                }
            }
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

        QFile f(cacheFile(currentSong.artist, currentSong.title, true));

        if (f.open(QIODevice::WriteOnly)) {
            QTextStream(&f) << text->toPlainText();
            f.close();
        }
    }
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
           return;
        }
    }
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
