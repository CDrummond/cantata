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

#include "playbacksettings.h"
#include "mpdconnection.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif
#include <QtGui/QListWidget>

PlaybackSettings::PlaybackSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    replayGain->addItem(i18n("None"), QVariant("off"));
    replayGain->addItem(i18n("Track"), QVariant("track"));
    replayGain->addItem(i18n("Album"), QVariant("album"));
    stopFadeDuration->setSpecialValueText(i18n("Do not fadeout"));
    stopFadeDuration->setSuffix(i18n(" ms"));
    #else
    replayGain->addItem(tr("None"), QVariant("off"));
    replayGain->addItem(tr("Track"), QVariant("track"));
    replayGain->addItem(tr("Album"), QVariant("album"));
    stopFadeDuration->setSpecialValueText(tr("Do not fadeout"));
    stopFadeDuration->setSuffix(tr(" ms"));
    #endif
    stopFadeDuration->setRange(Settings::MinFade, Settings::MaxFade);
    stopFadeDuration->setSingleStep(100);
    connect(MPDConnection::self(), SIGNAL(replayGain(const QString &)), this, SLOT(replayGainSetting(const QString &)));
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutpus(const QList<Output> &)));
    connect(MPDConnection::self(), SIGNAL(stateChanged(bool)), this, SLOT(mpdConnectionStateChanged(bool)));
    connect(this, SIGNAL(enable(int)), MPDConnection::self(), SLOT(enableOutput(int)));
    connect(this, SIGNAL(disable(int)), MPDConnection::self(), SLOT(disableOutput(int)));
    connect(this, SIGNAL(outputs()), MPDConnection::self(), SLOT(outputs()));
    connect(this, SIGNAL(setReplayGain(const QString &)), MPDConnection::self(), SLOT(setReplayGain(const QString &)));
    connect(this, SIGNAL(setCrossFade(int)), MPDConnection::self(), SLOT(setCrossFade(int)));
    connect(this, SIGNAL(getReplayGain()), MPDConnection::self(), SLOT(getReplayGain()));
    mpdConnectionStateChanged(MPDConnection::self()->isConnected());
    errorIcon->setPixmap(QIcon::fromTheme("dialog-error").pixmap(32, 32));
};

void PlaybackSettings::load()
{
    crossfading->setValue(MPDStatus::self()->crossFade());
    emit getReplayGain();
    emit outputs();
    stopOnExit->setChecked(Settings::self()->stopOnExit());
    stopFadeDuration->setValue(Settings::self()->stopFadeDuration());
    scrollPlayQueue->setChecked(Settings::self()->scrollPlayQueue());
}

void PlaybackSettings::save()
{
    emit setCrossFade(crossfading->value());
    emit setReplayGain(replayGain->itemData(replayGain->currentIndex()).toString());
    Settings::self()->saveStopOnExit(stopOnExit->isChecked());
    Settings::self()->saveStopFadeDuration(stopFadeDuration->value());
    Settings::self()->saveScrollPlayQueue(scrollPlayQueue->isChecked());
    for (int i=0; i<view->count(); ++i) {
        QListWidgetItem *item=view->item(i);

        if (Qt::Checked==item->checkState()) {
            emit enable(item->data(Qt::UserRole).toInt());
        } else {
            emit disable(item->data(Qt::UserRole).toInt());
        }
    }
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

void PlaybackSettings::updateOutpus(const QList<Output> &outputs)
{
    view->clear();
    foreach(const Output &output, outputs) {
        QListWidgetItem *item=new QListWidgetItem(output.name, view);
        item->setCheckState(output.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, output.id);
    }
}

void PlaybackSettings::mpdConnectionStateChanged(bool c)
{
    errorIcon->setVisible(!c);
    errorLabel->setVisible(!c);
    outputFrame->setEnabled(c);
    crossfading->setEnabled(c);
    replayGain->setEnabled(c);
    crossfadingLabel->setEnabled(c);
    replayGainLabel->setEnabled(c);
}
