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

#include "streamspage.h"
#include "streamdialog.h"
#include "streamcategorydialog.h"
#include "mpdconnection.h"
#include "messagebox.h"
#include "localize.h"
#include "icons.h"
#include "mainwindow.h"
#include "action.h"
#include "actioncollection.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "streamsmodel.h"
#include <QToolButton>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QFileDialog>
#include <QDir>
#endif
#include <QAction>
#include <QMenu>
#include <QXmlStreamReader>
#include <QFileInfo>

static QString fixSingleGenre(const QString &g)
{
    if (g.length()) {
        QString genre=Song::capitalize(g);
        genre[0]=genre[0].toUpper();
        genre=genre.trimmed();
        genre=genre.replace(QLatin1String("Afrocaribbean"), QLatin1String("Afro-Caribbean"));
        genre=genre.replace(QLatin1String("Afro Caribbean"), QLatin1String("Afro-Caribbean"));
        if (genre.length() < 3 ||
            QLatin1String("The")==genre || QLatin1String("All")==genre ||
            QLatin1String("Various")==genre || QLatin1String("Unknown")==genre ||
            QLatin1String("Misc")==genre || QLatin1String("Mix")==genre || QLatin1String("100%")==genre ||
            genre.contains("ÃÂ") || // Broken unicode.
            genre.contains(QRegExp("^#x[0-9a-f][0-9a-f]"))) { // Broken XML entities.
                return QString();
        }

        if (genre==QLatin1String("R&B") || genre==QLatin1String("R B") || genre==QLatin1String("Rnb") || genre==QLatin1String("RnB")) {
            return QLatin1String("R&B");
        }
        if (genre==QLatin1String("Classic") || genre==QLatin1String("Classical")) {
            return QLatin1String("Classical");
        }
        if (genre==QLatin1String("Christian") || genre.startsWith(QLatin1String("Christian "))) {
            return QLatin1String("Christian");
        }
        if (genre==QLatin1String("Rock") || genre.startsWith(QLatin1String("Rock "))) {
            return QLatin1String("Rock");
        }
        if (genre==QLatin1String("Electronic") || genre==QLatin1String("Electronica") || genre==QLatin1String("Electric")) {
            return QLatin1String("Electronic");
        }
        if (genre==QLatin1String("Easy") || genre==QLatin1String("Easy Listening")) {
            return QLatin1String("Easy Listening");
        }
        if (genre==QLatin1String("Hit") || genre==QLatin1String("Hits") || genre==QLatin1String("Easy listening")) {
            return QLatin1String("Hits");
        }
        if (genre==QLatin1String("Hip") || genre==QLatin1String("Hiphop") || genre==QLatin1String("Hip Hop") || genre==QLatin1String("Hop Hip")) {
            return QLatin1String("Hip Hop");
        }
        if (genre==QLatin1String("News") || genre==QLatin1String("News talk")) {
            return QLatin1String("News");
        }
        if (genre==QLatin1String("Top40") || genre==QLatin1String("Top 40") || genre==QLatin1String("40Top") || genre==QLatin1String("40 Top")) {
            return QLatin1String("Top 40");
        }

        QStringList small=QStringList() << QLatin1String("Adult Contemporary") << QLatin1String("Alternative")
                                       << QLatin1String("Community Radio") << QLatin1String("Local Service")
                                       << QLatin1String("Multiultural") << QLatin1String("News")
                                       << QLatin1String("Student") << QLatin1String("Urban");

        foreach (const QString &s, small) {
            if (genre==s || genre.startsWith(s+" ") || genre.endsWith(" "+s)) {
                return s;
            }
        }

        // Convert XX's to XXs
        if (genre.contains(QRegExp("^[0-9]0's$"))) {
            genre=genre.remove('\'');
        }
        if (genre.length()>25 && (0==genre.indexOf(QRegExp("^[0-9]0s ")) || 0==genre.indexOf(QRegExp("^[0-9]0 ")))) {
            int pos=genre.indexOf(' ');
            if (pos>1) {
                genre=genre.left(pos);
            }
        }
        // Convert 80 -> 80s.
        return genre.contains(QRegExp("^[0-9]0$")) ? genre + 's' : genre;
    }
    return g;
}

