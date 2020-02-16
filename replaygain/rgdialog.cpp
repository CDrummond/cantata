/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#ifdef ENABLE_DEVICES_SUPPORT
#include "devices/device.h"
#include "models/devicesmodel.h"
#endif
#include "gui/settings.h"
#include "tags/tags.h"
#include "tagreader.h"
#include "support/utils.h"
#include "support/messagebox.h"
#include "support/monoicon.h"
#include "jobcontroller.h"
#include "widgets/basicitemdelegate.h"
#include "support/action.h"
#include "mpd-interface/cuefile.h"
#include <QComboBox>
#include <QTreeWidget>
#include <QLabel>
#include <QProgressBar>
#include <QBoxLayout>
#include <QHeaderView>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEventLoop>
#include <algorithm>

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

static inline void setResizeMode(QHeaderView *hdr, int idx, QHeaderView::ResizeMode mode)
{
    hdr->setSectionResizeMode(idx, mode);
}

RgDialog::RgDialog(QWidget *parent)
    : SongDialog(parent, "RgDialog", QSize(800, 400))
    , state(State_Idle)
    , totalToScan(0)
    , tagReader(0)
    , autoScanTags(false)
{
    iCount++;
    setButtons(User1|Ok|Cancel);
    setCaption(tr("ReplayGain"));
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidget = new QWidget(this);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, mainWidget);
    combo = new QComboBox(this);
    view = new QTreeWidget(this);
    statusLabel = new QLabel(this);
    statusLabel->setVisible(false);
    progress = new QProgressBar(this);
    progress->setVisible(false);
    combo->addItem(tr("Show All Tracks"), true);
    combo->addItem(tr("Show Untagged Tracks"), false);
    view->setRootIsDecorated(false);
    view->setAllColumnsShowFocus(true);
    view->setItemDelegate(new BasicItemDelegate(view));
    view->setAlternatingRowColors(false);
    view->setContextMenuPolicy(Qt::ActionsContextMenu);
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    removeAct=new Action(tr("Remove From List"), view);
    removeAct->setEnabled(false);
    view->addAction(removeAct);
    QTreeWidgetItem *hdr = view->headerItem();
    hdr->setText(COL_ARTIST, tr("Artist"));
    hdr->setText(COL_ALBUM, tr("Album"));
    hdr->setText(COL_TITLE, tr("Title"));
    hdr->setText(COL_ALBUMGAIN, tr("Album Gain"));
    hdr->setText(COL_TRACKGAIN, tr("Track Gain"));
    hdr->setText(COL_ALBUMPEAK, tr("Album Peak"));
    hdr->setText(COL_TRACKPEAK, tr("Track Peak"));

    QHeaderView *hv=view->header();
    setResizeMode(hv, COL_ARTIST, QHeaderView::ResizeToContents);
    setResizeMode(hv, COL_ALBUM, QHeaderView::ResizeToContents);
    setResizeMode(hv, COL_TITLE, QHeaderView::Stretch);
    setResizeMode(hv, COL_ALBUMGAIN, QHeaderView::Fixed);
    setResizeMode(hv, COL_TRACKGAIN, QHeaderView::Fixed);
    setResizeMode(hv, COL_ALBUMPEAK, QHeaderView::Fixed);
    setResizeMode(hv, COL_TRACKPEAK, QHeaderView::Fixed);
    hv->setStretchLastSection(false);

    layout->setMargin(0);
    layout->addWidget(combo);
    layout->addWidget(view);
    layout->addWidget(statusLabel);
    layout->addWidget(progress);
    setMainWidget(mainWidget);
    setButtonGuiItem(Ok, StdGuiItem::save());
    setButtonGuiItem(Cancel, StdGuiItem::close());
    setButtonGuiItem(User1, GuiItem(tr("Scan"), FontAwesome::search));
    enableButton(Ok, false);
    enableButton(User1, false);
    qRegisterMetaType<Tags::ReplayGain>("Tags::ReplayGain");
    connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(toggleDisplay()));
    connect(view, SIGNAL(itemSelectionChanged()), SLOT(controlRemoveAct()));
    connect(removeAct, SIGNAL(triggered()), SLOT(removeItems()));

    italic=font();
    italic.setItalic(true);
    JobController::self()->setMaxActive(1);
}

RgDialog::~RgDialog()
{
    clearScanners();
    iCount--;
}

