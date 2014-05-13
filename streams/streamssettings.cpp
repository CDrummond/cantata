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

#include "streamssettings.h"
#include "models/streamsmodel.h"
#include "streamproviderlistdialog.h"
#include "widgets/basicitemdelegate.h"
#include "support/icon.h"
#include "support/localize.h"
#include "tar.h"
#include "support/messagebox.h"
#include "support/utils.h"
#include "digitallyimportedsettings.h"
#include "support/flickcharm.h"
#include <QListWidget>
#include <QMenu>
#include <QFileInfo>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QFileDialog>
#endif

enum Roles {
    KeyRole = Qt::UserRole,
    BuiltInRole,
    ConfigurableRole
};

static bool removeDir(const QString &d, const QStringList &types)
{
    QDir dir(d);
    if (dir.exists()) {
        QFileInfoList files=dir.entryInfoList(types, QDir::Files|QDir::NoDotAndDotDot);
        foreach (const QFileInfo &file, files) {
            if (!QFile::remove(file.absoluteFilePath())) {
                return false;
            }
        }
        return dir.rmdir(d);
    }
    return true; // Does not exist...
}

StreamsSettings::StreamsSettings(QWidget *p)
    : QWidget(p)
    , providerDialog(0)
{
    setupUi(this);
    categories->setItemDelegate(new BasicItemDelegate(categories));
    categories->setSortingEnabled(true);
    FlickCharm::self()->activateOn(categories);
    int iSize=Icon::stdSize(QApplication::fontMetrics().height()*1.25);
    QMenu *installMenu=new QMenu(this);
    QAction *installFromFileAct=installMenu->addAction(i18n("From File..."));
    QAction *installFromWebAct=installMenu->addAction(i18n("Download..."));
    installButton->setMenu(installMenu);
    categories->setIconSize(QSize(iSize, iSize));
    connect(categories, SIGNAL(currentRowChanged(int)), SLOT(currentCategoryChanged(int)));
    connect(installFromFileAct, SIGNAL(triggered()), this, SLOT(installFromFile()));
    connect(installFromWebAct, SIGNAL(triggered()), this, SLOT(installFromWeb()));
    connect(removeButton, SIGNAL(clicked()), this, SLOT(remove()));
    connect(configureButton, SIGNAL(clicked()), this, SLOT(configure()));
    removeButton->setEnabled(false);
    configureButton->setEnabled(false);
}

void StreamsSettings::load()
{
    QList<StreamsModel::Category> cats=StreamsModel::self()->getCategories();
    QFont f(font());
    f.setItalic(true);
    foreach (const StreamsModel::Category &cat, cats) {
        QListWidgetItem *item=new QListWidgetItem(cat.name, categories);
        item->setCheckState(cat.hidden ? Qt::Unchecked : Qt::Checked);
        item->setData(KeyRole, cat.key);
        item->setData(BuiltInRole, cat.builtin);
        item->setData(ConfigurableRole, cat.configurable);
        item->setIcon(cat.icon);
        if (cat.builtin) {
            item->setFont(f);
        }
    }
}

void StreamsSettings::save()
{
    QSet<QString> disabled;
    for (int i=0; i<categories->count(); ++i) {
        QListWidgetItem *item=categories->item(i);
        if (Qt::Unchecked==item->checkState()) {
            disabled.insert(item->data(Qt::UserRole).toString());
        }
    }
    StreamsModel::self()->setHiddenCategories(disabled);
}

void StreamsSettings::currentCategoryChanged(int row)
{
    bool enableRemove=false;
    bool enableConfigure=false;

    if (row>=0) {
        QListWidgetItem *item=categories->item(row);
        enableRemove=!item->data(BuiltInRole).toBool();
        enableConfigure=item->data(ConfigurableRole).toBool();
    }
    removeButton->setEnabled(enableRemove);
    configureButton->setEnabled(enableConfigure);
}

void StreamsSettings::installFromFile()
{
    #ifdef ENABLE_KDE_SUPPORT
    QString fileName=KFileDialog::getOpenFileName(KUrl(), i18n("*.streams|Cantata Streams"), this, i18n("Install Streams"));
    #else
    QString fileName=QFileDialog::getOpenFileName(this, i18n("Install Streams"), QDir::homePath(), i18n("Cantata Streams (*.streams)"));
    #endif
    if (fileName.isEmpty()) {
        return;
    }

    QString name=QFileInfo(fileName).baseName();
    if (name.isEmpty()) {
        return;
    }
    name=name.replace(Utils::constDirSep, "_");
    #ifdef Q_OS_WIN
    name=name.replace("\\", "_");
    #endif

    if (get(name) && MessageBox::No==MessageBox::warningYesNo(this, i18n("A category named <b>%1</b> already exists!<br/>Overwrite?", name))) {
        return;
    }
    install(fileName, name);
}

void StreamsSettings::installFromWeb()
{
    if (!providerDialog) {
        providerDialog=new StreamProviderListDialog(this);
    }

    QSet<QString> installed;
    for (int i=0; i<categories->count(); ++i) {
        QListWidgetItem *item=categories->item(i);
        installed.insert(item->text());
    }

    providerDialog->show(installed);
}

