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
#include <QNetworkReply>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif
#include <QXmlStreamReader>
#include <QFile>

static QString localeFile() {
    return Utils::configDir(QString(), true)+"wikipedia-langs.xml.gz";
}

WikipediaSettings::WikipediaSettings(QWidget *p)
    : ContextSettings(p)
    , loaded(false)
    , job(0)
    , spinner(0)
{
    setupUi(this);
    connect(upButton, SIGNAL(clicked()), SLOT(moveUp()));
    connect(downButton, SIGNAL(clicked()), SLOT(moveDown()));
    connect(reload, SIGNAL(clicked()), SLOT(getLangs()));
    connect(addButton, SIGNAL(clicked()), SLOT(add()));
    connect(removeButton, SIGNAL(clicked()), SLOT(remove()));
    connect(langs, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(currentLangChanged(QListWidgetItem*)));
    connect(preferredLangs, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(currentPreferredLangChanged(QListWidgetItem*)));
    upButton->setIcon(Icon("go-up"));
    downButton->setIcon(Icon("go-down"));
    addButton->setIcon(Icon("list-add"));
    removeButton->setIcon(Icon("list-remove"));

    upButton->setEnabled(false);
    downButton->setEnabled(false);
    addButton->setEnabled(false);
    removeButton->setEnabled(false);
}

void WikipediaSettings::showEvent(QShowEvent *e)
{
    if (!loaded) {
        loaded=true;
        QByteArray data;
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
    if (!loaded) {
        return;
    }
    QStringList pref;
    for (int i=0; i<preferredLangs->count(); ++i) {
        pref.append(preferredLangs->item(i)->data(Qt::UserRole).toString());
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
    if (!spinner) {
        spinner=new Spinner(langs);
        spinner->setWidget(langs);
    }
    spinner->start();
    langs->clear();
    preferredLangs->clear();
    reload->setEnabled(false);
    cancel();
    QUrl url("https://en.wikipedia.org/w/api.php");
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
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    reply->deleteLater();
    if (reply!=job) {
        return;
    }
    job=0;
    parseLangs(reply->readAll());
}

void WikipediaSettings::parseLangs(const QByteArray &data)
{
    QStringList preferred=WikipediaEngine::getPreferedLangs();
    QXmlStreamReader xml(data);
    QMap<int, QListWidgetItem *> prefMap;

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if( xml.isStartElement() && QLatin1String("iw")==xml.name()) {
            const QXmlStreamAttributes &a = xml.attributes();
            if (a.hasAttribute(QLatin1String("prefix")) && a.hasAttribute(QLatin1String("language")) && a.hasAttribute(QLatin1String("url"))) {
                // The urlPrefix is the lang code infront of the wikipedia host
                // url. It is mostly the same as the "prefix" attribute but in
                // some weird cases they differ, so we can't just use "prefix".
                QString urlPrefix=QUrl(a.value(QLatin1String("url")).toString()).host().remove(QLatin1String(".wikipedia.org"));
                QString prefix=a.value(QLatin1String("prefix")).toString();
                QString entry=prefix+":"+urlPrefix;
                int index=preferred.indexOf(entry);
                QListWidgetItem *item = new QListWidgetItem(-1==index ? langs : preferredLangs);
                item->setText(QString("[%1] %2").arg(prefix).arg(a.value(QLatin1String("language")).toString()));
                item->setData(Qt::UserRole, entry);
                if (-1!=index) {
                    prefMap[index]=item;
                }
            }
        }
    }

    QMap<int, QListWidgetItem *>::ConstIterator it(prefMap.constBegin());
    QMap<int, QListWidgetItem *>::ConstIterator end(prefMap.constEnd());
    for (; it!=end; ++it) {
        int row=preferredLangs->row(it.value());
        if (row!=it.key()) {
            preferredLangs->insertItem(it.key(), preferredLangs->takeItem(row));
        }
    }
    QFile f(localeFile());
    QtIOCompressor compressor(&f);
    compressor.setStreamFormat(QtIOCompressor::GzipFormat);
    if (compressor.open(QIODevice::WriteOnly)) {
        compressor.write(data);
    }

    reload->setEnabled(true);
    if (spinner) {
        spinner->stop();
    }
}

void WikipediaSettings::currentLangChanged(QListWidgetItem *item)
{
    addButton->setEnabled(item);
}

void WikipediaSettings::currentPreferredLangChanged(QListWidgetItem *item)
{
    if (!item) {
        upButton->setEnabled(false);
        downButton->setEnabled(false);
        removeButton->setEnabled(false);
    } else {
        const int row = langs->row(item);
        upButton->setEnabled(row != 0);
        downButton->setEnabled(row != langs->count() - 1);
    }
    removeButton->setEnabled(item);
}

void WikipediaSettings::moveUp()
{
    move(-1);
}

void WikipediaSettings::moveDown()
{
    move(+1);
}

void WikipediaSettings::add()
{
    int index=langs->currentRow();
    if (index<0 || index>langs->count()) {
        return;
    }
    QListWidgetItem *item = langs->takeItem(index);
    QListWidgetItem *newItem=new QListWidgetItem(preferredLangs);
    newItem->setText(item->text());
    newItem->setData(Qt::UserRole, item->data(Qt::UserRole));
    delete item;
}

void WikipediaSettings::remove()
{
    int index=preferredLangs->currentRow();
    if (index<0 || index>preferredLangs->count()) {
        return;
    }
    QListWidgetItem *item = preferredLangs->takeItem(index);
    QListWidgetItem *newItem=new QListWidgetItem(langs);
    newItem->setText(item->text());
    newItem->setData(Qt::UserRole, item->data(Qt::UserRole));
    delete item;
}

void WikipediaSettings::move(int d)
{
    const int row = preferredLangs->currentRow();
    QListWidgetItem *item = preferredLangs->takeItem(row);
    preferredLangs->insertItem(row + d, item);
    preferredLangs->setCurrentRow(row + d);
}
