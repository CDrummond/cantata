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

#include "streamproviderlistdialog.h"
#include "streamssettings.h"
#include "networkaccessmanager.h"
#include "basicitemdelegate.h"
#include "messagebox.h"
#include "squeezedtextlabel.h"
#include "spinner.h"
#include "flickcharm.h"
#include <QLabel>
#include <QXmlStreamReader>
#include <QTreeWidget>
#include <QHeaderView>
#include <QBoxLayout>
#include <QProgressBar>
#include <QTemporaryFile>
#include <QDir>
#include <QMap>

static const QLatin1String constProverListUrl("http://127.0.0.1/list.xml"); // TODO: Real URL!!!

StreamProviderListDialog::StreamProviderListDialog(StreamsSettings *parent)
    : Dialog(parent, "StreamProviderListDialog")
    , p(parent)
{
    QWidget *wid=new QWidget(this);
    QBoxLayout *l=new QBoxLayout(QBoxLayout::TopToBottom, wid);
    tree=new QTreeWidget(wid);
    tree->setItemDelegate(new BasicItemDelegate(this));
    tree->header()->setVisible(false);
    tree->header()->setStretchLastSection(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    FlickCharm::self()->activateOn(tree);
    statusText=new SqueezedTextLabel(wid);
    progress=new QProgressBar(wid);
    l->addWidget(new QLabel(i18n("Check the providers you wish to install/update."), wid));
    l->addWidget(tree);
    l->addWidget(statusText);
    l->addWidget(progress);
    l->setMargin(0);
    connect(tree, SIGNAL(itemChanged(QTreeWidgetItem*,int)), SLOT(itemChanged(QTreeWidgetItem*,int)));
    setMainWidget(wid);
    setButtons(User1|Close);
    enableButton(User1, false);
    setCaption(i18n("Install/Update Stream Providers"));
    spinner=new Spinner(this);
    spinner->setWidget(tree);
}

StreamProviderListDialog::~StreamProviderListDialog()
{
    if (job) {
        disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        job->cancelAndDelete();
        job=0;
    }
}

void StreamProviderListDialog::show(const QSet<QString> &installed)
{
    installedProviders=installed;
    checkedItems.clear();
    setState(false);
    updateView(true);

    if (!tree->topLevelItemCount()) {
        job=NetworkAccessManager::self()->get(QUrl(constProverListUrl));
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        spinner->start();
    }
    exec();
}

enum Categories {
    Cat_General           = 0,
    Cat_DigitallyImported = 1,
    Cat_ListenLive        = 2,

    Cat_Total
};

static QString catName(int cat) {
    switch (cat) {
    default:
    case Cat_General:           return i18n("General");
    case Cat_DigitallyImported: return i18n("Digitally Imported");
    case Cat_ListenLive:        return i18n("Local and National Radio");
    }
}

void StreamProviderListDialog::jobFinished()
{
    NetworkJob *j=qobject_cast<NetworkJob *>(sender());
    if (!j) {
        return;
    }
    j->deleteLater();
    if (j!=job) {
        return;
    }
    job=0;

    QMap<int, QTreeWidgetItem *> categories;

    if (0==tree->topLevelItemCount()) {
        if (j->ok()) {
            QXmlStreamReader doc(j->readAll());
            int currentCat=-1;
            while (!doc.atEnd()) {
                doc.readNext();

                if (doc.isStartElement()) {
                    if (QLatin1String("category")==doc.name()) {
                        currentCat=doc.attributes().value("type").toString().toInt();
                    } else if (QLatin1String("provider")==doc.name()) {
                        QString name=doc.attributes().value("name").toString();
                        QString url=doc.attributes().value("url").toString();
                        if (!name.isEmpty() && !url.isEmpty() && currentCat>=0 && currentCat<Cat_Total) {
                            QTreeWidgetItem *cat=0;
                            if (!categories.contains(currentCat)) {
                                cat=new QTreeWidgetItem(tree, QStringList() << catName(currentCat));
                                cat->setFlags(Qt::ItemIsEnabled);
                                QFont f=tree->font();
                                f.setBold(true);
                                cat->setFont(0, f);
                                categories.insert(currentCat, cat);
                            } else {
                                cat=categories[currentCat];
                            }
                            QTreeWidgetItem *prov=new QTreeWidgetItem(cat, QStringList() << name);
                            prov->setData(0, Qt::UserRole, url);
                            prov->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);
                            prov->setCheckState(0, Qt::Unchecked);
                            cat->setExpanded(true);
                        }
                    }
                }
            }
            updateView();
        } else {
            MessageBox::error(this, i18n("Failed to download list of stream providers!"));
            slotButtonClicked(Close);
        }
        spinner->stop();
    } else {
        QTreeWidgetItem *item=*(checkedItems.begin());
        if (j->ok()) {
            statusText->setText(i18n("Installing/updating %1", item->text(0)));
            QTemporaryFile temp(QDir::tempPath()+"/cantata_XXXXXX.streams");
            temp.setAutoRemove(true);
            temp.write(j->readAll());
            if (!p->install(temp.fileName(), item->text(0), false)) {
                MessageBox::error(this, i18n("Failed to install <b>%1</b>", item->text(0)));
                setState(false);
            } else {
                item->setCheckState(0, Qt::Unchecked);
                checkedItems.remove(item);
                progress->setValue(progress->value()+1);
                doNext();
            }
        } else {
            MessageBox::error(this, i18n("Failed to download <b>%1</b>", item->text(0)));
            setState(false);
        }
    }
}

