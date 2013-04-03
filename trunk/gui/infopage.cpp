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

#include "infopage.h"
#include "localize.h"
#include "covers.h"
#include "utils.h"
#include "combobox.h"
#include "headerlabel.h"
#include "musiclibrarymodel.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "qjson/parser.h"
#include "qtiocompressor/qtiocompressor.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QNetworkReply>
#include <QFile>
#include <QBuffer>
#include <QApplication>
#include <QTextDocument>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#ifndef Q_OS_WIN
#include "gtkproxystyle.h"
#include <QProcess>
#include <QXmlStreamReader>
#endif

static const QLatin1String constApiKey("N5JHVHNG0UOZZIDVT");
static const char *constNameKey="name";
static const int constCacheAge=7;
static int imageWidth=-1;
static int imageHeight=-1;

const QLatin1String InfoPage::constCacheDir("artists/");
const QLatin1String InfoPage::constInfoExt(".json.gz");

static QString cacheFileName(const QString &artist, bool similar, bool createDir)
{
    return Utils::cacheDir(InfoPage::constCacheDir, createDir)+Covers::encodeName(artist)+(similar ? "-similar" : "")+InfoPage::constInfoExt;
}

static QString encode(const QImage &img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return QString("<img src=\"data:image/png;base64,%1\">").arg(QString(buffer.data().toBase64()));
}

InfoBrowser::InfoBrowser(QWidget *p)
    : TextBrowser(p)
{
    setZoom(Settings::self()->infoZoom());
    setOpenLinks(false);
}

// QTextEdit/QTextBrowser seems to do FastTransformation when scaling images, and this looks bad.
QVariant InfoBrowser::loadResource(int type, const QUrl &name)
{
    if (QTextDocument::ImageResource==type && (name.scheme().isEmpty() || QLatin1String("file")==name.scheme())) {
        QImage img;
        img.load(name.path());
        if (!img.isNull()) {
            return img.scaled(imageWidth, imageHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    return TextBrowser::loadResource(type, name);
}

InfoPage::InfoPage(QWidget *parent)
    : QWidget(parent)
    , needToUpdate(false)
    , currentBioJob(0)
    , currentSimilarJob(0)
{
    QVBoxLayout *vlayout=new QVBoxLayout(this);
    header=new HeaderLabel(this);
    text=new InfoBrowser(this);
    combo=new ComboBox(this);
    vlayout->addWidget(header);
    vlayout->addWidget(text);
    vlayout->addWidget(combo);
    vlayout->setMargin(0);
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(setBio()));
    #ifndef Q_OS_WIN
    combo->setProperty(GtkProxyStyle::constSlimComboProperty, true);
    #endif
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage,QString)), SLOT(artistImage(Song,QImage,QString)));
    connect(text, SIGNAL(anchorClicked(QUrl)), SLOT(showArtist(QUrl)));
    Utils::clearOldCache(constCacheDir, constCacheAge);
    header->setText(i18n("Information"));
    if (-1==imageHeight) {
        imageHeight=fontMetrics().height()*12;
        imageWidth=imageHeight*1.5;
    }
}

void InfoPage::saveSettings()
{
    Settings::self()->saveInfoZoom(text->zoom());
}

void InfoPage::showEvent(QShowEvent *e)
{
    if (needToUpdate) {
        update(currentSong, true);
    }
    needToUpdate=false;
    QWidget::showEvent(e);
}

void InfoPage::update(const Song &s, bool force)
{
    Song song=s;
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }

    bool artistChanged=song.artist!=currentSong.artist;

    if (!isVisible()) {
        if (artistChanged) {
            needToUpdate=true;
        }
        currentSong=song;
        return;
    }

    if (artistChanged || force) {
        currentSong=song;
        combo->clear();
        biographies.clear();
        text->setImage(QImage());
        image=QString();
        similarArtists=QString();
        if (currentSong.isEmpty()) {
            text->setText(QString());
            header->setText(i18n("Information"));
        } else {
            text->setHtml("<i>Retrieving...</i>", true);
            header->setText(currentSong.artist);

            Song s;
            s.albumartist=currentSong.artist;
            s.file=currentSong.file;
            Covers::self()->requestCover(s, true);
            loadBio();
        }
    }
}

