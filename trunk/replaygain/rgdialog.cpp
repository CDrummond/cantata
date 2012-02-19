/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "rgdialog.h"
#include "device.h"
#include "devicesmodel.h"
#include "settings.h"
#include "tags.h"
#include <QtGui/QTreeWidget>
#include <QtGui/QProgressBar>
#include <QtGui/QBoxLayout>
#include <QtGui/QHeaderView>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <KDE/ThreadWeaver/Weaver>

enum Columns
{
    COL_ARTIST,
    COL_ALBUM,
    COL_TITLE,
    COL_ALBUMGAIN,
    COL_TRACKGAIN,
    COL_ALBUMPEAK,
    COL_TRACKPEAK
};

static int iCount=0;

int RgDialog::instanceCount()
{
    return iCount;
}

RgDialog::RgDialog(QWidget *parent)
    : KDialog(parent)
    , scanning(false)
{
    iCount++;
    setButtons(User1|Ok|Cancel);
    setCaption(i18n("ReplayGain"));
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, mainWidet);
    view = new QTreeWidget(this);
    progress = new QProgressBar(this);
    progress->setVisible(false);
    view->setRootIsDecorated(false);
    view->setAllColumnsShowFocus(true);
    QTreeWidgetItem *hdr = view->headerItem();
    hdr->setText(COL_ARTIST, i18n("Artist"));
    hdr->setText(COL_ALBUM, i18n("Album"));
    hdr->setText(COL_TITLE, i18n("Title"));
    hdr->setText(COL_ALBUMGAIN, i18n("Album Gain"));
    hdr->setText(COL_TRACKGAIN, i18n("Track Gain"));
    hdr->setText(COL_ALBUMPEAK, i18n("Album Peak"));
    hdr->setText(COL_TRACKPEAK, i18n("Track Peak"));

    QHeaderView *hv=view->header();
    hv->setResizeMode(COL_ARTIST, QHeaderView::ResizeToContents);
    hv->setResizeMode(COL_ALBUM, QHeaderView::ResizeToContents);
    hv->setResizeMode(COL_TITLE, QHeaderView::Stretch);
    hv->setResizeMode(COL_ALBUMGAIN, QHeaderView::Fixed);
    hv->setResizeMode(COL_TRACKGAIN, QHeaderView::Fixed);
    hv->setResizeMode(COL_ALBUMPEAK, QHeaderView::Fixed);
    hv->setResizeMode(COL_TRACKPEAK, QHeaderView::Fixed);
    hv->setStretchLastSection(false);

    layout->addWidget(view);
    layout->addWidget(progress);
    setMainWidget(mainWidet);
    setButtonGuiItem(Ok, KStandardGuiItem::save());
    setButtonGuiItem(Cancel, KStandardGuiItem::close());
    setButtonGuiItem(User1, KGuiItem(i18n("Scan"), "edit-find"));
    enableButton(Ok, false);
    enableButton(User1, false);
    resize(800, 400);
}

RgDialog::~RgDialog()
{
    iCount--;
}

void RgDialog::show(const QList<Song> &songs, const QString &udi)
{
    if (songs.count()<1) {
        deleteLater();
        return;
    }

    if (udi.isEmpty()) {
        origSongs=songs;
        base=Settings::self()->mpdDir();
    } else {
        Device *dev=getDevice(udi, parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        base=dev->path();
        int pathLen=base.length();
        foreach (const Song &s, songs) {
            Song m=s;
            m.file=m.file.mid(pathLen);
            origSongs.append(m);
        }
    }
    scanning=false;
    enableButton(User1, songs.count());
    view->clear();
    int i=0;
    foreach (const Song &s, songs) {
        QString a=s.albumArtist()+QLatin1String(" - ")+s.album;
        albums[a].tracks.append(i++);
        new QTreeWidgetItem(view, QStringList() << s.albumArtist() << s.album << s.title);
    }
    KDialog::show();
}

void RgDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        if (KMessageBox::Yes==KMessageBox::questionYesNo(this, i18n("Update ReplayGain tags in files?"))) {
            saveTags();
            stopScanning();
            accept();
        }
        break;
    case User1:
        startScanning();
        break;
    case Cancel:
        if (scanning) {
            if (KMessageBox::Yes==KMessageBox::questionYesNo(this, i18n("Cancel scanning of files?"))) {
                stopScanning();
            }
        } else {
            stopScanning();
            reject();
        }
        break;
    default:
        break;
    }
}

void RgDialog::startScanning()
{
    setButtonGuiItem(Cancel, KStandardGuiItem::cancel());
    scanning=true;
    enableButton(Ok, false);
    enableButton(User1, false);
    progress->setRange(0, 100*origSongs.count());
    progress->setVisible(true);

    jobs.clear();
    scanners.clear();
    for (int i=0; i<origSongs.count(); ++i) {
        Scanner *s=new Scanner(this);
        s->setFile(base+origSongs.at(i).file);
        connect(s, SIGNAL(progress(Scanner *, int)), this, SLOT(scannerProgress(Scanner *, int)));
        connect(s, SIGNAL(done(ThreadWeaver::Job *)), this, SLOT(scannerDone(ThreadWeaver::Job *)));
        jobs.insert(s, JobStatus(i));
        scanners.insert(i, s);
        ThreadWeaver::Weaver::instance()->enqueue(s);
    }
}

