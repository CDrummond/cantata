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
#include "tagreader.h"
#include "utils.h"
#include "localize.h"
#include "messagebox.h"
#include "jobcontroller.h"
#include <QtGui/QTreeWidget>
#include <QtGui/QLabel>
#include <QtGui/QProgressBar>
#include <QtGui/QBoxLayout>
#include <QtGui/QHeaderView>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

#ifdef ENABLE_DEVICES_SUPPORT
static QString formatNumber(double number, int precision)
{
    return KGlobal::locale()->formatNumber(number, precision);
}
#else
static QString formatNumber(double number, int precision)
{
    return QString::number(number, 'g', precision);
}
#endif

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

static inline QString createAlbumName(const Song &s)
{
    return s.albumArtist()+QLatin1String(" - ")+s.album;
}

int RgDialog::instanceCount()
{
    return iCount;
}

RgDialog::RgDialog(QWidget *parent)
    : Dialog(parent)
    , state(State_Idle)
    , tagReader(0)
{
    iCount++;
    setButtons(User1|Ok|Cancel);
    setCaption(i18n("ReplayGain"));
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, mainWidet);
    view = new QTreeWidget(this);
    statusLabel = new QLabel(this);
    statusLabel->setVisible(false);
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
    layout->addWidget(statusLabel);
    layout->addWidget(progress);
    setMainWidget(mainWidet);
    #ifdef ENABLE_DEVICES_SUPPORT
    setButtonGuiItem(Ok, KStandardGuiItem::save());
    setButtonGuiItem(Cancel, KStandardGuiItem::close());
    #else
    setButtonGuiItem(Ok, KGuiItem(i18n("Save"), "document-save"));
    setButtonGuiItem(Cancel, KGuiItem(i18n("Close"), "dialog-close"));
    #endif
    setButtonGuiItem(User1, KGuiItem(i18n("Scan"), "edit-find"));
    enableButton(Ok, false);
    enableButton(User1, false);
    resize(800, 400);
    qRegisterMetaType<Tags::ReplayGain>("Tags::ReplayGain");
}

RgDialog::~RgDialog()
{
    clearScanners();
    iCount--;
}

void RgDialog::show(const QList<Song> &songs, const QString &udi)
{
    if (songs.count()<1) {
        deleteLater();
        return;
    }

    origSongs=songs;
    qSort(origSongs);
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
    Q_UNUSED(udi)
    base=MPDConnection::self()->getDetails().dir;
    #endif
    state=State_Idle;
    enableButton(User1, origSongs.count());
    view->clear();
    int i=0;
    foreach (const Song &s, origSongs) {
        albums[createAlbumName(s)].tracks.append(i++);
        new QTreeWidgetItem(view, QStringList() << s.albumArtist() << s.album << s.title);
    }
    Dialog::show();
    startReadingTags();
}

void RgDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        if (MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Update ReplayGain tags in files?"))) {
            saveTags();
            stopScanning();
            accept();
        }
        break;
    case User1:
        startScanning();
        break;
    case Cancel:
        switch (state) {
        case State_ScanningFiles:
            if (MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Cancel scanning of files?"))) {
                stopScanning();
                // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
                Dialog::slotButtonClicked(button);
            }
            break;
        case State_ScanningTags:
            if (MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Cancel reading of existing tags?"))) {
                stopReadingTags();
                // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
                Dialog::slotButtonClicked(button);
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
             MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Do you wish to scan all files, or only files without existing tags?"), QString(),
                                                        KGuiItem(i18n("All Tracks")), KGuiItem(i18n("Untagged Tracks")));
    if (!all && origTags.count()==origSongs.count()) {
        return;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    setButtonGuiItem(Cancel, KStandardGuiItem::cancel());
    #else
    setButtonGuiItem(Cancel, KGuiItem(i18n("Cancel"), "dialog-cancel"));
    #endif
    state=State_ScanningFiles;
    enableButton(Ok, false);
    enableButton(User1, false);
    progress->setVisible(true);
    statusLabel->setText(i18n("Scanning files..."));
    statusLabel->setVisible(true);
    clearScanners();
    int count=0;
    for (int i=0; i<origSongs.count(); ++i) {
        if (all || !origTags.contains(i)) {
            if (scanners.count()<100) {
                createScanner(i);
            } else {
                toScan.append(i);
            }
            count++;
        }
    }
    progress->setRange(0, 100*count);
}

void RgDialog::stopScanning()
{
    state=State_Idle;
    enableButton(User1, true);
    progress->setVisible(false);
    statusLabel->setVisible(false);

    JobController::self()->cancel();
    clearScanners();
    #ifdef ENABLE_DEVICES_SUPPORT
    setButtonGuiItem(Cancel, KStandardGuiItem::cancel());
    #else
    setButtonGuiItem(Cancel, KGuiItem(i18n("Close"), "dialog-close"));
    #endif
}

void RgDialog::createScanner(int index)
{
    Scanner *s=new Scanner(index);
    s->setFile(base+origSongs.at(index).file);
    connect(s, SIGNAL(progress(int)), this, SLOT(scannerProgress(int)));
    connect(s, SIGNAL(done()), this, SLOT(scannerDone()));
    scanners.insert(index, s);
    JobController::self()->add(s);
}

void RgDialog::clearScanners()
{
    QMap<int, Scanner *>::ConstIterator it(scanners.constBegin());
    QMap<int, Scanner *>::ConstIterator end(scanners.constEnd());

    for (; it!=end; ++it) {
        it.value()->stop();
    }
    scanners.clear();
    toScan.clear();
    tracks.clear();
    QMap<QString, Album>::iterator a(albums.begin());
    QMap<QString, Album>::iterator ae(albums.end());

    for (; a!=ae; ++a) {
        (*a).scannedTracks.clear();
    }
}

void RgDialog::startReadingTags()
{
    if (tagReader) {
        return;
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    setButtonGuiItem(Cancel, KStandardGuiItem::cancel());
    #else
    setButtonGuiItem(Cancel, KGuiItem(i18n("Cancel"), "dialog-cancel"));
    #endif
    state=State_ScanningTags;
    enableButton(Ok, false);
    enableButton(User1, false);
    progress->setRange(0, origSongs.count());
    progress->setVisible(true);
    statusLabel->setText(i18n("Reading existing tags..."));
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
}

void RgDialog::saveTags()
{
    QStringList failed;
    QMap<QString, Album>::iterator a(albums.begin());
    QMap<QString, Album>::iterator ae(albums.end());

    for (; a!=ae; ++a) {
        foreach (int idx, (*a).tracks) {
            if (needToSave.contains(idx)) {
                const Track &track=tracks[idx];
                if (track.success && Tags::Update_Failed==Tags::updateReplaygain(base+origSongs.at(idx).file,
                                                                                 Tags::ReplayGain(Scanner::reference(track.data.loudness),
                                                                                                  Scanner::reference((*a).data.loudness),
                                                                                                  track.data.peakValue(),
                                                                                                  (*a).data.peakValue()))) {
                    failed.append(origSongs.at(idx).file);
                }
            }
        }
    }

    if (failed.count()) {
        #ifdef ENABLE_KDE_SUPPORT
        KMessageBox::errorList(this, i18n("Failed to update the tags of the following tracks:"), failed);
        #else
        QMessageBox::warning(this, tr("Warning"), 1==failed.count()
                                                    ? tr("Failed to update the tags of %1").arg(failed.at(0))
                                                    : tr("Failed to update the tags of %2 tracks").arg(failed.count()));
        #endif
    }
}

void RgDialog::updateView()
{
    bool allFinished=true;
    quint64 totalProgress=0;
    QMap<int, Track>::iterator it=tracks.begin();
    QMap<int, Track>::iterator end=tracks.end();

    for (; it!=end; ++it) {
        if (!(*it).finished) {
            allFinished=false;
        }
        totalProgress+=(*it).finished ? 100 : (*it).progress;
    }

    if (allFinished) {
        progress->setVisible(false);
        statusLabel->setVisible(false);
        state=State_Idle;
        #ifdef ENABLE_DEVICES_SUPPORT
        setButtonGuiItem(Cancel, KStandardGuiItem::close());
        #else
        setButtonGuiItem(Cancel, KGuiItem(i18n("Close"), "dialog-close"));
        #endif

        enableButton(Ok, needToSave.count());
    } else {
        progress->setValue(totalProgress);
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
Device * RgDialog::getDevice(const QString &udi, QWidget *p)
{
    Device *dev=DevicesModel::self()->device(udi);
    if (!dev) {
        KMessageBox::error(p ? p : this, i18n("Device has been removed!"));
        reject();
        return 0;
    }
    if (!dev->isConnected()) {
        KMessageBox::error(p ? p : this, i18n("Device is not connected."));
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
#endif

void RgDialog::scannerProgress(int p)
{
    Scanner *s=qobject_cast<Scanner *>(sender());
    if (!s) {
        return;
    }

    tracks[s->index()].progress=p;
    updateView();
}

void RgDialog::scannerDone()
{
    Scanner *s=qobject_cast<Scanner *>(sender());
    if (!s) {
        return;
    }
    Track &track=tracks[s->index()];
    if (!track.finished) {
        track.finished=true;
        track.success=s->success();
        QTreeWidgetItem *item=view->topLevelItem(s->index());
        if (s->success()) {
            track.progress=100;
            track.data=s->results();
            item->setText(COL_TRACKGAIN, i18n("%1 dB").arg(formatNumber(Scanner::reference(track.data.loudness), 2)));
            item->setText(COL_TRACKPEAK, formatNumber(track.data.peakValue(), 6));
            QFont f(font());
            f.setItalic(true);
            if (origTags.contains(s->index())) {
                Tags::ReplayGain t=origTags[s->index()];
                bool diff=false;
                if (!Utils::equal(t.trackGain, Scanner::reference(track.data.loudness), 0.01)) {
                    item->setFont(COL_TRACKGAIN, f);
                    diff=true;
                }
                if (!Utils::equal(t.trackPeak, track.data.peakValue(), 0.000001)) {
                    item->setFont(COL_TRACKPEAK, f);
                    diff=true;
                }
                if (diff) {
                    item->setFont(COL_ARTIST, f);
                    item->setFont(COL_TITLE, f);
                    needToSave.insert(s->index());
                } else {
                    needToSave.remove(s->index());
                }
            } else {
                item->setFont(COL_ARTIST, f);
                item->setFont(COL_TITLE, f);
                item->setFont(COL_TRACKGAIN, f);
                item->setFont(COL_TRACKPEAK, f);
                needToSave.insert(s->index());
            }
        } else {
            item->setText(COL_TRACKGAIN, i18n("Failed"));
            item->setText(COL_TRACKPEAK, i18n("Failed"));
            needToSave.remove(s->index());
        }

        QMap<QString, Album>::iterator a(albums.find(createAlbumName(origSongs.at(s->index()))));
        QMap<QString, Album>::iterator ae(albums.end());

        if (a!=ae) {
            (*a).scannedTracks.insert(s->index());
            if ((*a).scannedTracks.count()==(*a).tracks.count()) {
                QList<Scanner *> as;
                foreach (int idx, (*a).tracks) {
                    Scanner *s=scanners[idx];
                    if (s && s->success()) {
                        as.append(s);
                    }
                }

                if (as.count()) {
                    QFont f(font());
                    f.setItalic(true);
                    (*a).data=Scanner::global(as);
                    foreach (int idx, (*a).tracks) {
                        QTreeWidgetItem *item=view->topLevelItem(idx);
                        item->setText(COL_ALBUMGAIN, i18n("%1 dB").arg(formatNumber(Scanner::reference((*a).data.loudness), 2)));
                        item->setText(COL_ALBUMPEAK, formatNumber((*a).data.peak, 6));

                        if (origTags.contains(idx)) {
                            Tags::ReplayGain t=origTags[idx];
                            bool diff=false;
                            if (!Utils::equal(t.albumGain, Scanner::reference((*a).data.loudness), 0.01)) {
                                item->setFont(COL_ALBUMGAIN, f);
                                needToSave.insert(idx);
                                diff=true;
                            }
                            if (!Utils::equal(t.albumPeak, (*a).data.peak, 0.000001)) {
                                item->setFont(COL_ALBUMPEAK, f);
                                needToSave.insert(idx);
                                diff=true;
                            }

                            if (diff) {
                                item->setFont(COL_ARTIST, f);
                                item->setFont(COL_ALBUM, f);
                            }
                        } else {
                            item->setFont(COL_ARTIST, f);
                            item->setFont(COL_ALBUM, f);
                            item->setFont(COL_ALBUMGAIN, f);
                            item->setFont(COL_ALBUMPEAK, f);
                        }
                    }
                }

                foreach (int idx, (*a).tracks) {
                    Scanner *s=scanners[idx];
                    if (s) {
                        scanners.remove(idx);
                        JobController::self()->finishedWith(s);
                    }
                }
            }
        }
        updateView();
    }

    if (!toScan.isEmpty()) {
        int index=toScan.takeAt(0);
        createScanner(index);
    }
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
            item->setText(COL_TRACKGAIN, i18n("%1 dB").arg(formatNumber(tags.trackGain, 2)));
            item->setText(COL_TRACKPEAK, formatNumber(tags.trackPeak, 6));
            item->setText(COL_ALBUMGAIN, i18n("%1 dB").arg(formatNumber(tags.albumGain, 2)));
            item->setText(COL_ALBUMPEAK, formatNumber(tags.albumPeak, 6));
        }
    }
}

void RgDialog::tagReaderDone()
{
    TagReader *t=qobject_cast<TagReader *>(sender());
    if (!t) {
        return;
    }

    t->stop();
    tagReader=0;

    state=State_Idle;
    enableButton(User1, true);
    progress->setVisible(false);
    statusLabel->setVisible(false);
}