void InfoPage::artistImage(const Song &song, const QImage &i, const QString &f)
{
    if (song.albumartist==currentSong.artist && image.isEmpty() && !i.isNull()) {
        if (!f.isEmpty() && QFile::exists(f)) {
            image=QString("<img src=\"%1\">").arg(f);
        } else {
            // No filename given, or file does not exist - therefore scale image.
            image=encode(i.scaled(imageWidth, imageHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        setBio();
    }
}

void InfoPage::loadBio()
{
    QString cachedFile=cacheFileName(currentSong.artist, false, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::ReadOnly)) {
            QByteArray data=compressor.readAll();

            if (!data.isEmpty() && parseBioResponse(data)) {
                setBio();
                Utils::touchFile(cachedFile);
                return;
            }
        }
    }

    requestBio();
}

void InfoPage::loadSimilar()
{
    QString cachedFile=cacheFileName(currentSong.artist, true, false);
    if (QFile::exists(cachedFile)) {
        QFile f(cachedFile);
        QtIOCompressor compressor(&f);
        compressor.setStreamFormat(QtIOCompressor::GzipFormat);
        if (compressor.open(QIODevice::ReadOnly)) {
            QByteArray data=compressor.readAll();

            if (!data.isEmpty() && parseSimilarResponse(data)) {
                setBio();
                Utils::touchFile(cachedFile);
                return;
            }
        }
    }

    requestSimilar();
}

void InfoPage::handleBioReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (reply==currentBioJob) {
        bool ok=false;
        if (QNetworkReply::NoError==reply->error()) {
            QByteArray data=reply->readAll();
            if (parseBioResponse(data)) {
                setBio();
                ok=true;
                QFile f(cacheFileName(reply->property(constNameKey).toString(), false, true));
                QtIOCompressor compressor(&f);
                compressor.setStreamFormat(QtIOCompressor::GzipFormat);
                if (compressor.open(QIODevice::WriteOnly)) {
                    compressor.write(data);
                }
            }
        }
        if (!ok) {
            text->setHtml(i18n(("<i><Failed to download information!</i>")));
        }
        reply->deleteLater();
        currentBioJob=0;
    }
}

void InfoPage::handleSimilarReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (reply==currentSimilarJob) {
        if (QNetworkReply::NoError==reply->error()) {
            QByteArray data=reply->readAll();
            if (parseSimilarResponse(data)) {
                setBio();
                QFile f(cacheFileName(reply->property(constNameKey).toString(), true, true));
                QtIOCompressor compressor(&f);
                compressor.setStreamFormat(QtIOCompressor::GzipFormat);
                if (compressor.open(QIODevice::WriteOnly)) {
                    compressor.write(data);
                }
            }
        }
        reply->deleteLater();
        currentSimilarJob=0;
    }
}

void InfoPage::setBio()
{
    int bio=combo->currentIndex();
    if (biographies.contains(bio)) {
        QString html;

        if (!image.isEmpty()) {
            html=image;
        }
        html+="<p>"+biographies[bio]+"</p>";
        if (!similarArtists.isEmpty()) {
            html+=similarArtists;
        }

        #ifndef Q_OS_WIN
        if (webLinks.isEmpty()) {
            QFile file(":weblinks.xml");
            if (file.open(QIODevice::ReadOnly)) {
                QXmlStreamReader reader(&file);
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (QLatin1String("link")==reader.name()) {
                        QXmlStreamAttributes attributes = reader.attributes();
                        QString url=attributes.value("url").toString();
                        QString name=attributes.value("name").toString();

                        if (!url.isEmpty() && !name.isEmpty()) {
                            webLinks+=QLatin1String("<li><a href=\"")+url+"\">"+name+"</a></li>";
                        }
                    }
                }
            }
        }

        if (!webLinks.isEmpty()) {
            html+="<br/><b>"+i18n("Web Links")+"</b><ul>"+QString(webLinks).replace("${artist}", currentSong.artist)+"</ul>";
        }
        #endif
        text->setHtml(html);
    }
}

void InfoPage::requestBio()
{
    abort();
    #if QT_VERSION < 0x050000
    QUrl url;
    QUrl &query=url;
    #else
    QUrl url;
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost("developer.echonest.com");
    url.setPath("api/v4/artist/biographies");
    query.addQueryItem("api_key", constApiKey);
    query.addQueryItem("name", currentSong.artist);
    query.addQueryItem("format", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    currentBioJob=NetworkAccessManager::self()->get(url);
    currentBioJob->setProperty(constNameKey, currentSong.artist);
    connect(currentBioJob, SIGNAL(finished()), this, SLOT(handleBioReply()));
}

void InfoPage::requestSimilar()
{
    abort();
    #if QT_VERSION < 0x050000
    QUrl url;
    QUrl &query=url;
    #else
    QUrl url;
    QUrlQuery query;
    #endif
    url.setScheme("http");
    url.setHost("developer.echonest.com");
    url.setPath("api/v4/artist/similar");
    query.addQueryItem("api_key", constApiKey);
    query.addQueryItem("name", currentSong.artist);
    query.addQueryItem("format", "json");
    #if QT_VERSION >= 0x050000
    url.setQuery(query);
    #endif

    currentSimilarJob=NetworkAccessManager::self()->get(url);
    currentSimilarJob->setProperty(constNameKey, currentSong.artist);
    connect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarReply()));
}

void InfoPage::abort()
{
    if (currentBioJob) {
        disconnect(currentBioJob, SIGNAL(finished()), this, SLOT(handleBioReply()));
        currentBioJob->abort();
        currentBioJob=0;
    }

    if (currentSimilarJob) {
        disconnect(currentSimilarJob, SIGNAL(finished()), this, SLOT(handleSimilarArtistsReply()));
        currentSimilarJob->abort();
        currentSimilarJob=0;
    }
}

