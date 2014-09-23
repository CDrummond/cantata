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
#include "network/networkaccessmanager.h"
#include "widgets/basicitemdelegate.h"
#include "support/messagebox.h"
#include "support/squeezedtextlabel.h"
#include "support/spinner.h"
#include "support/icon.h"
#include "widgets/actionitemdelegate.h"
#include "widgets/messageoverlay.h"
#include "models/streamsmodel.h"
#include "gui/plurals.h"
#include <QLabel>
#include <QXmlStreamReader>
#include <QTreeWidget>
#include <QHeaderView>
#include <QBoxLayout>
#include <QProgressBar>
#include <QToolButton>
#include <QStylePainter>
#include <QTemporaryFile>
#include <QDir>
#include <QMap>
#include <QTimer>
#include <QCryptographicHash>

//#define TEST_PROVIDERS

static const QLatin1String constProverListUrl("https://googledrive.com/host/0Bzghs6gQWi60dHBPajNjbjExZzQ");

static QString fileMd5(const QString &fileName)
{
    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly)) {
        return QString::fromLatin1(QCryptographicHash::hash(f.readAll(), QCryptographicHash::Md5).toHex());
    }
    return QString();
}

static QString getMd5(const QString &p)
{
    QString dir=Utils::dataDir(StreamsModel::constSubDir, false);
    if (dir.isEmpty()) {
        return QString();
    }
    dir+=Utils::constDirSep+p+Utils::constDirSep;
    if (QFile::exists(dir+StreamsModel::constSettingsFile)) {
        return fileMd5(dir+StreamsModel::constSettingsFile);
    }
    if (QFile::exists(dir+StreamsModel::constCompressedXmlFile)) {
        return fileMd5(dir+StreamsModel::constCompressedXmlFile);
    }
    return QString();
}

class IconLabel : public QToolButton
{
public:
    IconLabel(const Icon &icon, const QString &text, QWidget *p)
        : QToolButton(p)
    {
        setIcon(icon);
        setText(text);
        setAutoRaise(true);
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        QFont f(font());
        f.setItalic(true);
        setFont(f);
        setStyleSheet("QToolButton {border: 0}");
        setFocusPolicy(Qt::NoFocus);
    }

    void paintEvent(QPaintEvent *)
    {
        QStylePainter p(this);
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.state&=~(QStyle::State_MouseOver|QStyle::State_Sunken|QStyle::State_On);
        opt.state|=QStyle::State_Raised;
        p.drawComplexControl(QStyle::CC_ToolButton, opt);
    }
};