static QString fixGenres(const QString &genre)
{
    QString g(genre);
    int pos=g.indexOf("<br");
    if (pos>3) {
        g=g.left(pos);
    }
    pos=g.indexOf("(");
    if (pos>3) {
        g=g.left(pos);
    }

    g=Song::capitalize(g);
    QStringList genres=g.split('|', QString::SkipEmptyParts);
    QStringList allGenres;

    foreach (const QString &genre, genres) {
        allGenres+=genre.split('/', QString::SkipEmptyParts);
    }

    QStringList fixed;
    foreach (const QString &genre, allGenres) {
        fixed.append(fixSingleGenre(genre));
    }
    return fixed.join(StreamsModel::constGenreSeparator);
}

static void trimGenres(QMultiHash<QString, StreamsModel::StreamItem *> &genres)
{
    QSet<QString> genreSet = genres.keys().toSet();
    foreach (const QString &genre, genreSet) {
        if (genres.count(genre) < 3) {
            const QList<StreamsModel::StreamItem *> &smallGnre = genres.values(genre);
            foreach (StreamsModel::StreamItem* s, smallGnre) {
                s->genres.remove(genre);
            }
        }
    }
}

static QList<StreamsModel::StreamItem *> parseIceCast(QIODevice *dev)
{
    QList<StreamsModel::StreamItem *> streams;
    QString name;
    QUrl url;
    QString genre;
    QSet<QString> names;
    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    int level=0;
    QXmlStreamReader doc(dev);

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("entry")==doc.name()) {
                name=QString();
                url=QUrl();
                genre=QString();
            } else if (3==level) {
                if (QLatin1String("server_name")==doc.name()) {
                    name=doc.readElementText();
                    --level;
                } else if (QLatin1String("genre")==doc.name()) {
                    genre=fixGenres(doc.readElementText());
                    --level;
                } else if (QLatin1String("listen_url")==doc.name()) {
                    url=QUrl(doc.readElementText());
                    --level;
                }
            }
        } else if (doc.isEndElement()) {
            if (2==level && QLatin1String("entry")==doc.name() && !name.isEmpty() && url.isValid() && !names.contains(name)) {
                StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                streams.append(item);
                foreach (const QString &genre, item->genres) {
                    genres.insert(genre, item);
                }
                names.insert(item->name);
            }
            --level;
        }
    }
    trimGenres(genres);
    return streams;
}

static QList<StreamsModel::StreamItem *> parseSomaFm(QIODevice *dev)
{
    QList<StreamsModel::StreamItem *> streams;
    QSet<QString> names;
    QString streamFormat;
    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    QString name;
    QUrl url;
    QString genre;
    int level=0;
    QXmlStreamReader doc(dev);

    while (!doc.atEnd()) {
        doc.readNext();

        if (doc.isStartElement()) {
            ++level;
            if (2==level && QLatin1String("channel")==doc.name()) {
                name=QString();
                url=QUrl();
                genre=QString();
                streamFormat=QString();
            } else if (3==level) {
                if (QLatin1String("title")==doc.name()) {
                    name=doc.readElementText();
                    --level;
                } else if (QLatin1String("genre")==doc.name()) {
                    genre=fixGenres(doc.readElementText());
                    --level;
                } else if (QLatin1String("fastpls")==doc.name()) {
                    if (streamFormat.isEmpty() || QLatin1String("mp3")!=streamFormat) {
                        streamFormat=doc.attributes().value("format").toString();
                        url=QUrl(doc.readElementText());
                        --level;
                    }
                }
            }
        } else if (doc.isEndElement()) {
            if (2==level && QLatin1String("channel")==doc.name() && !name.isEmpty() && url.isValid() && !names.contains(name)) {
                StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                streams.append(item);
                foreach (const QString &genre, item->genres) {
                    genres.insert(genre, item);
                }
                names.insert(item->name);
            }
            --level;
        }
    }

    trimGenres(genres);
    return streams;
}