struct Bio
{
    Bio(const QString &s=QString(), const QString &t=QString())
        : site(s), text(t) {
        if (QLatin1String("last.fm")==site) {
            text.replace("  ", "<br/><br/>");
        } else {
            text.replace("\n", "<br/><br/>");
        }
        text.replace("<br/><br/><br/>", "<br/>");
    }
    QString site;
    QString text;
};

int value(const QString &site)
{
    return QLatin1String("last.fm")==site
            ? 0
            : QLatin1String("wikipedia")==site
              ? 1
              : 2;
}

static const QString constReferencesLine("This article does not cite any references or sources.");
static const QString constDisambiguationLine("This article is about the band");
static const QString constDisambiguationLine2("For other uses, see ");

bool InfoPage::parseBioResponse(const QByteArray &resp)
{
    QMultiMap<int, Bio> biogs;
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("response")) {
        QVariantMap response=parsed["response"].toMap();
        if (response.contains("biographies")) {
            QVariantList biographies=response["biographies"].toList();
            foreach (const QVariant &b, biographies) {
                QVariantMap details=b.toMap();
                if (!details.isEmpty() && details.contains("text") && details.contains("site")) {
                    QString site=details["site"].toString();
                    QString text=details["text"].toString();

                    if (text.startsWith(constReferencesLine) || text.startsWith(constDisambiguationLine) || text.startsWith(constDisambiguationLine2)) {
                        int eol=text.indexOf("\n");
                        if (-1!=eol) {
                            text=text.mid(eol+1);
                        }
                    }

//                    if (QLatin1String("wikipedia")==site) {
//                        int start=text.indexOf("\n , ");
//                        if (-1!=start) {
//                            int end=text.indexOf("\n", start+4);
//                            if (-1!=end) {
//                                text=text.remove(start, end-start);
//                            }
//                        }
//                    }
                    while ('\n'==text[0]) {
                        text=text.mid(1);
                    }
                    text.replace("\n ", "\n");

                    biogs.insertMulti(value(site), Bio(site, text));
                }
            }
        }
    }

    QMultiMap<int, Bio>::ConstIterator it=biogs.constBegin();
    QMultiMap<int, Bio>::ConstIterator end=biogs.constEnd();
    for (; it!=end; ++it) {
        if (it.value().text.length()<75 && combo->count()>0) {
            // Ignore the "..." cr*p
            continue;
        }

        // And some others that seem to be just lots of dots???
        if (it.value().text.length()<250) {
            QString copy=it.value().text;
            copy.replace(".", "");
            if (copy.length()<75) {
                continue;
            }
        }
        biographies[combo->count()]="<p><b>"+i18n("Biography")+"</b></p><p>"+it.value().text;
        combo->insertItem(combo->count(), i18n("Source: %1").arg(it.value().site));
    }

    if (!biogs.isEmpty()) {
        loadSimilar();
    }
    return !biogs.isEmpty();
}

bool InfoPage::parseSimilarResponse(const QByteArray &resp)
{
    QSet<QString> mpdArtists=MusicLibraryModel::self()->getAlbumArtists();
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(resp, &ok).toMap();
    if (ok && parsed.contains("response")) {
        QVariantMap response=parsed["response"].toMap();
        if (response.contains("artists")) {
            QVariantList artists=response["artists"].toList();
            foreach (const QVariant &a, artists) {
                QVariantMap details=a.toMap();
                if (!details.isEmpty() && details.contains("name")) {
                    QString artist=details["name"].toString();
                    if (similarArtists.isEmpty()) {
                        similarArtists="<br/><b>"+i18n("Similar Artists")+"</b><ul>";
                    }
                    if (mpdArtists.contains(artist)) {
                        artist=QLatin1String("<a href=\"cantata://?artist=")+artist+"\">"+artist+"</a>";
                    } else {
                        // Check for AC/DC -> AC-DC
                        QString mod=artist;
                        mod=mod.replace("/", "-");
                        if (mod!=artist && mpdArtists.contains(mod)) {
                            artist=QLatin1String("<a href=\"cantata://?artist=")+mod+"\">"+artist+"</a>";
                        }
                    }

                    similarArtists+="<li>"+artist+"</li>";
                }
            }
        }
    }

    if (!similarArtists.isEmpty()) {
        similarArtists+="</ul></p>";
    }
    return !similarArtists.isEmpty();
}

void InfoPage::showArtist(const QUrl &url)
{
    if (QLatin1String("cantata")==url.scheme()) {
        #if QT_VERSION < 0x050000
        const QUrl &q=url;
        #else
        QUrlQuery q(url);
        #endif

        if (q.hasQueryItem("artist")) {
            emit findArtist(q.queryItemValue("artist"));
        }
    }
    #ifndef Q_OS_WIN
    else {
        QProcess::startDetached(QLatin1String("xdg-open"), QStringList() << url.toString());
    }
    #endif
}