void StreamProviderListDialog::itemChanged(QTreeWidgetItem *itm, int col)
{
    Q_UNUSED(col)
    if (Qt::Checked==itm->checkState(0)) {
        checkedItems.insert(itm);
    } else {
        checkedItems.remove(itm);
    }
    enableButton(User1, !checkedItems.isEmpty());
}

void StreamProviderListDialog::slotButtonClicked(int button)
{
    switch (button) {
    case User1: {
        QStringList update;
        QStringList install;
        foreach (const QTreeWidgetItem *i, checkedItems) {
            if (installedProviders.contains(i->text(0))) {
                update.append(i->text(0));
            } else {
                install.append(i->text(0));
            }
        }
        update.sort();
        install.sort();
        QString message="<p>";
        if (!install.isEmpty()) {
            message+=i18n("Install the following providers?")+"</p><ul>";
            foreach (const QString &n, install) {
                message+="<li>"+n+"</li>";
            }
            message+="</ul>";
            if (!update.isEmpty()) {
                message+="<p>";
            }
        }
        if (!update.isEmpty()) {
            message+=i18n("Update the following providers?")+"</p><ul>";
            foreach (const QString &n, update) {
                message+="<li>"+n+"</li>";
            }
            message+="</ul>";
        }

        if (MessageBox::Yes==MessageBox::questionYesNo(this, message, i18n("Install/Update"))) {
            setState(true);
            doNext();
        }
        break;
    }
    case Cancel:
        if (MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Abort installation/update ?"))) {
            if (job) {
                disconnect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
                job->cancelAndDelete();
                job=0;
            }
            reject();
            // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
            Dialog::slotButtonClicked(button);
        }
        break;
    case Close:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}

void StreamProviderListDialog::updateView(bool unCheck)
{
    QFont plain=tree->font();
    QFont italic=plain;
    italic.setItalic(true);

    for (int tl=0; tl<tree->topLevelItemCount(); ++tl) {
        QTreeWidgetItem *tli=tree->topLevelItem(tl);
        for (int c=0; c<tli->childCount(); ++c) {
            QTreeWidgetItem *ci=tli->child(c);
            ci->setFont(0, installedProviders.contains(ci->text(0)) ? italic : plain);
            if (unCheck) {
                ci->setCheckState(0, Qt::Unchecked);
            }
        }
    }
}

void StreamProviderListDialog::doNext()
{
    if (checkedItems.isEmpty()) {
        accept();
    } else {
        QTreeWidgetItem *item=*(checkedItems.begin());
        statusText->setText(i18n("Downloading %1", item->text(0)));
        job=NetworkAccessManager::self()->get(QUrl(item->data(0, Qt::UserRole).toString()));
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    }
}

void StreamProviderListDialog::setState(bool downloading)
{
    tree->setEnabled(!downloading);
    progress->setVisible(downloading);
    statusText->setVisible(downloading);
    if (downloading) {
        setButtons(Cancel);
        progress->setRange(0, checkedItems.count());
        progress->setValue(0);
    } else {
        setButtons(User1|Close);
        setButtonText(User1, i18n("Install/Update"));
        enableButton(User1, !checkedItems.isEmpty());
    }
}