struct Stream {
    enum Format {
        AAC,
        MP3,
        WMA,
        Unknown
    };

    Stream() : format(Unknown), bitrate(0) { }
    bool operator<(const Stream &o) const {
        return weight()>o.weight();
    }

    int weight() const {
        return ((format&0x0f)<<8)+(bitrate&0xff);
    }

    void setFormat(const QString &f) {
        if (QLatin1String("mp3")==f.toLower()) {
            format=MP3;
        } else if (QLatin1String("aacplus")==f.toLower()) {
            format=AAC;
        } else if (QLatin1String("windows media")==f.toLower()) {
            format=WMA;
        } else {
            format=Unknown;
        }
    }

    QUrl url;
    Format format;
    unsigned int bitrate;
};

struct StationEntry {
    StationEntry() { clear(); }
    void clear() { name=location=comment=QString(); streams.clear(); }
    QString name;
    QString location;
    QString comment;
    QList<Stream> streams;
};

static QString getString(QString &str, const QString &start, const QString &end)
{
    QString rv;
    int b=str.indexOf(start);
    int e=-1==b ? -1 : str.indexOf(end, b+start.length());
    if (-1!=e) {
        rv=str.mid(b+start.length(), e-(b+start.length()));
        str=str.mid(e+end.length());
    }
    return rv;
}

static QList<StreamsModel::StreamItem *> parseRadio(QIODevice *dev)
{
    QList<StreamsModel::StreamItem *> streams;
//    QMultiHash<QString, StreamsModel::StreamItem *> genres;
    QSet<QString> names;

    if (dev) {
        StationEntry entry;

        while (!dev->atEnd()) {
            QString line=dev->readLine().trimmed().replace("> <", "><").replace("<td><b><a href", "<td><a href").replace("</b></a></b>", "</b></a>");
            if ("<tr>"==line) {
                entry.clear();
            } else if (line.startsWith("<td><a href=")) {
                if (line.endsWith("</b></a></td>")) {
                    // Station name
                    if (entry.name.isEmpty()) {
                        entry.name=getString(line, "<b>", "</b>");
                    }
                } else {
                    // Station URLs...
                    QString url;
                    QString bitrate;
                    int idx=0;
                    do {
                        url=getString(line, "href=\"", "\"");
                        bitrate=getString(line, ">", " Kbps");
                        if (!url.isEmpty() && !bitrate.isEmpty()) {
                            if (idx<entry.streams.count()) {
                                entry.streams[idx].url=url;
                                entry.streams[idx].bitrate=bitrate.toUInt();
                                idx++;
                            }
                        }
                    } while (!url.isEmpty() && !bitrate.isEmpty());
                }
            } else if (line.startsWith("<td><img src")) {
                // Station formats...
                QString format;
                do {
                    format=getString(line, "alt=\"", "\"");
                    if (!format.isEmpty()) {
                        Stream stream;
                        stream.setFormat(format);
                        entry.streams.append(stream);
                    }
                } while (!format.isEmpty());
            } else if (line.startsWith("<td>")) {
                if (entry.location.isEmpty()) {
                    entry.location=getString(line, "<td>", "</td>");
                } else {
                    entry.comment=getString(line, "<td>", "</td>");
                }
            } else if ("</tr>"==line) {
                if (entry.streams.count()) {
                    QString name=QLatin1String("National")==entry.location ? entry.name : (entry.name+" ("+entry.location+")");
                    QUrl url=entry.streams.at(0).url;

                    if (!names.contains(name) && !name.isEmpty() && url.isValid()) {
                        qSort(entry.streams);
                        QString genre=fixGenres(entry.comment);
                        StreamsModel::StreamItem *item=new StreamsModel::StreamItem(name, genre, QString(), url);
                        streams.append(item);
//                        foreach (const QString &genre, item->genres) {
//                            genres.insert(genre, item);
//                        }
                        names.insert(item->name);
                    }
                }
            }
        }
    }

//    trimGenres(genres);
    return streams;
}

