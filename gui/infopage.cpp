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
#include "combobox.h"
#include "networkaccessmanager.h"
#include "settings.h"
#ifndef Q_OS_WIN
#include "gtkproxystyle.h"
#endif
#include "qjson/parser.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static const QLatin1String constApiKey("N5JHVHNG0UOZZIDVT");
static const QLatin1String constBioUrl("http://developer.echonest.com/api/v4/artist/biographies");

InfoPage::InfoPage(QWidget *parent)
    : QWidget(parent)
    , currentJob(0)
{
    QVBoxLayout *vlayout=new QVBoxLayout(this);
    text=new TextBrowser(this);
    combo=new ComboBox(this);
    vlayout->addWidget(text);
    vlayout->addWidget(combo);
    vlayout->setMargin(0);
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(setBio()));
    #ifndef Q_OS_WIN
    combo->setProperty(GtkProxyStyle::constSlimComboProperty, true);
    #endif
    setBgndImageEnabled(Settings::self()->lyricsBgnd());
    text->setZoom(Settings::self()->infoZoom());
    connect(Covers::self(), SIGNAL(artistImage(Song,QImage)), SLOT(artistImage(Song,QImage)));
}

void InfoPage::saveSettings()
{
    Settings::self()->saveInfoZoom(text->zoom());
}

void InfoPage::update(const Song &s)
{
    Song song=s;
    if (song.isVariousArtists()) {
        song.revertVariousArtists();
    }
    if (song.artist!=currentSong.artist) {
        currentSong=song;
        combo->clear();
        biographies.clear();
        if (currentSong.isEmpty()) {
            text->setText(QString());
        } else {
            text->setHtml("<i>Retrieving...</i>");
            text->setImage(QImage());

            Song s;
            s.albumartist=currentSong.artist;
            s.file=currentSong.file;
            Covers::self()->requestCover(s, true);
            requestBio();
        }
    }
}

void InfoPage::artistImage(const Song &song, const QImage &img)
{
    if (song.albumartist==currentSong.artist) {
        text->setImage(img);
    }
}

void InfoPage::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (reply==currentJob) {
        if (QNetworkReply::NoError==reply->error() && parseResponse(reply->readAll())) {
            setBio();
        } else {
            text->setHtml(i18n(("<i><Failed to download information!</i>")));
        }
        reply->deleteLater();
        currentJob=0;
    }
}

void InfoPage::setBio()
{
    int bio=combo->currentIndex();
    if (biographies.contains(bio)) {
        text->setHtml(biographies[bio]);
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

    currentJob=NetworkAccessManager::self()->get(url);
    connect(currentJob, SIGNAL(finished()), this, SLOT(handleReply()));
}

void InfoPage::abort()
{
    if (currentJob) {
        disconnect(currentJob, SIGNAL(finished()), this, SLOT(handleReply()));
        currentJob->abort();
        currentJob=0;
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

bool InfoPage::parseResponse(const QByteArray &resp)
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
        biographies[combo->count()]=it.value().text;
        combo->insertItem(combo->count(), i18n("Source: %1").arg(it.value().site));
    }

    return !biogs.isEmpty();
}