void RgDialog::stopScanning()
{
    scanning=false;
    enableButton(User1, true);
    progress->setVisible(false);

    QList<Scanner *> scanners=jobs.keys();
    foreach (Scanner *s, scanners) {
        disconnect(s, SIGNAL(progress(Scanner *, int)), this, SLOT(scannerProgress(Scanner *, int)));
        disconnect(s, SIGNAL(done(ThreadWeaver::Job *)), this, SLOT(scannerDone(ThreadWeaver::Job *)));
        if (!ThreadWeaver::Weaver::instance()->dequeue(s)) {
            s->requestAbort();
        }
    }
    jobs.clear();
    scanners.clear();
    setButtonGuiItem(Cancel, KStandardGuiItem::close());
}

void RgDialog::saveTags()
{
    QStringList failed;

    QMap<QString, Album>::iterator a(albums.begin());
    QMap<QString, Album>::iterator ae(albums.end());

    for (; a!=ae; ++a) {
        foreach (int idx, (*a).tracks) {
            Scanner *s=scanners[idx];
            if (s && s->success() && Tags::Update_Failed==Tags::updateReplaygain(base+origSongs.at(idx).file,
                                                                                Scanner::reference(s->results().loudness), s->results().peakValue(),
                                                                                Scanner::reference((*a).data.loudness), (*a).data.peakValue())) {
                failed.append(origSongs.at(idx).file);
            }
        }
    }

    if (failed.count()) {
        KMessageBox::errorList(this, i18n("Failed to update the tags of the following tracks:"), failed);
    }
}

void RgDialog::updateView()
{
    bool allFinished=true;
    quint64 totalProgress=0;
    QMap<Scanner *, JobStatus>::iterator it=jobs.begin();
    QMap<Scanner *, JobStatus>::iterator end=jobs.end();

    for (; it!=end; ++it) {
        if (!(*it).finished) {
            allFinished=false;
        }
        totalProgress+=(*it).finished ? 100 : (*it).progress;
    }

    if (allFinished) {
        progress->setVisible(false);
        scanning=false;
        enableButton(Ok, true);
        setButtonGuiItem(Cancel, KStandardGuiItem::close());

        QMap<QString, Album>::iterator a(albums.begin());
        QMap<QString, Album>::iterator ae(albums.end());

        for (; a!=ae; ++a) {
            QList<Scanner *> as;
            foreach (int idx, (*a).tracks) {
                Scanner *s=scanners[idx];
                if (s && s->success()) {
                    as.append(s);
                }
            }

            if (as.count()) {
                (*a).data=Scanner::global(as);
                foreach (int idx, (*a).tracks) {
                    QTreeWidgetItem *item=view->topLevelItem(idx);
                    item->setText(COL_ALBUMGAIN, i18n("%1 dB", KGlobal::locale()->formatNumber(Scanner::reference((*a).data.loudness), 2)));
                    item->setText(COL_ALBUMPEAK, KGlobal::locale()->formatNumber((*a).data.peak, 6));
                }
            }
        }

    } else {
        progress->setValue(totalProgress);
    }
}

Device * RgDialog::getDevice(const QString &udi, QWidget *p)
{
    Device *dev=DevicesModel::self()->device(udi);
    if (!dev) {
        KMessageBox::error(p ? p : this, i18n("Device has been removed!"));
        reject();
        return 0;
    }
    if (!dev->isIdle()) {
        KMessageBox::error(p ? p : this, i18n("Device is busy?"));
        reject();
        return 0;
    }
    return dev;
}

void RgDialog::scannerProgress(Scanner *s, int p)
{
    QMap<Scanner *, JobStatus>::iterator it=jobs.find(s);
    if (it!=jobs.end()) {
        if (p!=(*it).progress) {
            (*it).progress=p;
            updateView();
        }
    }
}

void RgDialog::scannerDone(ThreadWeaver::Job *j)
{
    Scanner *s=static_cast<Scanner *>(j);
    QMap<Scanner *, JobStatus>::iterator it=jobs.find(s);
    if (it!=jobs.end()) {
        if (!(*it).finished) {
            (*it).finished=true;
            QTreeWidgetItem *item=view->topLevelItem((*it).index);
            if (s->success()) {
                (*it).progress=100;
                item->setText(COL_TRACKGAIN, i18n("%1 dB", KGlobal::locale()->formatNumber(Scanner::reference(s->results().loudness), 2)));
                item->setText(COL_TRACKPEAK, KGlobal::locale()->formatNumber(s->results().peakValue(), 6));
            } else {
                item->setText(COL_TRACKGAIN, i18n("Failed"));
                item->setText(COL_TRACKPEAK, i18n("Failed"));
            }
            updateView();
        }
    }
}