void RgDialog::show(const QList<Song> &songs, const QString &udi, bool autoScan)
{
    if (songs.isEmpty()) {
        deleteLater();
        return;
    }

    for (const Song &s: songs) {
        if (!CueFile::isCue(s.file)) {
           origSongs.append(s);
        }
    }

    if (origSongs.isEmpty()) {
        deleteLater();
        return;
    }

    autoScanTags=autoScan;
    std::sort(origSongs.begin(), origSongs.end());

    #ifdef ENABLE_DEVICES_SUPPORT
    if (udi.isEmpty()) {
        base=MPDConnection::self()->getDetails().dir;
    } else {
        Device *dev=getDevice(udi, parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        base=dev->path();
    }
    #else
    base=MPDConnection::self()->getDetails().dir;
    #endif

    if (!songsOk(origSongs, base, udi.isEmpty())) {
        return;
    }

    state=State_Idle;
    enableButton(User1, origSongs.count());
    view->clear();
    for (const Song &s: origSongs) {
        if (s.isLocalFile()) {
            base = QString();
        }
        QString aa = s.albumArtist();
        const QString &al = s.album;
        const QString &t = s.title;
        if (aa.isEmpty() && al.isEmpty() && t.isEmpty()) {
            new QTreeWidgetItem(view, QStringList() << "-" << "-" << Utils::getFile(s.file));
        } else {
            new QTreeWidgetItem(view, QStringList() << s.albumArtist() << s.album << s.title);
        }
    }
    Dialog::show();
    startReadingTags();
}

void RgDialog::slotButtonClicked(int button)
{
    if (State_Saving==state) {
        return;
    }

    switch (button) {
    case Ok:
        //if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Update ReplayGain tags in tracks?"), tr("Update Tags"),
        //                                               GuiItem(tr("Update Tags")), StdGuiItem::cancel())) {
            if (saveTags()) {
                stopScanning();
                accept();
            }
        //}
        break;
    case User1:
        startScanning();
        break;
    case Cancel:
        switch (state) {
        case State_ScanningFiles:
            if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Abort scanning of tracks?"), tr("Abort"),
                                                           GuiItem(tr("Abort")), StdGuiItem::no())) {
                stopScanning();
                // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
                Dialog::slotButtonClicked(button);
                state=State_Idle;
            }
            break;
        case State_ScanningTags:
            if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Abort reading of existing tags?"), tr("Abort"),
                                                           GuiItem(tr("Abort")), StdGuiItem::no())) {
                stopReadingTags();
                // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
                Dialog::slotButtonClicked(button);
                state=State_Idle;
            }
            break;
        default:
        case State_Idle:
            stopScanning();
            reject();
            // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
            Dialog::slotButtonClicked(button);
            break;
        }
        break;
    default:
        break;
    }
}

void RgDialog::startScanning()
{
    bool all=origTags.isEmpty() ||
             (origTags.count()==origSongs.count()
                ? MessageBox::Yes==MessageBox::questionYesNo(this, tr("Scan <b>all</b> tracks?<br/><br/><i>All tracks have existing ReplayGain tags.</i>"), QString(),
                                                             GuiItem(tr("Scan")), StdGuiItem::cancel())
                : MessageBox::No==MessageBox::questionYesNo(this, tr("Do you wish to scan all tracks, or only tracks without existing tags?"), QString(),
                                                            GuiItem(tr("Untagged Tracks")), GuiItem(tr("All Tracks"))));
    if (!all && origTags.count()==origSongs.count()) {
        return;
    }
    setButtonGuiItem(Cancel, StdGuiItem::cancel());
    state=State_ScanningFiles;
    enableButton(Ok, false);
    enableButton(User1, false);
    progress->setValue(0);
    progress->setVisible(true);
    statusLabel->setText(tr("Scanning tracks..."));
    statusLabel->setVisible(true);
    clearScanners();
    totalToScan=0;
    QMap<QString, QList<int> > groupedTracks;
    for (int i=0; i<origSongs.count(); ++i) {
        if (!removedItems.contains(i) && (all || !origTags.contains(i))) {
            const Song &sng=origSongs.at(i);
            QString aa = sng.albumArtist();
            const QString &al = sng.album;
            groupedTracks[aa.isEmpty() && al.isEmpty() ? (Utils::getFile(sng.file)+" -- "+QString::number(i)) : (sng.albumArtist()+" -- "+sng.album)].append(i);
        }
    }
    QMap<QString, QList<int> >::ConstIterator it(groupedTracks.constBegin());
    QMap<QString, QList<int> >::ConstIterator end(groupedTracks.constEnd());

    for (; it!=end; ++it) {
        createScanner(*it);
        totalToScan++;
    }
    progress->setRange(0, 100*totalToScan);
}

