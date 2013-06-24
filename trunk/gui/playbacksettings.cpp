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

#include "playbacksettings.h"
#include "settings.h"
#include "localize.h"
#include "mpdconnection.h"
#include "icon.h"
#include "treeview.h"
#include "messagebox.h"
#include <QListWidget>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    stopFadeDuration->setSpecialValueText(i18n("Do not fadeout"));
    stopFadeDuration->setSuffix(i18n(" ms"));
    stopFadeDuration->setRange(Settings::MinFade, Settings::MaxFade);
    stopFadeDuration->setSingleStep(100);

    outputsView->setItemDelegate(new SimpleTreeViewDelegate(outputsView));
    replayGain->addItem(i18n("None"), QVariant("off"));
    replayGain->addItem(i18n("Track"), QVariant("track"));
    replayGain->addItem(i18n("Album"), QVariant("album"));
    replayGain->addItem(i18n("Auto"), QVariant("auto"));
    connect(MPDConnection::self(), SIGNAL(replayGain(const QString &)), this, SLOT(replayGainSetting(const QString &)));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutputs(const QList<Output> &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(mpdConnectionStateChanged(bool)));
    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(setReplayGain(const QString &)), MPDConnection::self(), SLOT(setReplayGain(const QString &)));
    connect(this, SIGNAL(setCrossFade(int)), MPDConnection::self(), SLOT(setCrossFade(int)));
    connect(this, SIGNAL(getReplayGain()), MPDConnection::self(), SLOT(getReplayGain()));
    connect(aboutReplayGain, SIGNAL(leftClickedUrl()), SLOT(showAboutReplayGain()));
    int iconSize=Icon::dlgIconSize();
    messageIcon->setMinimumSize(iconSize, iconSize);
    messageIcon->setMaximumSize(iconSize, iconSize);
    mpdConnectionStateChanged(MPDConnection::self()->isConnected());
    #ifndef ENABLE_HTTP_STREAM_PLAYBACK
    REMOVE(streamBox)
    #endif
}

void PlaybackSettings::load()
{
    stopOnExit->setChecked(Settings::self()->stopOnExit());
    stopFadeDuration->setValue(Settings::self()->stopFadeDuration());
    stopDynamizerOnExit->setChecked(Settings::self()->stopDynamizerOnExit());

    crossfading->setValue(MPDStatus::self()->crossFade());
    if (MPDConnection::self()->isConnected()) {
        emit getReplayGain();
        emit outputs();
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamUrl->setText(Settings::self()->streamUrl());
    #endif
}

void PlaybackSettings::save()
{
    Settings::self()->saveStopOnExit(stopOnExit->isChecked());
    Settings::self()->saveStopFadeDuration(stopFadeDuration->value());
    Settings::self()->saveStopDynamizerOnExit(stopDynamizerOnExit->isChecked());

    if (MPDConnection::self()->isConnected()) {
        emit setCrossFade(crossfading->value());
        emit setReplayGain(replayGain->itemData(replayGain->currentIndex()).toString());
        for (int i=0; i<outputsView->count(); ++i) {
            QListWidgetItem *item=outputsView->item(i);
            emit enableOutput(item->data(Qt::UserRole).toInt(), Qt::Checked==item->checkState());
        }
    }
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    Settings::self()->saveStreamUrl(streamUrl->text().trimmed());
    #endif
}

void PlaybackSettings::replayGainSetting(const QString &rg)
{
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
    foreach(const Output &output, outputs) {
        QListWidgetItem *item=new QListWidgetItem(output.name, outputsView);
        item->setCheckState(output.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, output.id);
    }
    outputsView->setVisible(outputsView->count()>1);
    outputsViewLabel->setVisible(outputsView->count()>1);
}

void PlaybackSettings::mpdConnectionStateChanged(bool c)
{
    outputsView->setEnabled(c);
    outputsViewLabel->setEnabled(c);
    crossfading->setEnabled(c);
    replayGain->setEnabled(c);
    crossfadingLabel->setEnabled(c);
    replayGainLabel->setEnabled(c);
    #ifdef ENABLE_HTTP_STREAM_PLAYBACK
    streamUrl->setEnabled(c);
    streamUrlLabel->setEnabled(c);
    #endif
    messageIcon->setPixmap(Icon(c ? "dialog-information" : "dialog-warning").pixmap(messageIcon->minimumSize()));
    if (c) {
        messageLabel->setText(i18n("<i>Connected to %1<br/>The entries below apply to the currently connected MPD collection.</i>")
                              .arg(MPDConnection::self()->getDetails().description()));
    } else {
        messageLabel->setText(i18n("<i>Not Connected!<br/>The entries below cannot be modified, as Cantata is not connected to MPD.</i>"));
        outputsView->clear();
    }
}

void PlaybackSettings::showAboutReplayGain()
{
    MessageBox::information(this, i18n("<p>Replay Gain is a proposed standard published in 2001 to normalize the perceived loudness of computer "
                                       "audio formats such as MP3 and Ogg Vorbis. It works on a track/album basis, and is now supported in a "
                                       "growing number of players.</p>"
                                       "<p>The following ReplayGain settings may be used:<ul>"
                                       "<li><i>None</i> - No ReplayGain is applied.</li>"
                                       "<li><i>Track</i> - Volume will be adjusted using the track's ReplayGain tags.</li>"
                                       "<li><i>Album</i> - Volume will be adjusted using the albums's ReplayGain tags.</li>"
                                       "<li><i>Auto</i> - Volume will be adjusted using the track's ReplayGain tags if random play is actived, otherwise the album's tags will be used.</li>"
                                       "</ul></p>"));
}
