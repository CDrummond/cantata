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

#include <QBoxLayout>
#include <QComboBox>
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include "localize.h"
#include "infopage.h"
#include "combobox.h"
#include "networkaccessmanager.h"
#include "settings.h"
#ifndef Q_OS_WIN
#include "gtkproxystyle.h"
#endif
#include "qjson/parser.h"

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
            requestBio();
        }
    }
}

void InfoPage::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    if (reply==currentJob) {
        if (QNetworkReply::NoError==reply->error() && parseResponse(reply)) {
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
        text.replace("\n", "<br/><br/>");
        if (QLatin1String("last.fm")==site) {
            text.replace("  ", "<br/><br/>");
        }
        text.replace("<br/><br/><br/>", "<br/>");
    }
    QString site;
    QString text;
};

int value(const QString &site)
{
    return QLatin1String("wikipedia")==site
            ? 0
            : QLatin1String("last.fm")==site
              ? 1
              : 2;
}

bool InfoPage::parseResponse(QIODevice *dev)
{
    QMultiMap<int, Bio> biogs;
    QByteArray array=dev->readAll();
    QJson::Parser parser;
    bool ok=false;
    QVariantMap parsed=parser.parse(array, &ok).toMap();
    if (ok && parsed.contains("response")) {
        QVariantMap response=parsed["response"].toMap();
        if (response.contains("biographies")) {
            QVariantList biographies=response["biographies"].toList();
            foreach (const QVariant &b, biographies) {
                QVariantMap details=b.toMap();
                if (!details.isEmpty() && details.contains("text") && details.contains("site")) {
                    QString site=details["site"].toString();
                    biogs.insertMulti(value(site), Bio(site, details["text"].toString()));
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
        biographies[combo->count()]=it.value().text;
        combo->insertItem(combo->count(), i18n("Source: %1").arg(it.value().site));
    }

    return !biogs.isEmpty();
}