static const char * constUrlProperty("url");

StreamsPage::StreamsPage(MainWindow *p)
    : QWidget(p)
    , enabled(false)
    , mw(p)
{
    setupUi(this);
    importAction = ActionCollection::get()->createAction("importstreams", i18n("Import Streams"), "document-import");
    exportAction = ActionCollection::get()->createAction("exportstreams", i18n("Export Streams"), "document-export");
    addAction = ActionCollection::get()->createAction("addstream", i18n("Add Stream"), "list-add");
    editAction = ActionCollection::get()->createAction("editstream", i18n("Edit"), Icons::editIcon);

    QMenu *importMenu=new QMenu(this);
    importFileAction=importMenu->addAction("From Cantata File");
    initWebStreams();
    int radio=0;
    foreach (const WebStream &ws, webStreams) {
        if (ws.type==WebStream::WS_Radio) {
            radio++;
        } else {
            QAction *act=importMenu->addAction(i18n("From %1").arg(ws.name));
            act->setProperty(constUrlProperty, ws.url);
            connect(act, SIGNAL(triggered(bool)), this, SLOT(importWebStreams()));
        }
    }

    if (radio) {
        QAction *radioAction=importMenu->addAction(i18n("Radio Stations"));
        QMenu *radioMenu=new QMenu(this);
        radioAction->setMenu(radioMenu);

        QMap<QString, QMenu *> regions;
        foreach (const WebStream &ws, webStreams) {
            if (ws.type==WebStream::WS_Radio) {
                QMenu *menu=radioMenu;
                if (!ws.region.isEmpty()) {
                    if (!regions.contains(ws.region)) {
                        QAction *regionAction=radioMenu->addAction(ws.region);
                        regions.insert(ws.region, new QMenu(this));
                        regionAction->setMenu(regions[ws.region]);
                    }
                    menu=regions[ws.region];
                }
                QAction *act=menu->addAction(ws.name);
                act->setProperty(constUrlProperty, ws.url);
                connect(act, SIGNAL(triggered(bool)), this, SLOT(importWebStreams()));
            }
        }
    }

    importAction->setMenu(importMenu);
    replacePlayQueue->setDefaultAction(p->replacePlayQueueAction);
//     connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(view, SIGNAL(searchItems()), this, SLOT(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), SLOT(controlActions()));
    connect(addAction, SIGNAL(triggered(bool)), this, SLOT(add()));
    connect(editAction, SIGNAL(triggered(bool)), this, SLOT(edit()));
    connect(importFileAction, SIGNAL(triggered(bool)), this, SLOT(importXml()));
    connect(exportAction, SIGNAL(triggered(bool)), this, SLOT(exportXml()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(StreamsModel::self(), SIGNAL(updateGenres(const QSet<QString> &)), genreCombo, SLOT(update(const QSet<QString> &)));
    connect(StreamsModel::self(), SIGNAL(error(const QString &)), mw, SLOT(showError(QString)));
    connect(StreamsModel::self(), SIGNAL(downloading(bool)), this, SLOT(downloading(bool)));
    connect(MPDConnection::self(), SIGNAL(dirChanged()), SLOT(mpdDirChanged()));
    Icon::init(menuButton);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu=new QMenu(this);
    menu->addAction(addAction);
    menu->addAction(p->removeAction);
    menu->addAction(editAction);
    menu->addAction(importAction);
    menu->addAction(exportAction);
    menuButton->setMenu(menu);
    menuButton->setIcon(Icons::menuIcon);
    menuButton->setToolTip(i18n("Other Actions"));
    Icon::init(replacePlayQueue);

    view->setTopText(i18n("Streams"));
    view->setUniformRowHeights(true);
    view->setAcceptDrops(true);
    view->setDragDropMode(QAbstractItemView::DragDrop);
//     view->addAction(p->addToPlaylistAction);
    view->addAction(p->replacePlayQueueAction);
    view->addAction(p->addWithPriorityAction);
    view->addAction(editAction);
    view->addAction(exportAction);
    view->addAction(p->removeAction);
    proxy.setSourceModel(StreamsModel::self());
    view->setModel(&proxy);
    view->setDeleteAction(p->removeAction);
    view->init(p->replacePlayQueueAction, 0);

    infoLabel->hide();
    infoIcon->hide();
    int iconSize=Icon::stdSize(QApplication::fontMetrics().height());
    infoIcon->setPixmap(Icon("locked").pixmap(iconSize, iconSize));
}

StreamsPage::~StreamsPage()
{
    foreach (const WebStream &ws, webStreams) {
        if (ws.job) {
            disconnect(ws.job, SIGNAL(finished()), this, SLOT(downloadFinished()));
            ws.job->deleteLater();
            ws.job=0;
        }
    }
}

void StreamsPage::setEnabled(bool e)
{
    if (e==enabled) {
        return;
    }
    enabled=e;
}

void StreamsPage::mpdDirChanged()
{
    if (Settings::self()->storeStreamsInMpdDir()) {
        refresh();
    }
}

void StreamsPage::checkWriteable()
{
    bool isHttp=StreamsModel::dir().startsWith("http:/");
    bool dirWritable=!isHttp && QFileInfo(StreamsModel::dir()).isWritable();
    if (dirWritable) {
        infoLabel->hide();
        infoIcon->hide();
    } else {
        infoLabel->setVisible(true);
        infoLabel->setText(isHttp ? i18n("Streams from HTTP server") : i18n("Music folder not writeable."));
        infoIcon->setVisible(true);
    }
    if (dirWritable!=StreamsModel::self()->isWritable()) {
        StreamsModel::self()->setWritable(dirWritable);
        controlActions();
    }
}

void StreamsPage::refresh()
{
    if (enabled) {
        checkWriteable();
        view->setLevel(0);
        StreamsModel::self()->reload();
        exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
    }
}

void StreamsPage::save()
{
    StreamsModel::self()->save(true);
}

void StreamsPage::addSelectionToPlaylist(bool replace, quint8 priorty)
{
    addItemsToPlayQueue(view->selectedIndexes(), replace, priorty);
}

void StreamsPage::addItemsToPlayQueue(const QModelIndexList &indexes, bool replace, quint8 priorty)
{
    if (0==indexes.size()) {
        return;
    }

    QModelIndexList sorted=indexes;
    qSort(sorted);

    QModelIndexList mapped;
    foreach (const QModelIndex &idx, sorted) {
        mapped.append(proxy.mapToSource(idx));
    }

    QStringList files=StreamsModel::self()->filenames(mapped, true);

    if (!files.isEmpty()) {
        emit add(files, replace, priorty);
        view->clearSelection();
    }
}

void StreamsPage::itemDoubleClicked(const QModelIndex &index)
{
    if (!static_cast<StreamsModel::Item *>(proxy.mapToSource(index).internalPointer())->isCategory()) {
        QModelIndexList indexes;
        indexes.append(index);
        addItemsToPlayQueue(indexes, false);
    }
}

void StreamsPage::downloading(bool dl)
{
    if (dl) {
        view->showSpinner();
    } else {
        view->hideSpinner();
    }
}

void StreamsPage::downloadFinished()
{
    QNetworkReply *reply=qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    bool busy=false;
    QMap<QUrl, WebStream>::Iterator it(webStreams.begin());
    QMap<QUrl, WebStream>::Iterator end(webStreams.end());
    QMap<QUrl, WebStream>::Iterator stream=end;

    for (; it!=end; ++it) {
        if ((*it).job==reply) {
            stream=it;
        } else if ((*it).job) {
            busy=true;
        }
    }

    if (stream!=end) {
        if(QNetworkReply::NoError==reply->error()) {
            QList<StreamsModel::StreamItem *> streams;
            switch ((*stream).type) {
            case WebStream::WS_IceCast:
                streams=parseIceCast(reply);
                break;
            case WebStream::WS_SomaFm:
                streams=parseSomaFm(reply);
                break;
            case WebStream::WS_Radio:
                streams=parseRadio(reply);
                break;
            default:
                break;
            }

            if (streams.isEmpty()) {
                MessageBox::error(this, i18nc("message \n url", "No streams downloaded from %1\n(%2)").arg((*stream).name).arg((*stream).url.toString()));
            } else {
                StreamsModel::self()->add((*stream).name, streams);
            }
        } else {
            MessageBox::error(this, i18nc("message \n url", "Failed to download streams from %1\n(%2)").arg((*stream).name).arg((*stream).url.toString()));
        }
        (*stream).job=0;
    }

    if (!busy) {
        view->hideSpinner();
    }
    reply->deleteLater();
}

void StreamsPage::importXml()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.cantata|Cantata Streams"), this, i18n("Import Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Import Streams"), QDir::homePath(), i18n("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!StreamsModel::self()->import(fileName)) {
        MessageBox::error(this, i18n("Failed to import <b>%1</b>!<br/>Please check this is of the correct type.").arg(fileName));
    }
}