bool StreamsSettings::install(const QString &fileName, const QString &name, bool showErrors)
{
    Tar tar(fileName);
    if (!tar.open()) {
        if (showErrors) {
            MessageBox::error(this, i18n("Failed top open package file"));
        }
        return false;
    }

    QMap<QString, QByteArray> files=tar.extract(QStringList() << StreamsModel::constXmlFile << StreamsModel::constCompressedXmlFile
                                                              << StreamsModel::constSettingsFile
                                                              << StreamsModel::constPngIcon << StreamsModel::constSvgIcon
                                                              << ".png" << ".svg");
    QString streamsName=files.contains(StreamsModel::constCompressedXmlFile)
                            ? StreamsModel::constCompressedXmlFile
                            : files.contains(StreamsModel::constXmlFile)
                                ? StreamsModel::constXmlFile
                                : StreamsModel::constSettingsFile;
    QString iconName=files.contains(StreamsModel::constSvgIcon) ? StreamsModel::constSvgIcon : StreamsModel::constPngIcon;
    QByteArray streamFile=files[streamsName];
    QByteArray icon=files[iconName];

    if (streamFile.isEmpty()) {
        if (showErrors) {
            MessageBox::error(this, i18n("Invalid file format!"));
        }
        return false;
    }

    QString streamsDir=Utils::dataDir(StreamsModel::constSubDir, true);
    QString dir=streamsDir+name;
    if (!QDir(dir).exists() && !QDir(dir).mkpath(dir)) {
        if (showErrors) {
            MessageBox::error(this, i18n("Failed to create stream category folder!"));
        }
        return false;
    }

    QFile streamsFile(dir+Utils::constDirSep+streamsName);
    if (!streamsFile.open(QIODevice::WriteOnly)) {
        if (showErrors) {
            MessageBox::error(this, i18n("Failed to save stream list!"));
        }
        return false;
    }
    streamsFile.write(streamFile);
    streamsFile.close();

    QIcon icn;
    if (!icon.isEmpty()) {
        QFile iconFile(dir+Utils::constDirSep+iconName);
        if (iconFile.open(QIODevice::WriteOnly)) {
            iconFile.write(icon);
        }
        icn.addFile(dir+Utils::constDirSep+iconName);
    }

    // Write all other png and svg files...
    QMap<QString, QByteArray>::ConstIterator it=files.constBegin();
    QMap<QString, QByteArray>::ConstIterator end=files.constEnd();
    for (; it!=end; ++it) {
        if (it.key()!=iconName && (it.key().endsWith(".png") || it.key().endsWith(".svg"))) {
            QFile f(dir+Utils::constDirSep+it.key());
            if (f.open(QIODevice::WriteOnly)) {
                f.write(it.value());
            }
        }
    }

    StreamsModel::CategoryItem *cat=StreamsModel::self()->addInstalledProvider(name, icn, dir+Utils::constDirSep+streamsName, true);
    if (!cat) {
        if (showErrors) {
            MessageBox::error(this, i18n("Invalid file format!"));
        }
        return false;
    }
    QListWidgetItem *existing=get(name);
    if (existing) {
        delete existing;
    }

    QListWidgetItem *item=new QListWidgetItem(name, categories);
    item->setCheckState(Qt::Checked);
    item->setData(KeyRole, cat->configName);
    item->setData(BuiltInRole, false);
    item->setData(ConfigurableRole, cat->canConfigure());
    item->setIcon(icn);
    return true;
}

void StreamsSettings::remove()
{
    int row=categories->currentRow();
    if (row<0) {
        return;
    }

    QListWidgetItem *item=categories->item(row);
    if (!item->data(BuiltInRole).toBool() && MessageBox::No==MessageBox::warningYesNo(this, i18n("Are you sure you wish to remove <b>%1</b>?", item->text()))) {
        return;
    }

    QString dir=Utils::dataDir(StreamsModel::constSubDir);
    if (!dir.isEmpty() && !removeDir(dir+item->text(), QStringList() << StreamsModel::constXmlFile << StreamsModel::constCompressedXmlFile
                                                                     << StreamsModel::constSettingsFile << "*.png" << "*.svg")) {
        MessageBox::error(this, i18n("Failed to remove streams folder!"));
        return;
    }

    StreamsModel::self()->removeInstalledProvider(item->data(KeyRole).toString());
    delete item;
}

void StreamsSettings::configure()
{
    int row=categories->currentRow();
    if (row<0) {
        return;
    }

    QListWidgetItem *item=categories->item(row);
    if (!item->data(ConfigurableRole).toBool()) {
        return;
    }

    // TODO: Currently only digitally imported can be configured...
    DigitallyImportedSettings(this).show();
}

QListWidgetItem *  StreamsSettings::get(const QString &name)
{
    for (int i=0; i<categories->count(); ++i) {
        QListWidgetItem *item=categories->item(i);
        if (!item->data(BuiltInRole).toBool() && item->text()==name) {
            return item;
        }
    }
    return 0;
}
