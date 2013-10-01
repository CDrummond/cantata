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

#include "podcastsettingsdialog.h"
#include "buddylabel.h"
#include "pathrequester.h"
#include "settings.h"
#include "onoffbutton.h"
#include <QComboBox>
#include <QFormLayout>

static void setIndex(QComboBox *combo, int val)
{
    int possible=0;
    for (int i=0; i<combo->count(); ++i) {
        int cval=combo->itemData(i).toInt();
        if (cval==val) {
            combo->setCurrentIndex(i);
            possible=-1;
            break;
        }
        if (cval<val) {
            possible=i;
        }
    }

    if (possible>=0) {
        combo->setCurrentIndex(possible);
    }
}

PodcastSettingsDialog::PodcastSettingsDialog(QWidget *p)
    : Dialog(p)
{
    QWidget *mw=new QWidget(this);
    QFormLayout * lay=new QFormLayout(mw);
    BuddyLabel * updateLabel=new BuddyLabel(i18n("Check for new episodes:"), mw);
    BuddyLabel * downloadLabel=new BuddyLabel(i18n("Download episodes to:"), mw);
    BuddyLabel * autoDownloadLabel=new BuddyLabel(i18n("Automatically download new episodes:"), mw);

    updateCombo = new QComboBox(this);
    updateLabel->setBuddy(updateCombo);
    downloadPath = new PathRequester(this);
    downloadLabel->setBuddy(downloadPath);
    downloadPath->setDirMode(true);
    autoDownload = new OnOffButton(this);
    autoDownloadLabel->setBuddy(autoDownload);

    int row=0;
    lay->setWidget(row, QFormLayout::LabelRole, updateLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, updateCombo);
    lay->setWidget(row, QFormLayout::LabelRole, downloadLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, downloadPath);
    lay->setWidget(row, QFormLayout::LabelRole, downloadLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, downloadPath);
    lay->setWidget(row, QFormLayout::LabelRole, autoDownloadLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, autoDownload);
    setButtons(Ok|Cancel);
    setMainWidget(mw);
    setCaption(i18n("Podcast Settings"));

    updateCombo->addItem(i18n("Manually"), 0);
    updateCombo->addItem(i18n("Every 15 minutes"), 15);
    updateCombo->addItem(i18n("Every 30 minutes"), 30);
    updateCombo->addItem(i18n("Every hour"), 60);
    updateCombo->addItem(i18n("Every 2 hours"), 2*60);
    updateCombo->addItem(i18n("Every 6 hours"), 6*60);
    updateCombo->addItem(i18n("Every 12 hours"), 12*60);
    updateCombo->addItem(i18n("Every day"), 24*60);
    updateCombo->addItem(i18n("Every week"), 7*24*60);

    origRssUpdate=Settings::self()->rssUpdate();
    origPodcastDownloadPath=Settings::self()->podcastDownloadPath();
    origPodcastAutoDownload=Settings::self()->podcastAutoDownload();

    setIndex(updateCombo, origRssUpdate);
    downloadPath->setText(origPodcastDownloadPath);
    autoDownload->setChecked(origPodcastAutoDownload);

    connect(updateCombo, SIGNAL(currentIndexChanged(int)), SLOT(checkSaveable()));
    connect(downloadPath, SIGNAL(textChanged(QString)), SLOT(checkSaveable()));
    connect(autoDownload, SIGNAL(toggled(bool)), SLOT(checkSaveable()));
    enableButton(Ok, false);
    changed=0;
}

void PodcastSettingsDialog::checkSaveable()
{
    enableButton(Ok, origPodcastAutoDownload!=autoDownload->isChecked() ||
                     updateCombo->itemData(updateCombo->currentIndex()).toInt()!=origRssUpdate ||
                     downloadPath->text().trimmed()!=origPodcastDownloadPath);
}

void PodcastSettingsDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        if (origPodcastAutoDownload!=autoDownload->isChecked()) {
            changed|=AutoDownload;
            Settings::self()->savePodcastAutoDownload(autoDownload->isChecked());
        }
        if (updateCombo->itemData(updateCombo->currentIndex()).toInt()!=origRssUpdate) {
            changed|=RssUpdate;
            Settings::self()->saveRssUpdate(updateCombo->itemData(updateCombo->currentIndex()).toInt());
        }
        if (downloadPath->text().trimmed()!=origPodcastDownloadPath) {
            changed|=DownloadPath;
            Settings::self()->savePodcastDownloadPath(downloadPath->text().trimmed());
        }
        accept();
    case Close:
    case Cancel:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}