void StreamsPage::exportXml()
{
    QModelIndexList selected=view->selectedIndexes();
    QSet<StreamsModel::Item *> items;
    QSet<StreamsModel::Item *> categories;
    foreach (const QModelIndex &idx, selected) {
        QModelIndex i=proxy.mapToSource(idx);
        StreamsModel::Item *itm=static_cast<StreamsModel::Item *>(i.internalPointer());
        if (itm->isCategory()) {
            if (!categories.contains(itm)) {
                categories.insert(itm);
            }
            foreach (StreamsModel::StreamItem *s, static_cast<StreamsModel::CategoryItem *>(itm)->streams) {
                items.insert(s);
            }
        } else {
            items.insert(itm);
            categories.insert(static_cast<StreamsModel::StreamItem *>(itm)->parent);
        }
    }

    QString name;

    if (1==categories.count()) {
        name=static_cast<StreamsModel::StreamItem *>(*(categories.begin()))->name+QLatin1String(".cantata");
    }
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getSaveFileName(name, i18n("*.cantata|Cantata Streams"), this, i18n("Export Streams"));
    #else
    QString fileName=QFileDialog::getSaveFileName(this, i18n("Export Streams"), name, i18n("Cantata Streams (*.cantata)"));
    #endif

    if (fileName.isEmpty()) {
        return;
    }

    if (!StreamsModel::self()->save(fileName, categories+items)) {
        MessageBox::error(this, i18n("Failed to create <b>%1</b>!").arg(fileName));
    }
}

