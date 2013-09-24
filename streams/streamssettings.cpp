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

#include "streamssettings.h"
#include "streamsmodel.h"
#include "basicitemdelegate.h"
#include "icon.h"
#include "localize.h"
#include "tar.h"
#include "messagebox.h"
#include "utils.h"
#include <QListWidget>
#include <QFileInfo>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KFileDialog>
#else
#include <QFileDialog>
#endif

enum Roles {
    KeyRole = Qt::UserRole,
    BuiltInRole
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
{
    setupUi(this);
    categories->setItemDelegate(new BasicItemDelegate(categories));
    categories->setSortingEnabled(true);
    int iSize=Icon::stdSize(QApplication::fontMetrics().height()*1.25);
    categories->setIconSize(QSize(iSize, iSize));
    connect(categories, SIGNAL(currentRowChanged(int)), SLOT(currentCategoryChanged(int)));
    connect(installButton, SIGNAL(clicked()), this, SLOT(install()));
    connect(removeButton, SIGNAL(clicked()), this, SLOT(remove()));
    removeButton->setEnabled(false);
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
    bool enable=false;

    if (row>=0) {
        QListWidgetItem *item=categories->item(row);
        enable=!item->data(BuiltInRole).toBool();
    }
    removeButton->setEnabled(enable);
}

void StreamsSettings::install()
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
    name=name.replace("/", "_");

    QListWidgetItem *existing=get(name);

    if (existing && MessageBox::No==MessageBox::warningYesNo(this, i18n("A category named <b>%1</b> already exists!<br/>Overwrite?", name))) {
        return;
    }

    Tar tar(fileName);
    if (!tar.open()) {
        MessageBox::error(this, i18n("Failed top open %1", fileName));
        return;
    }

    QMap<QString, QByteArray> files=tar.extract(QStringList() << StreamsModel::constXmlFile << StreamsModel::constCompressedXmlFile
                                                              << StreamsModel::constPngIcon << StreamsModel::constSvgIcon
                                                              << ".png" << ".svg");
    QString streamsName=files.contains(StreamsModel::constCompressedXmlFile) ? StreamsModel::constCompressedXmlFile : StreamsModel::constXmlFile;
    QString iconName=files.contains(StreamsModel::constSvgIcon) ? StreamsModel::constSvgIcon : StreamsModel::constPngIcon;
    QByteArray xml=files[streamsName];
    QByteArray icon=files[iconName];

    if (xml.isEmpty() || icon.isEmpty()) {
        MessageBox::error(this, i18n("Invalid file format!"));
        return;
    }

    QString streamsDir=Utils::dataDir(StreamsModel::constSubDir, true);
    QString dir=streamsDir+name;
    if (!QDir(dir).exists() && !Utils::createDir(dir, streamsDir)) {
        MessageBox::error(this, i18n("Failed to create stream category folder!"));
        return;
    }

    QFile streamsFile(dir+"/"+streamsName);
    if (!streamsFile.open(QIODevice::WriteOnly)) {
        MessageBox::error(this, i18n("Failed to save stream list!"));
        return;
    }
    streamsFile.write(xml);

    QFile iconFile(dir+"/"+iconName);
    if (iconFile.open(QIODevice::WriteOnly)) {
        iconFile.write(icon);
    }
    QIcon icn;
    icn.addFile(dir+"/"+iconName);

    // Write all other png and svg files...
    QMap<QString, QByteArray>::ConstIterator it=files.constBegin();
    QMap<QString, QByteArray>::ConstIterator end=files.constEnd();
    for (; it!=end; ++it) {
        if (it.key()!=iconName && (it.key().endsWith(".png") || it.key().endsWith(".svg"))) {
            QFile f(dir+"/"+it.key());
            if (f.open(QIODevice::WriteOnly)) {
                f.write(it.value());
            }
        }
    }

    StreamsModel::CategoryItem *cat=StreamsModel::self()->addXmlCategory(name, icn, dir+"/"+streamsName, true);
    if (existing) {
        delete existing;
    }

    QListWidgetItem *item=new QListWidgetItem(name, categories);
    item->setCheckState(Qt::Checked);
    item->setData(KeyRole, cat->configName);
    item->setData(BuiltInRole, false);
    item->setIcon(icn);
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
    if (!dir.isEmpty() && !removeDir(dir+item->text(), QStringList() << "*.xml" << "*.xml.gz" << "*.png" << "*.svg")) {
        MessageBox::error(this, i18n("Failed to remove streams folder!"));
        return;
    }

    StreamsModel::self()->removeXmlCategory(item->data(KeyRole).toString());
    delete item;
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
