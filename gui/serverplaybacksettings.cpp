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

#include "serverplaybacksettings.h"
#include "mpdconnection.h"
#include "localize.h"
#include "settings.h"
#include "icon.h"
#include <QtGui/QListWidget>

static const int constIconSize=48;

ServerPlaybackSettings::ServerPlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    replayGain->addItem(i18n("None"), QVariant("off"));
    replayGain->addItem(i18n("Track"), QVariant("track"));
    replayGain->addItem(i18n("Album"), QVariant("album"));
    connect(MPDConnection::self(), SIGNAL(replayGain(const QString &)), this, SLOT(replayGainSetting(const QString &)));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutpus(const QList<Output> &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(mpdConnectionStateChanged(bool)));
    connect(this, SIGNAL(enableOutput(int, bool)), MPDConnection::self(), SLOT(enableOutput(int, bool)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(setReplayGain(const QString &)), MPDConnection::self(), SLOT(setReplayGain(const QString &)));
    connect(this, SIGNAL(setCrossFade(int)), MPDConnection::self(), SLOT(setCrossFade(int)));
    connect(this, SIGNAL(getReplayGain()), MPDConnection::self(), SLOT(getReplayGain()));
    messageIcon->setMinimumSize(constIconSize, constIconSize);
    messageIcon->setMaximumSize(constIconSize, constIconSize);
    mpdConnectionStateChanged(MPDConnection::self()->isConnected());
    #ifndef PHONON_FOUND
    streamUrl->setVisible(false);
    streamUrlLabel->setVisible(false);
    streamUrlInfoLabel->setVisible(false);
    #endif
};

void ServerPlaybackSettings::load()
{
    crossfading->setValue(MPDStatus::self()->crossFade());
    if (MPDConnection::self()->isConnected()) {
        emit getReplayGain();
        emit outputs();
    }
    #ifdef PHONON_FOUND
    streamUrl->setText(Settings::self()->streamUrl());
    #endif
}

void ServerPlaybackSettings::save()
{
    if (MPDConnection::self()->isConnected()) {
        emit setCrossFade(crossfading->value());
        emit setReplayGain(replayGain->itemData(replayGain->currentIndex()).toString());
        for (int i=0; i<view->count(); ++i) {
            QListWidgetItem *item=view->item(i);
            emit enableOutput(item->data(Qt::UserRole).toInt(), Qt::Checked==item->checkState());
        }
    }
    #ifdef PHONON_FOUND
    Settings::self()->saveStreamUrl(streamUrl->text().trimmed());
    #endif
}

void ServerPlaybackSettings::replayGainSetting(const QString &rg)
{
    replayGain->setCurrentIndex(0);

    for(int i=0; i<replayGain->count(); ++i) {
        if (replayGain->itemData(i).toString()==rg){
            replayGain->setCurrentIndex(i);
            break;
        }
    }
}

void ServerPlaybackSettings::updateOutpus(const QList<Output> &outputs)
{
    view->clear();
    foreach(const Output &output, outputs) {
        QListWidgetItem *item=new QListWidgetItem(output.name, view);
        item->setCheckState(output.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, output.id);
    }
}

void ServerPlaybackSettings::mpdConnectionStateChanged(bool c)
{
    outputFrame->setEnabled(c);
    crossfading->setEnabled(c);
    replayGain->setEnabled(c);
    crossfadingLabel->setEnabled(c);
    replayGainLabel->setEnabled(c);
    #ifdef PHONON_FOUND
    streamUrl->setEnabled(c);
    streamUrlLabel->setEnabled(c);
    #endif
    messageIcon->setPixmap(Icon(c ? "dialog-information" : "dialog-warning").pixmap(constIconSize, constIconSize));
    if (c) {
        messageLabel->setText(i18n("<i><b>Connected to %1</b><br/>The entries below apply to the currently connected MPD instance.</i>")
                              .arg(MPDConnection::self()->getDetails().description()));
    } else {
        messageLabel->setText(i18n("<i><b>Not Connected.</b><br/>The entries below cannot be modified, as Cantata is not connected to MPD.</i>"));
        view->clear();
    }
}