void StreamsPage::add()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    StreamDialog dlg(getCategories(), getGenres(), this);

    if (QDialog::Accepted==dlg.exec()) {
        QString name=dlg.name();
        QString url=dlg.url();
        QString cat=dlg.category();
        QString existing=StreamsModel::self()->name(cat, url);

        if (!existing.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1</b>").arg(existing));
            return;
        }

        if (!StreamsModel::self()->add(cat, name, dlg.genre(), dlg.icon(), url)) {
            MessageBox::error(this, i18n("A stream named <b>%1</b> already exists!").arg(name));
        }
    }
    exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
}

void StreamsPage::removeItems()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    QStringList streams;
    QModelIndexList selected = view->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    bool haveStreams=false;
    bool haveCategories=false;
    foreach(const QModelIndex &index, selected) {
        if (static_cast<StreamsModel::Item *>(proxy.mapToSource(index).internalPointer())->isCategory()) {
            haveCategories=true;
        } else {
             haveStreams=true;
        }

        if (haveStreams && haveCategories) {
            break;
        }
    }

    QModelIndex firstIndex=proxy.mapToSource(selected.first());
    QString firstName=StreamsModel::self()->data(firstIndex, Qt::DisplayRole).toString();
    QString message;

    if (selected.size()>1) {
        if (haveStreams && haveCategories) {
            message=i18n("Are you sure you wish to remove the selected categories &streams?");
        } else if (haveStreams) {
            message=i18n("Are you sure you wish to remove the %1 selected streams?").arg(selected.size());
        } else {
            message=i18n("Are you sure you wish to remove the %1 selected categories (and their streams)?").arg(selected.size());
        }
    } else if (haveStreams) {
        message=i18n("Are you sure you wish to remove <b>%1</b>?").arg(firstName);
    } else {
        message=i18n("Are you sure you wish to remove the <b>%1</b> category (and its streams)?").arg(firstName);
    }

    if (MessageBox::No==MessageBox::warningYesNo(this, message, i18n("Remove?"))) {
        return;
    }

    // Ensure that if we have a category selected, we dont also try to remove one of its children
    QSet<StreamsModel::CategoryItem *> removeCategories;
    QList<StreamsModel::StreamItem *> removeStreams;
    QModelIndexList remove;
    //..obtain catagories to remove...
    foreach(QModelIndex index, selected) {
        QModelIndex idx=proxy.mapToSource(index);
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(idx.internalPointer());

        if (item->isCategory()) {
            removeCategories.insert(static_cast<StreamsModel::CategoryItem *>(item));
        }
    }
    // Obtain streams in non-selected categories...
    foreach(QModelIndex index, selected) {
        QModelIndex idx=proxy.mapToSource(index);
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(idx.internalPointer());

        if (!item->isCategory()) {
            removeStreams.append(static_cast<StreamsModel::StreamItem *>(item));
        }
    }

    foreach (StreamsModel::CategoryItem *i, removeCategories) {
        StreamsModel::self()->removeCategory(i);
    }

    foreach (StreamsModel::StreamItem *i, removeStreams) {
        StreamsModel::self()->removeStream(i);
    }
    StreamsModel::self()->updateGenres();
    exportAction->setEnabled(StreamsModel::self()->rowCount()>0);
}