StreamProviderListDialog::StreamProviderListDialog(StreamsSettings *parent)
    : Dialog(parent, "StreamProviderListDialog")
    , installed("dialog-ok")
    , updateable("go-down")
    , p(parent)
    , job(0)
    , spinner(0)
    , msgOverlay(0)
{
    QWidget *wid=new QWidget(this);
    QBoxLayout *l=new QBoxLayout(QBoxLayout::TopToBottom, wid);
    QWidget *legends=new QWidget(wid);
    QBoxLayout *legendsLayout=new QBoxLayout(QBoxLayout::LeftToRight, legends);
    legendsLayout->setMargin(0);
    tree=new QTreeWidget(wid);
    tree->setItemDelegate(new BasicItemDelegate(this));
    tree->header()->setVisible(false);
    tree->header()->setStretchLastSection(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    statusText=new SqueezedTextLabel(wid);
    progress=new QProgressBar(wid);
    legendsLayout->addWidget(new IconLabel(installed, i18n("Installed"), legends));
    legendsLayout->addItem(new QSpacerItem(l->spacing(), 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
    legendsLayout->addWidget(new IconLabel(updateable, i18n("Update available"), legends));
    legendsLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
    l->addWidget(new QLabel(i18n("Check the providers you wish to install/update."), wid));
    l->addWidget(tree);
    l->addWidget(legends);
    l->addItem(new QSpacerItem(0, l->spacing(), QSizePolicy::Fixed, QSizePolicy::Fixed));
    l->addWidget(statusText);
    l->addWidget(progress);
    l->setMargin(0);
    connect(tree, SIGNAL(itemChanged(QTreeWidgetItem*,int)), SLOT(itemChanged(QTreeWidgetItem*,int)));
    setMainWidget(wid);
    setButtons(User1|User2|Close);
    enableButton(User1, false);
    enableButton(User2, false);
    setDefaultButton(Close);
    setCaption(i18n("Install/Update Stream Providers"));
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
    installedProviders.clear();
    foreach (const QString &inst, installed) {
        installedProviders.insert(inst, getMd5(inst));
    }

    processItems.clear();
    checkedItems.clear();
    setState(false);
    updateView(true);

    if (!tree->topLevelItemCount()) {
        QTimer::singleShot(0, this, SLOT(getProviderList()));
    }
    exec();
}

void StreamProviderListDialog::getProviderList()
{
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(tree);
    }
    if (!msgOverlay) {
        msgOverlay=new MessageOverlay(this);
        msgOverlay->setWidget(tree);
    }
    #ifdef TEST_PROVIDERS
    QFile f("list.xml");
    if (f.open(QIODevice::ReadOnly)) {
        readProviders(&f);
    }
    #else
    job=NetworkAccessManager::self()->get(QUrl(constProverListUrl));
    connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    spinner->start();
    msgOverlay->setText(i18n("Downloading list..."), -1, false);
    #endif
}

enum Categories {
    Cat_General           = 0,
    Cat_DigitallyImported = 1,
    Cat_ListenLive        = 2,

    Cat_Total
};

enum Roles {
    Role_Url = Qt::UserRole,
    Role_Md5,
    Role_Updateable,
    Role_Category
};

static QString catName(int cat) {
    switch (cat) {
    default:
    case Cat_General:           return i18n("General");
    case Cat_DigitallyImported: return i18n("Digitally Imported");
    case Cat_ListenLive:        return i18n("Local and National Radio (ListenLive)");
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

    if (0==tree->topLevelItemCount()) {
        if (j->ok()) {
            readProviders(j->actualJob());
        } else {
            MessageBox::error(this, i18n("Failed to download list of stream providers!"));
            slotButtonClicked(Close);
        }
        if (spinner) {
            spinner->stop();
        }
        if (msgOverlay) {
            msgOverlay->setText(QString());
        }
    } else {
        QTreeWidgetItem *item=*(processItems.begin());
        if (j->ok()) {
            statusText->setText(i18n("Installing/updating %1", item->text(0)));
            QTemporaryFile temp(QDir::tempPath()+"/cantata_XXXXXX.streams");
            temp.setAutoRemove(true);
            temp.open();
            temp.write(j->readAll());
            temp.close();
            if (!p->install(temp.fileName(), item->text(0), false)) {
                MessageBox::error(this, i18n("Failed to install <b>%1</b>", item->text(0)));
                setState(false);
            } else {
                item->setCheckState(0, Qt::Unchecked);
                processItems.removeAll(item);
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

void StreamProviderListDialog::readProviders(QIODevice *dev)
{
    QMap<int, QTreeWidgetItem *> categories;
    QXmlStreamReader doc(dev);
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
                    prov->setData(0, Role_Url, url);
                    prov->setData(0, Role_Md5, doc.attributes().value("md5").toString());
                    prov->setData(0, Role_Updateable, false);
                    prov->setData(0, Role_Category, currentCat);
                    prov->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);
                    prov->setCheckState(0, Qt::Unchecked);
                    cat->setExpanded(true);
                }
            }
        }
    }
    updateView();
}

static QString nameString(QTreeWidgetItem *i)
{
    if (Cat_ListenLive==i->data(0, Role_Category).toInt()) {
        return i->text(0)+QLatin1String(" <i>(")+i18n("ListenLive")+QLatin1String(")</i>");
    }
    return i->text(0);
}

void StreamProviderListDialog::slotButtonClicked(int button)
{
    switch (button) {
    case User2:
    case User1: {
        QStringList update;
        QStringList install;
        processItems.clear();
        if (User2==button) {
            processItems.clear();
            for (int tl=0; tl<tree->topLevelItemCount(); ++tl) {
                QTreeWidgetItem *tli=tree->topLevelItem(tl);
                for (int c=0; c<tli->childCount(); ++c) {
                    QTreeWidgetItem *ci=tli->child(c);
                    if (ci->data(0, Role_Updateable).toBool()) {
                        update.append(nameString(ci));
                        processItems.append(ci);
                    }
                }
            }
        } else {
            foreach (QTreeWidgetItem *i, checkedItems) {
                if (installedProviders.keys().contains(i->text(0))) {
                    update.append(nameString(i));
                } else {
                    install.append(nameString(i));
                }
                processItems.append(i);
            }
        }
        QString message="<p>";
        if (!install.isEmpty()) {
            message+=i18n("Install the following?")+"</p><ul>";
            foreach (const QString &n, install) {
                message+="<li>"+n+"</li>";
            }
            message+="</ul>";
            if (!update.isEmpty()) {
                message+="<p>";
            }
        }
        if (!update.isEmpty()) {
            message+=i18n("Update the following?")+"</p><ul>";
            foreach (const QString &n, update) {
                message+="<li>"+n+"</li>";
            }
            message+="</ul>";
        }

        if (!message.isEmpty() && MessageBox::Yes==MessageBox::questionYesNo(this, message, i18n("Install/Update"))) {
            setState(true);
            doNext();
        }
        break;
    }
    case Cancel:
        if (MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Abort installation/update?"), i18n("Abort"))) {
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
    QHash<QString, QString>::ConstIterator end=installedProviders.end();
    int numUpdates=0;

    for (int tl=0; tl<tree->topLevelItemCount(); ++tl) {
        QTreeWidgetItem *tli=tree->topLevelItem(tl);
        for (int c=0; c<tli->childCount(); ++c) {
            bool update=false;
            QTreeWidgetItem *ci=tli->child(c);
            QHash<QString, QString>::ConstIterator inst=installedProviders.find(ci->text(0));
            if (inst!=end) {
                update=inst.value()!=ci->data(0, Role_Md5).toString();
                ci->setData(0, Qt::DecorationRole, update ? updateable : installed);
                if (update) {
                    numUpdates++;
                }
            } else {
                ci->setData(0, Qt::DecorationRole, def);
            }
            if (unCheck) {
                ci->setCheckState(0, Qt::Unchecked);
            }
            ci->setData(0, Role_Updateable, update);
        }
    }

    updateText=0==numUpdates ? QString() : Plurals::updates(numUpdates);
    setState(false);
}

void StreamProviderListDialog::doNext()
{
    if (processItems.isEmpty()) {
        accept();
    } else {
        QTreeWidgetItem *item=*(processItems.begin());
        statusText->setText(i18n("Downloading %1", item->text(0)));
        job=NetworkAccessManager::self()->get(QUrl(item->data(0, Role_Url).toString()));
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    }
}

void StreamProviderListDialog::setState(bool downloading)
{
    tree->setEnabled(!downloading);
    progress->setVisible(downloading);
    statusText->setVisible(downloading || !updateText.isEmpty());
    if (downloading) {
        setButtons(Cancel);
        progress->setRange(0, processItems.count());
        progress->setValue(0);
    } else {
        setButtons(User1|User2|Close);
        setButtonText(User1, i18n("Install/Update"));
        enableButton(User1, !checkedItems.isEmpty());
        setButtonText(User2, i18n("Update all updateable providers"));
        enableButton(User2, !updateText.isEmpty());
        setDefaultButton(Close);
        statusText->setText(updateText);
    }
}
