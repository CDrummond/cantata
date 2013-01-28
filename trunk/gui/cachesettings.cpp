/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cachesettings.h"
#include "localize.h"
#include "lyricspage.h"
#include "covers.h"
#include "musiclibrarymodel.h"
#include "utils.h"
#include "messagebox.h"
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QGridLayout>
#include <QThread>
#include <QFileInfo>
#include <QDir>

static const int constMaxRecurseLevel=4;

static void calculate(const QString &d, const QStringList &types, int &items, int &space, int level=0)
{
    if (level<constMaxRecurseLevel) {
        QDir dir(d);
        if (dir.exists()) {
            QFileInfoList files=dir.entryInfoList(types, QDir::Files|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &file, files) {
                space+=file.size();
            }
            items+=files.count();

            QFileInfoList dirs=dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &subDir, dirs) {
                calculate(subDir.absoluteFilePath(), types, items, space, level+1);
            }
        }
    }
}

static void deleteAll(const QString &d, const QStringList &types, int level=0)
{
    if (level<constMaxRecurseLevel) {
        QDir dir(d);
        if (dir.exists()) {
            QFileInfoList dirs=dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &subDir, dirs) {
                deleteAll(subDir.absoluteFilePath(), types, level+1);
            }

            QFileInfoList files=dir.entryInfoList(types, QDir::Files|QDir::NoDotAndDotDot);
            foreach (const QFileInfo &file, files) {
                QFile::remove(file.absoluteFilePath());
            }
            if (0!=level) {
                QString dirName=dir.dirName();
                if (!dirName.isEmpty()) {
                    dir.cdUp();
                    dir.rmdir(dirName);
                }
            }
        }
    }
}

CacheItemCounter::CacheItemCounter(const QString &d, const QStringList &t)
    : dir(d)
    , types(t)
{
    thread=new QThread();
    moveToThread(thread);
    thread->start();
}

CacheItemCounter::~CacheItemCounter()
{
    if (thread) {
        Utils::stopThread(thread);
    }
}

void CacheItemCounter::getCount()
{
    int items=0;
    int space=0;
    calculate(dir, types, items, space);
    emit count(items, space);
}

void CacheItemCounter::deleteAll()
{
    ::deleteAll(dir, types);
    getCount();
}

CacheItem::CacheItem(const QString &title, const QString &d, const QStringList &t, QWidget *p)
    : QGroupBox(p)
    , calculated(false)
    , counter(new CacheItemCounter(d, t))
{
    setTitle(title);
    QFormLayout *layout=new QFormLayout(this);
    button=new QPushButton(i18n("Delete All"), this);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    button->setEnabled(false);
    connect(this, SIGNAL(getCount()), counter, SLOT(getCount()), Qt::QueuedConnection);
    connect(this, SIGNAL(deleteAll()), counter, SLOT(deleteAll()), Qt::QueuedConnection);
    connect(counter, SIGNAL(count(int, int)), this, SLOT(update(int, int)), Qt::QueuedConnection);
    connect(button, SIGNAL(clicked(bool)), this, SLOT(deleteAllItems()));

    numItems=new QLabel(this);
    spaceUsed=new QLabel(this);
    layout->addRow(i18n("Number of items:"), numItems);
    layout->addRow(i18n("Space used:"), spaceUsed);
    layout->setWidget(2, QFormLayout::FieldRole, button);
    numItems->setText(i18n("Calculating..."));
    spaceUsed->setText(i18n("Calculating..."));
}

CacheItem::~CacheItem()
{
}

void CacheItem::showEvent(QShowEvent *e)
{
    if (!calculated) {
        emit getCount();
        calculated=true;
    }
    QGroupBox::showEvent(e);
}

void CacheItem::deleteAllItems()
{
    if (MessageBox::Yes==MessageBox::warningYesNo(this, i18n("Delete all cached items?"), title())) {
        emit deleteAll();
    }
}

void CacheItem::update(int itemCount, int space)
{
    numItems->setText(QString::number(itemCount));
    spaceUsed->setText(Utils::formatByteSize(space));
    button->setEnabled(itemCount>0);
}

CacheSettings::CacheSettings(QWidget *parent)
    : QWidget(parent)
{
    int spacing=style()->layoutSpacing(QSizePolicy::DefaultType, QSizePolicy::DefaultType, Qt::Vertical);
    if (spacing<0) {
        spacing=4;
    }

    QGridLayout *layout=new QGridLayout(this);
    layout->setMargin(0);
    int row=0;
    int col=0;
    QLabel *label=new QLabel(i18n("To speed up loading of the music library, Cantata caches a local copy of the MPD listing. Cantata might also have cached "
                                  "covers, or lyrics, if these have been downloaded and could not be saved into the MPD folder (because Cantata cannot access it, "
                                  "or you have configured Cantata to not save these items there). Below is a summary of Cantata's cache usage."), this);
    label->setWordWrap(true);
    layout->addWidget(label, row++, col, 1, 2);
    layout->addItem(new QSpacerItem(spacing, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed), row++, 0);
    layout->addWidget(new CacheItem(i18n("Covers"), Utils::cacheDir(Covers::constCoverDir, false), QStringList() << "*.jpg" << "*.png", this), row++, col);
    layout->addWidget(new CacheItem(i18n("Lyrics"), Utils::cacheDir(LyricsPage::constLyricsDir, false), QStringList() << "*"+LyricsPage::constExtension, this), row++, col);
    layout->addWidget(new CacheItem(i18n("Music Library List"), Utils::cacheDir(MusicLibraryModel::constLibraryCache, false),
                                    QStringList() << "*"+MusicLibraryModel::constLibraryExt << "*"+MusicLibraryModel::constLibraryCompressedExt, this), row++, col);
    layout->addItem(new QSpacerItem(0, 8, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding), row++, 0);
    row=2;
    col=1;
    layout->addWidget(new CacheItem(i18n("Jamendo"), Utils::cacheDir("jamendo", false), QStringList() << "*"+MusicLibraryModel::constLibraryCompressedExt << "*.jpg" << "*.png", this), row++, 1);
    layout->addWidget(new CacheItem(i18n("Magnatune"), Utils::cacheDir("magnatune", false), QStringList() << "*"+MusicLibraryModel::constLibraryCompressedExt<< "*.jpg" << "*.png", this), row++, 1);
}

CacheSettings::~CacheSettings() {
}