void StreamsPage::edit()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }
    QStringList streams;
    QModelIndexList selected = view->selectedIndexes();

    if (1!=selected.size()) {
        return;
    }

    QModelIndex index=proxy.mapToSource(selected.first());
    StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
    QString name=item->name;
    QString icon=item->icon;

    if (item->isCategory()) {
        StreamCategoryDialog dlg(getCategories(), this);
        dlg.setEdit(name, icon);
        if (QDialog::Accepted==dlg.exec()) {
            StreamsModel::self()->editCategory(index, dlg.name(), dlg.icon());
        }
        return;
    }

    StreamDialog dlg(getCategories(), getGenres(), this);
    StreamsModel::StreamItem *stream=static_cast<StreamsModel::StreamItem *>(item);
    QString url=stream->url.toString();
    QString cat=stream->parent->name;
    QString genre=stream->genreString();

    dlg.setEdit(cat, name, genre, icon, url);

    if (QDialog::Accepted==dlg.exec()) {
        QString newName=dlg.name();
        #ifdef ENABLE_KDE_SUPPORT
        QString newIcon=dlg.icon();
        #else
        QString newIcon;
        #endif
        QString newUrl=dlg.url();
        QString newCat=dlg.category();
        QString newGenre=dlg.genre();
        QString existingNameForUrl=newUrl!=url ? StreamsModel::self()->name(newCat, newUrl) : QString();
//
        if (!existingNameForUrl.isEmpty()) {
            MessageBox::error(this, i18n("Stream already exists!<br/><b>%1 (%2)</b>").arg(existingNameForUrl).arg(newCat));
        } else if (newName!=name && StreamsModel::self()->entryExists(newCat, newName)) {
            MessageBox::error(this, i18n("A stream named <b>%1 (%2)</b> already exists!").arg(newName).arg(newCat));
        } else {
            StreamsModel::self()->editStream(index, cat, newCat, newName, newGenre, newIcon, newUrl);
        }
    }
}

