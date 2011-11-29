#include "songinfoprovider.h"
#include "lyricspage.h"
#include "ultimatelyricsprovider.h"
#include "ultimatelyricsreader.h"
#include "musiclibrarymodel.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QSettings>
#include <QtConcurrentRun>
#include <QTextBrowser>
#include <QFile>
#include <QDir>
#include <KDE/KLocale>

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
    return QDir::toNativeSeparators(MusicLibraryModel::cacheDir(constLyricsDir+artist+'/', createDir))+title+constExtension;
}

typedef QList<UltimateLyricsProvider *> ProviderList;

bool CompareLyricProviders(const UltimateLyricsProvider* a, const UltimateLyricsProvider* b) {
    if (a->is_enabled() && !b->is_enabled())
        return true;
    if (!a->is_enabled() && b->is_enabled())
        return false;
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
}

LyricsPage::~LyricsPage()
{
}

void LyricsPage::setEnabledProviders(const QStringList &providerList)
{
    int relevance=0;
    foreach (UltimateLyricsProvider* provider, providers) {
        if (UltimateLyricsProvider* lyrics = qobject_cast<UltimateLyricsProvider*>(provider)) {
            lyrics->set_enabled(providerList.contains(lyrics->name()));
            lyrics->set_relevance(relevance++);
        }
    }

    qSort(providers.begin(), providers.end(), CompareLyricProviders);
}

void LyricsPage::update(const Song &song)
{
    if (song.artist!=currentSong.artist || song.title!=currentSong.title) {
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
            QFile f(file);

            if (f.open(QIODevice::ReadOnly)) {
                text->setText(f.readAll());
                f.close();
                return;
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
                text->setText(i18n("Fetching lyrics via %1", prov->name()));
                prov->FetchInfo(currentRequest, currentSong);
                return;
            }
        } else {
           text->setText(i18n("No lyrics found"));
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