void RgDialog::stopScanning()
{
    state=State_Idle;
    enableButton(User1, true);
    progress->setVisible(false);
    statusLabel->setVisible(false);

    JobController::self()->cancel();
    clearScanners();
    setButtonGuiItem(Cancel, StdGuiItem::close());
}

void RgDialog::createScanner(const QList<int> &indexes)
{
    QMap<int, QString> fileMap;
    for (int i: indexes) {
        fileMap[i]=origSongs.at(i).filePath(base);
    }

    AlbumScanner *s=new AlbumScanner(fileMap);
    connect(s, SIGNAL(progress(int)), this, SLOT(scannerProgress(int)));
    connect(s, SIGNAL(done()), this, SLOT(scannerDone()));
    scanners[s]=0;
    JobController::self()->add(s);
}

void RgDialog::clearScanners()
{
    QList<AlbumScanner *> scannerList=scanners.keys();
    for (AlbumScanner *sc: scannerList) {
        sc->stop();
    }
    scanners.clear();
}

void RgDialog::startReadingTags()
{
    if (tagReader) {
        return;
    }
    setButtonGuiItem(Cancel, StdGuiItem::cancel());
    state=State_ScanningTags;
    enableButton(Ok, false);
    enableButton(User1, false);
    progress->setRange(0, origSongs.count());
    progress->setVisible(true);
    statusLabel->setText(tr("Reading existing tags..."));
    statusLabel->setVisible(true);
    tagReader=new TagReader();
    tagReader->setDetails(origSongs, base);
    connect(tagReader, SIGNAL(progress(int, Tags::ReplayGain)), this, SLOT(songTags(int, Tags::ReplayGain)));
    connect(tagReader, SIGNAL(done()), this, SLOT(tagReaderDone()));
    JobController::self()->add(tagReader);
}

void RgDialog::stopReadingTags()
{
    if (!tagReader) {
        return;
    }

    state=State_Idle;
    enableButton(User1, true);
    progress->setVisible(false);
    statusLabel->setVisible(false);
    disconnect(tagReader, SIGNAL(progress(int, Tags::ReplayGain)), this, SLOT(songTags(int, Tags::ReplayGain)));
    disconnect(tagReader, SIGNAL(done()), this, SLOT(tagReaderDone()));
    tagReader->requestAbort();
    tagReader=0;
    autoScanTags=false;
}