void StreamsPage::controlActions()
{
    QModelIndexList selected=view->selectedIndexes();
    editAction->setEnabled(1==selected.size() && StreamsModel::self()->isWritable());
    mw->removeAction->setEnabled(selected.count() && StreamsModel::self()->isWritable());
    addAction->setEnabled(StreamsModel::self()->isWritable());
    exportAction->setEnabled(StreamsModel::self()->rowCount());
    importAction->setEnabled(StreamsModel::self()->isWritable());
    mw->replacePlayQueueAction->setEnabled(selected.count());
    mw->addWithPriorityAction->setEnabled(selected.count());
}

void StreamsPage::searchItems()
{
    QString text=view->searchText().trimmed();
    proxy.update(text, genreCombo->currentIndex()<=0 ? QString() : genreCombo->currentText());
    if (proxy.enabled() && !text.isEmpty()) {
        view->expandAll();
    }
}

QStringList StreamsPage::getCategories()
{
    QStringList categories;
    for(int i=0; i<StreamsModel::self()->rowCount(); ++i) {
        QModelIndex idx=StreamsModel::self()->index(i, 0, QModelIndex());
        if (idx.isValid()) {
            categories.append(static_cast<StreamsModel::Item *>(idx.internalPointer())->name);
        }
    }

    if (categories.isEmpty()) {
        categories.append(i18n("General"));
    }

    qSort(categories);
    return categories;
}

QStringList StreamsPage::getGenres()
{
    QStringList g=genreCombo->entries().toList();
    qSort(g);
    return g;
}

void StreamsPage::importWebStreams()
{
    if (!StreamsModel::self()->isWritable()) {
        return;
    }

    QObject *obj=qobject_cast<QObject *>(sender());
    if (!obj) {
        return;
    }

    QUrl url=obj->property(constUrlProperty).toUrl();
    if (!url.isValid()) {
        return;
    }

    if (0!=webStreams[url].job) {
        MessageBox::error(this, i18n("Download from %1 is already in progress!").arg(webStreams[url].name));
        return;
    }

    if (getCategories().contains(webStreams[url].name)) {
        if (MessageBox::No==MessageBox::warningYesNo(this, i18n("Update streams from %1?\n(This will replace any existing streams in this category)").arg(webStreams[url].name),
                                                     i18n("Update %1").arg(webStreams[url].name))) {
            return;
        }
    } else {
        if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Download streams from %1?").arg(webStreams[url].name), i18n("Download %1").arg(webStreams[url].name))) {
            return;
        }
    }

    QNetworkReply *job=NetworkAccessManager::self()->get(QNetworkRequest(url));
    connect(job, SIGNAL(finished()), this, SLOT(downloadFinished()));
    webStreams[url].job=job;
    view->showSpinner();
}

void StreamsPage::initWebStreams()
{
    QFile f(":streams.xml");
    if (f.open(QIODevice::ReadOnly)) {
        QXmlStreamReader doc(&f);
        while (!doc.atEnd()) {
            doc.readNext();

            if (doc.isStartElement() && QLatin1String("stream")==doc.name()) {
                QString name=doc.attributes().value("name").toString();
                unsigned int type=doc.attributes().value("type").toString().toUInt();
                QUrl url=QUrl(doc.attributes().value("url").toString());
                if (type<WebStream::WS_Count) {
                    webStreams[url]=WebStream(name, doc.attributes().value("region").toString(), url, (WebStream::Type)type);
                }
            }
        }
    }
}
