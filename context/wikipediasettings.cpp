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

#include "wikipediasettings.h"
#include "wikipediaengine.h"
#include "networkaccessmanager.h"
#include "localize.h"
#include "icon.h"
#include "spinner.h"
#include "settings.h"
#include "qtiocompressor/qtiocompressor.h"
#include "utils.h"
#include "action.h"
#include "thread.h"
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QFile>

static const QString constFileName=QLatin1String("wikipedia-available.xml.gz");

static QString localeFile()
{
    return Utils::dataDir(QString(), true)+constFileName;
}

// Move files from previous ~/.config/cantata to ~/.local/share/cantata
static void moveToNewLocation()
{
    #if !defined Q_OS_WIN && !defined Q_OS_MAC // Not required for windows - as already stored in data location!
    // Can't reliable check version here, as migh tnot have opened the wiki settings before version was changed...
    Utils::moveFile(Utils::configDir(QString())+constFileName, Utils::dataDir(QString(), true)+constFileName);
    #endif
}

WikipediaLoader::WikipediaLoader()
    : QObject(0)
{
    thread=new Thread(metaObject()->className());
    moveToThread(thread);
    thread->start();
}

WikipediaLoader::~WikipediaLoader()
{
    thread->stop();
}

void WikipediaLoader::load(const QByteArray &data)
{
    QStringList preferred=WikipediaEngine::getPreferedLangs();
    QXmlStreamReader xml(data);

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if( xml.isStartElement() && QLatin1String("iw")==xml.name()) {
            const QXmlStreamAttributes &a = xml.attributes();
            if (a.hasAttribute(QLatin1String("prefix")) && a.hasAttribute(QLatin1String("language")) && a.hasAttribute(QLatin1String("url"))) {
                // The urlPrefix is the lang code infront of the wikipedia host
                // url. It is mostly the same as the "prefix" attribute but in
                // some weird cases they differ, so we can't just use "prefix".
                QString prefix=a.value(QLatin1String("prefix")).toString();
                QString urlPrefix=QUrl(a.value(QLatin1String("url")).toString()).host().remove(QLatin1String(".wikipedia.org"));
                emit entry(prefix, urlPrefix, a.value(QLatin1String("language")).toString(), preferred.indexOf(prefix+":"+urlPrefix));
            }
        }
    }
    emit finished();
}

WikipediaSettings::WikipediaSettings(QWidget *p)
    : ToggleList(p)
    , state(Initial)
    , job(0)
    , spinner(0)
    , loader(0)
{
    label->setText(i18n("Choose the wikipedia languages you want to use when searching for artist and album information."));
    reload=new Action(i18n("Reload"), this);
    connect(reload, SIGNAL(triggered(bool)), this, SLOT(getLangs()));
    available->addAction(reload);
    available->setContextMenuPolicy(Qt::ActionsContextMenu);
}

WikipediaSettings::~WikipediaSettings()
{
    if (loader) {
        loader->deleteLater();
    }
}

void WikipediaSettings::showEvent(QShowEvent *e)
{
    if (Initial==state) {
        state=Loading;
        QByteArray data;
        moveToNewLocation();
        QString fileName=localeFile();
        if (QFile::exists(fileName)) {
            QFile f(fileName);
            QtIOCompressor compressor(&f);
            compressor.setStreamFormat(QtIOCompressor::GzipFormat);
            if (compressor.open(QIODevice::ReadOnly)) {
                data=compressor.readAll();
            }
        }

        if (data.isEmpty()) {
            getLangs();
        } else {
            showSpinner();
            parseLangs(data);
        }
    }
    QWidget::showEvent(e);
}

void WikipediaSettings::load()
{
}

void WikipediaSettings::save()
{
    if (Loaded!=state) {
        return;
    }
    QStringList pref;
    for (int i=0; i<selected->count(); ++i) {
        pref.append(selected->item(i)->data(Qt::UserRole).toString());
    }
    if (pref.isEmpty()) {
        pref.append("en:en");
    }
    Settings::self()->saveWikipediaLangs(pref);
    WikipediaEngine::setPreferedLangs(pref);
}

void WikipediaSettings::cancel()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(parseLangs()));
        job->deleteLater();
        job=0;
    }
}

void WikipediaSettings::getLangs()
{
    state=Loading;
    showSpinner();
    available->clear();
    selected->clear();
    reload->setEnabled(false);
    cancel();
    QUrl url("http://en.wikipedia.org/w/api.php");
    #if QT_VERSION < 0x050000
    QUrl &q=url;
    #else
    QUrlQuery q;
    #endif

    q.addQueryItem(QLatin1String("action"), QLatin1String("query"));
    q.addQueryItem(QLatin1String("meta"), QLatin1String("siteinfo"));
    q.addQueryItem(QLatin1String("siprop"), QLatin1String("interwikimap"));
    q.addQueryItem(QLatin1String("sifilteriw"), QLatin1String("local"));
    q.addQueryItem(QLatin1String("format"), QLatin1String("xml"));

    #if QT_VERSION >= 0x050000
    url.setQuery(q);
    #endif

    job=NetworkAccessManager::self()->get(url);
    connect(job, SIGNAL(finished()), this, SLOT(parseLangs()));
}

void WikipediaSettings::parseLangs()
{
    NetworkJob *reply = qobject_cast<NetworkJob*>(sender());
    if (!reply) {
        return;
    }
    reload->setEnabled(true);
    reply->deleteLater();
    if (reply!=job) {
        return;
    }
    job=0;
    QByteArray data=reply->readAll();
    parseLangs(data);
    QFile f(localeFile());
    QtIOCompressor compressor(&f);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (compressor.open(QIODevice::WriteOnly)) {
        compressor.write(data);
    }
}

void WikipediaSettings::parseLangs(const QByteArray &data)
{
    prefMap.clear();
    if (!loader) {
        loader=new WikipediaLoader();
        connect(loader, SIGNAL(entry(QString,QString,QString,int)), SLOT(addEntry(QString,QString,QString,int)));
        connect(loader, SIGNAL(finished()), SLOT(loaderFinished()));
        connect(this, SIGNAL(load(QByteArray)), loader, SLOT(load(QByteArray)));
    }
    emit load(data);
}

void WikipediaSettings::addEntry(const QString &prefix, const QString &urlPrefix, const QString &lang, int prefIndex)
{
    QString entry=prefix+":"+urlPrefix;
    QListWidgetItem *item = new QListWidgetItem(-1==prefIndex ? available : selected);
    item->setText(QString("[%1] %2").arg(prefix).arg(lang));
    item->setData(Qt::UserRole, entry);
    if (-1!=prefIndex) {
        prefMap[prefIndex]=item;
    }
}

void WikipediaSettings::loaderFinished()
{
    QMap<int, QListWidgetItem *>::ConstIterator it(prefMap.constBegin());
    QMap<int, QListWidgetItem *>::ConstIterator end(prefMap.constEnd());
    for (; it!=end; ++it) {
        int row=selected->row(it.value());
        if (row!=it.key()) {
            selected->insertItem(it.key(), selected->takeItem(row));
        }
    }

    hideSpinner();
    state=Loaded;
}

void WikipediaSettings::showSpinner()
{
    if (!spinner) {
        spinner=new Spinner(available);
        spinner->setWidget(available);
    }
    spinner->start();
}

void WikipediaSettings::hideSpinner()
{
    if (spinner) {
        spinner->stop();
    }
}