bool RgDialog::saveTags()
{
    state=State_Saving;
    enableButton(Ok, false);
    enableButton(Close, false);
    enableButton(Cancel, false);
    enableButton(User1, false);

    QStringList failed;

    progress->setVisible(true);
    progress->setRange(0, tagsToSave.count());

    int count=0;
    bool someTimedout=false;
    QMap<int, Tags::ReplayGain>::ConstIterator it=tagsToSave.constBegin();
    QMap<int, Tags::ReplayGain>::ConstIterator end=tagsToSave.constEnd();

    for (; it!=end; ++it) {
        QString filePath=origSongs.at(it.key()).filePath(base);
        switch (Tags::updateReplaygain(filePath, it.value())) {
        case Tags::Update_Failed:
            failed.append(filePath);
            break;
        case Tags::Update_BadFile:
            failed.append(tr("%1 (Corrupt tags?)", "filename (Corrupt tags?)").arg(filePath));
            break;
        default:
            break;
        }

        progress->setValue(progress->value()+1);
        if (0==count++%10) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    if (failed.count()) {
        MessageBox::errorListEx(this, tr("Failed to update the tags of the following tracks:"), failed);
    }

    return !someTimedout;
}

void RgDialog::updateView()
{
    int finished=0;
    quint64 totalProgress=0;
    QMap<AlbumScanner *, int>::ConstIterator it=scanners.constBegin();
    QMap<AlbumScanner *, int>::ConstIterator end=scanners.constEnd();

    for (; it!=end; ++it) {
        if (100==it.value()) {
            finished++;
        }
        totalProgress+=(*it);
    }

    if (finished==totalToScan) {
        progress->setVisible(false);
        statusLabel->setVisible(false);
        state=State_Idle;
        setButtonGuiItem(Cancel, StdGuiItem::close());
        enableButton(Ok, !tagsToSave.isEmpty());
    } else {
        progress->setValue(totalProgress);
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
Device * RgDialog::getDevice(const QString &udi, QWidget *p)
{
    Device *dev=DevicesModel::self()->device(udi);
    if (!dev) {
        MessageBox::error(p ? p : this, tr("Device has been removed!"));
        reject();
        return 0;
    }
    if (!dev->isConnected()) {
        MessageBox::error(p ? p : this, tr("Device is not connected."));
        reject();
        return 0;
    }
    if (!dev->isIdle()) {
        MessageBox::error(p ? p : this, tr("Device is busy?"));
        reject();
        return 0;
    }
    return dev;
}
#endif

void RgDialog::scannerProgress(int p)
{
    AlbumScanner *s=qobject_cast<AlbumScanner *>(sender());
    if (!s) {
        return;
    }

    scanners[s]=p;
    updateView();
}

void RgDialog::scannerDone()
{
    AlbumScanner *s=qobject_cast<AlbumScanner *>(sender());
    if (!s) {
        return;
    }

    const QMap<int, AlbumScanner::Values> &trackValues=s->trackValues();
    QMap<int, AlbumScanner::Values>::ConstIterator it(trackValues.constBegin());
    QMap<int, AlbumScanner::Values>::ConstIterator end(trackValues.constEnd());

    if (s->success()) {
        for(; it!=end; ++it) {
            Tags::ReplayGain updatedTags(it.value().gain, s->albumValues().gain, it.value().peak, s->albumValues().peak);
            QTreeWidgetItem *item=view->topLevelItem(it.key());
            if (it.value().ok) {
                item->setText(COL_TRACKGAIN, tr("%1 dB").arg(Utils::formatNumber(updatedTags.trackGain, 2)));
                if (!Utils::equal(updatedTags.trackPeak, 0.0)) {
                    item->setText(COL_TRACKPEAK, Utils::formatNumber(updatedTags.trackPeak, 6));
                }
            } else {
                item->setText(COL_TRACKGAIN, tr("Failed"));
                item->setText(COL_TRACKPEAK, tr("Failed"));
            }
            if (s->albumValues().ok) {
                item->setText(COL_ALBUMGAIN, tr("%1 dB").arg(Utils::formatNumber(updatedTags.albumGain, 2)));
                if (!Utils::equal(updatedTags.albumPeak, 0.0)) {
                    item->setText(COL_ALBUMPEAK, Utils::formatNumber(updatedTags.albumPeak, 6));
                }
            } else {
                item->setText(COL_ALBUMGAIN, tr("Failed"));
                item->setText(COL_ALBUMPEAK, tr("Failed"));
            }

            if (it.value().ok && origTags.contains(it.key())) {
                Tags::ReplayGain t=origTags[it.key()];
                bool diff=false;
                bool diffAlbum=false;
                if (!Utils::equal(t.trackGain, updatedTags.trackGain, 0.01)) {
                    item->setFont(COL_TRACKGAIN, italic);
                    item->setToolTip(COL_TRACKGAIN, tr("Original: %1 dB").arg(Utils::formatNumber(t.trackGain, 2)));
                    diff=true;
                }
                if (!Utils::equal(updatedTags.trackPeak, 0.0) && !Utils::equal(t.trackPeak, updatedTags.trackPeak, 0.000001)) {
                    item->setFont(COL_TRACKPEAK, italic);
                    item->setToolTip(COL_TRACKPEAK, tr("Original: %1").arg(Utils::formatNumber(t.trackPeak, 6)));
                    diff=true;
                }
                if (!Utils::equal(t.albumGain, updatedTags.albumGain, 0.01)) {
                    item->setFont(COL_ALBUMGAIN, italic);
                    item->setToolTip(COL_ALBUMGAIN, tr("Original: %1 dB").arg(Utils::formatNumber(t.albumGain, 2)));
                    diffAlbum=true;
                }
                if (!Utils::equal(updatedTags.albumPeak, 0.0) && !Utils::equal(t.albumPeak, updatedTags.albumPeak, 0.000001)) {
                    item->setFont(COL_ALBUMPEAK, italic);
                    item->setToolTip(COL_ALBUMPEAK, tr("Original: %1").arg(Utils::formatNumber(t.albumPeak, 6)));
                    diffAlbum=true;
                }
                if (diff || diffAlbum) {
                    if (diff) {
                        item->setFont(COL_ARTIST, italic);
                        item->setFont(COL_TITLE, italic);
                    }
                    if (diffAlbum) {
                        item->setFont(COL_ALBUM, italic);
                    }
                    tagsToSave.insert(it.key(), updatedTags);
                } else {
                    tagsToSave.remove(it.key());
                }
            } else if (it.value().ok) {
                item->setFont(COL_ARTIST, italic);
                item->setFont(COL_ALBUM, italic);
                item->setFont(COL_TITLE, italic);
                item->setFont(COL_TRACKGAIN, italic);
                item->setFont(COL_TRACKPEAK, italic);
                item->setFont(COL_ALBUMGAIN, italic);
                item->setFont(COL_ALBUMPEAK, italic);
                tagsToSave.insert(it.key(), updatedTags);
            } else {
                tagsToSave.remove(it.key());
            }
        }
    } else {
        for(; it!=end; ++it) {
            QTreeWidgetItem *item=view->topLevelItem(it.key());
            item->setText(COL_TRACKGAIN, tr("Failed"));
            item->setText(COL_TRACKPEAK, tr("Failed"));
            item->setText(COL_ALBUMGAIN, tr("Failed"));
            item->setText(COL_ALBUMPEAK, tr("Failed"));
            tagsToSave.remove(it.key());
        }
    }

    scanners[s]=100;
    updateView();
    JobController::self()->finishedWith(s);
}

void RgDialog::songTags(int index, Tags::ReplayGain tags)
{
    if (index>=0 && index<origSongs.count()) {
        progress->setValue(progress->value()+1);
        if (!tags.isEmpty()) {
            origTags[index]=tags;

            QTreeWidgetItem *item=view->topLevelItem(index);
            if (!item) {
                return;
            }
            item->setText(COL_TRACKGAIN, tr("%1 dB").arg(Utils::formatNumber(tags.trackGain, 2)));
            if (!Utils::equal(tags.trackPeak, 0.0)) {
                item->setText(COL_TRACKPEAK, Utils::formatNumber(tags.trackPeak, 6));
            }
            item->setText(COL_ALBUMGAIN, tr("%1 dB").arg(Utils::formatNumber(tags.albumGain, 2)));
            if (!Utils::equal(tags.albumPeak, 0.0)) {
                item->setText(COL_ALBUMPEAK, Utils::formatNumber(tags.albumPeak, 6));
            }
        }
    }
}

void RgDialog::tagReaderDone()
{
    TagReader *t=qobject_cast<TagReader *>(sender());
    if (!t) {
        return;
    }

    JobController::self()->finishedWith(t);
    tagReader=0;

    state=State_Idle;
    enableButton(User1, true);
    progress->setVisible(false);
    statusLabel->setVisible(false);

    if (autoScanTags) {
        autoScanTags=false;
        startScanning();
    }
}

void RgDialog::toggleDisplay()
{
    bool showAll=combo->itemData(combo->currentIndex()).toBool();

    for (int i=0; i<view->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item=view->topLevelItem(i);
        if (!removedItems.contains(view->indexOfTopLevelItem(item))) {
            item->setHidden(showAll ? false : origTags.contains(i));
        }
    }
}

void RgDialog::controlRemoveAct()
{
    removeAct->setEnabled(view->topLevelItemCount()>1 && !view->selectedItems().isEmpty());
}

void RgDialog::removeItems()
{
    if (view->topLevelItemCount()<1) {
        return;
    }

    if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Remove the selected tracks from the list?"),
                                                   tr("Remove Tracks"), StdGuiItem::remove(), StdGuiItem::cancel())) {
        QList<QTreeWidgetItem *> selection=view->selectedItems();
        for (QTreeWidgetItem *item: selection) {
            int index=view->indexOfTopLevelItem(item);
            item->setHidden(true);
            removedItems.insert(index);
            if (tagsToSave.contains(index)) {
                tagsToSave.remove(index);
            }
        }
    }
}

void RgDialog::closeEvent(QCloseEvent *event)
{
    if (State_Idle!=state) {
        slotButtonClicked(Cancel);
        if (State_Idle!=state) {
            event->ignore();
        }
    } else {
        Dialog::closeEvent(event);
    }
}

#include "moc_rgdialog.cpp"
