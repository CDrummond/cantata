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

#include "playbacksettings.h"
#include "settings.h"
#include "support/utils.h"
#include "mpd-interface/mpdconnection.h"
#include "support/icon.h"
#include "widgets/basicitemdelegate.h"
#include "support/messagebox.h"
#include <QListWidget>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    stopFadeDuration->setRange(MPDConnection::MinFade, MPDConnection::MaxFade);
    stopFadeDuration->setSingleStep(100);

    outputsView->setItemDelegate(new BasicItemDelegate(outputsView));
    replayGain->addItem(tr("None"), QVariant("off"));
    replayGain->addItem(tr("Track"), QVariant("track"));
    replayGain->addItem(tr("Album"), QVariant("album"));
    replayGain->addItem(tr("Auto"), QVariant("auto"));
    connect(MPDConnection::self(), SIGNAL(replayGain(const QString &)), this, SLOT(replayGainSetting(const QString &)));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutputs(const QList<Output> &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(mpdConnectionStateChanged(bool)));
    connect(this, SIGNAL(enableOutput(quint32, bool)), MPDConnection::self(), SLOT(enableOutput(quint32, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(setReplayGain(const QString &)), MPDConnection::self(), SLOT(setReplayGain(const QString &)));
    connect(this, SIGNAL(setCrossFade(int)), MPDConnection::self(), SLOT(setCrossFade(int)));
    connect(this, SIGNAL(getReplayGain()), MPDConnection::self(), SLOT(getReplayGain()));
    connect(aboutReplayGain, SIGNAL(leftClickedUrl()), SLOT(showAboutReplayGain()));
    int iconSize=Icon::dlgIconSize();
    messageIcon->setFixedSize(iconSize, iconSize);
    mpdConnectionStateChanged(MPDConnection::self()->isConnected());
    #if defined Q_OS_WIN || (defined Q_OS_MAC && !defined IOKIT_FOUND)
    REMOVE(inhibitSuspend)
    #endif
    outputsView->setVisible(outputsView->count()>1);
    outputsViewLabel->setVisible(outputsView->count()>1);

    #ifdef Q_OS_MAC
    expandingSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    #endif
    volumeStep->setToolTip(volumeStepLabel->toolTip());
}

void PlaybackSettings::load()
{
    stopOnExit->setChecked(Settings::self()->stopOnExit());
    stopFadeDuration->setValue(Settings::self()->stopFadeDuration());
    #if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
    inhibitSuspend->setChecked(Settings::self()->inhibitSuspend());
    #endif
    volumeStep->setValue(Settings::self()->volumeStep());

    crossfading->setValue(MPDStatus::self()->crossFade());
    if (MPDConnection::self()->isConnected()) {
        emit getReplayGain();
        emit outputs();
        applyReplayGain->setChecked(MPDConnection::self()->getDetails().applyReplayGain);
    }
}

void PlaybackSettings::save()
{
    Settings::self()->saveStopOnExit(stopOnExit->isChecked());
    Settings::self()->saveStopFadeDuration(stopFadeDuration->value());
    #if (defined Q_OS_LINUX && defined QT_QTDBUS_FOUND) || (defined Q_OS_MAC && defined IOKIT_FOUND)
    Settings::self()->saveInhibitSuspend(inhibitSuspend->isChecked());
    #endif
    Settings::self()->saveVolumeStep(volumeStep->value());

    if (MPDConnection::self()->isConnected()) {
        int crossFade=crossfading->value();
        if (crossFade!=MPDStatus::self()->crossFade()) {
            emit setCrossFade(crossFade);
        }
        QString rg=replayGain->itemData(replayGain->currentIndex()).toString();
        if (rgSetting!=rg) {
            rgSetting=rg;
            emit setReplayGain(rg);
        }
        MPDConnectionDetails det=MPDConnection::self()->getDetails();
        if (det.applyReplayGain!=applyReplayGain->isChecked()) {
            det.applyReplayGain=applyReplayGain->isChecked();
            Settings::self()->saveConnectionDetails(det);
        }
        if (outputsView->isVisible()) {
            for (int i=0; i<outputsView->count(); ++i) {
                QListWidgetItem *item=outputsView->item(i);
                bool isEnabled=Qt::Checked==item->checkState();
                int id=item->data(Qt::UserRole).toInt();
                if (isEnabled && !enabledOutputs.contains(id)) {
                    enabledOutputs.insert(id);
                    emit enableOutput(id, isEnabled);
                } else if (!isEnabled && enabledOutputs.contains(id)) {
                    enabledOutputs.remove(id);
                    emit enableOutput(id, isEnabled);
                }
            }
        }
    }
}

void PlaybackSettings::replayGainSetting(const QString &rg)
{
    rgSetting=rg;
    replayGain->setCurrentIndex(0);

    for(int i=0; i<replayGain->count(); ++i) {
        if (replayGain->itemData(i).toString()==rg){
            replayGain->setCurrentIndex(i);
            break;
        }
    }
}

void PlaybackSettings::updateOutputs(const QList<Output> &outputs)
{
    outputsView->clear();
    enabledOutputs.clear();
    for (const Output &output: outputs) {
        QListWidgetItem *item=new QListWidgetItem(output.name, outputsView);
        item->setCheckState(output.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, output.id);
        if (output.enabled) {
            enabledOutputs.insert(output.id);
        }
    }
    outputsView->setVisible(outputsView->count()>1);
    outputsViewLabel->setVisible(outputsView->count()>1);
}

void PlaybackSettings::mpdConnectionStateChanged(bool c)
{
    bool rgSupported=c && MPDConnection::self()->replaygainSupported();
    outputsView->setEnabled(c);
    outputsViewLabel->setEnabled(c);
    crossfading->setEnabled(c);
    crossfadingLabel->setEnabled(c);
    replayGainLabel->setEnabled(rgSupported);
    replayGain->setEnabled(rgSupported);
    applyReplayGain->setChecked(MPDConnection::self()->getDetails().applyReplayGain);
    messageIcon->setPixmap(style()->standardIcon(c ? QStyle::SP_MessageBoxInformation : QStyle::SP_MessageBoxWarning).pixmap(messageIcon->minimumSize()));
    if (c) {
        messageLabel->setText(tr("<i>Connected to %1<br/>The entries below apply to the currently connected MPD collection.</i>")
                                 .arg(MPDConnection::self()->getDetails().description()));
    } else {
        messageLabel->setText(tr("<i>Not Connected!<br/>The entries below cannot be modified, as Cantata is not connected to MPD.</i>"));
        outputsView->clear();
    }
}

void PlaybackSettings::showAboutReplayGain()
{
    MessageBox::information(this, tr("Replay Gain is a proposed standard published in 2001 to normalize the perceived loudness of computer "
                                       "audio formats such as MP3 and Ogg Vorbis. It works on a track/album basis, and is now supported in a "
                                       "growing number of players."
                                       "<br/><br/>The following ReplayGain settings may be used:<ul>"
                                       "<li><i>None</i> - No ReplayGain is applied.</li>"
                                       "<li><i>Track</i> - Volume will be adjusted using the track's ReplayGain tags.</li>"
                                       "<li><i>Album</i> - Volume will be adjusted using the albums's ReplayGain tags.</li>"
                                       "<li><i>Auto</i> - Volume will be adjusted using the track's ReplayGain tags if random play is activated, otherwise the album's tags will be used.</li>"
                                       "</ul>"));
}

#include "moc_playbacksettings.cpp"
