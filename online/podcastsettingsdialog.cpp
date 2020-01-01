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

#include "podcastsettingsdialog.h"
#include "support/buddylabel.h"
#include "support/pathrequester.h"
#include "gui/settings.h"
#include "support/utils.h"
#include <QComboBox>
#include <QCheckBox>
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
    : Dialog(p, "PodcastSettingsDialog", QSize(550, 160))
{
    QWidget *mw=new QWidget(this);
    QFormLayout * lay=new QFormLayout(mw);
    BuddyLabel * updateLabel=new BuddyLabel(tr("Check for new episodes:"), mw);
    BuddyLabel * downloadLabel=new BuddyLabel(tr("Download episodes to:"), mw);
    BuddyLabel * autoDownloadLabel=new BuddyLabel(tr("Download automatically:"), mw);

    updateCombo = new QComboBox(this);
    updateLabel->setBuddy(updateCombo);
    downloadPath = new PathRequester(this);
    downloadLabel->setBuddy(downloadPath);
    downloadPath->setDirMode(true);
    autoDownloadCombo = new QComboBox(this);
    autoDownloadLabel->setBuddy(autoDownloadCombo);

    int row=0;
    lay->setMargin(0);
    lay->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    lay->setWidget(row, QFormLayout::LabelRole, updateLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, updateCombo);
    lay->setWidget(row, QFormLayout::LabelRole, downloadLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, downloadPath);
    lay->setWidget(row, QFormLayout::LabelRole, downloadLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, downloadPath);
    lay->setWidget(row, QFormLayout::LabelRole, autoDownloadLabel);
    lay->setWidget(row++, QFormLayout::FieldRole, autoDownloadCombo);

    setButtons(Ok|Cancel);
    setMainWidget(mw);
    setCaption(tr("Podcast Settings"));

    updateCombo->addItem(tr("Manually"), 0);
    updateCombo->addItem(tr("Every 15 minutes"), 15);
    updateCombo->addItem(tr("Every 30 minutes"), 30);
    updateCombo->addItem(tr("Every hour"), 60);
    updateCombo->addItem(tr("Every 2 hours"), 2*60);
    updateCombo->addItem(tr("Every 6 hours"), 6*60);
    updateCombo->addItem(tr("Every 12 hours"), 12*60);
    updateCombo->addItem(tr("Every day"), 24*60);
    updateCombo->addItem(tr("Every week"), 7*24*60);

    autoDownloadCombo->addItem(tr("Don't automatically download episodes"), 0);
    autoDownloadCombo->addItem(tr("Latest episode"), 1);
    autoDownloadCombo->addItem(tr("Latest %1 episodes").arg(2), 2);
    autoDownloadCombo->addItem(tr("Latest %1 episodes").arg(5), 5);
    autoDownloadCombo->addItem(tr("Latest %1 episodes").arg(10), 10);
    autoDownloadCombo->addItem(tr("Latest %1 episodes").arg(15), 15);
    autoDownloadCombo->addItem(tr("Latest %1 episodes").arg(20), 20);
    autoDownloadCombo->addItem(tr("Latest %1 episodes").arg(50), 50);
    autoDownloadCombo->addItem(tr("All episodes"), 1000);

    origRssUpdate=Settings::self()->rssUpdate();
    setIndex(updateCombo, origRssUpdate);
    connect(updateCombo, SIGNAL(currentIndexChanged(int)), SLOT(checkSaveable()));
    origPodcastDownloadPath=Utils::convertPathForDisplay(Settings::self()->podcastDownloadPath());
    origPodcastAutoDownload=Settings::self()->podcastAutoDownloadLimit();
    setIndex(autoDownloadCombo, origPodcastAutoDownload);
    downloadPath->setText(origPodcastDownloadPath);
    connect(downloadPath, SIGNAL(textChanged(QString)), SLOT(checkSaveable()));
    connect(autoDownloadCombo, SIGNAL(currentIndexChanged(int)), SLOT(checkSaveable()));
    enableButton(Ok, false);
    changed=0;
}

void PodcastSettingsDialog::checkSaveable()
{
    enableButton(Ok, autoDownloadCombo->itemData(autoDownloadCombo->currentIndex()).toInt()!=origPodcastAutoDownload ||
                     updateCombo->itemData(updateCombo->currentIndex()).toInt()!=origRssUpdate ||
                     downloadPath->text().trimmed()!=origPodcastDownloadPath);
}

void PodcastSettingsDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        if (updateCombo->itemData(updateCombo->currentIndex()).toInt()!=origRssUpdate) {
            changed|=RssUpdate;
            Settings::self()->saveRssUpdate(updateCombo->itemData(updateCombo->currentIndex()).toInt());
        }
        if (downloadPath->text().trimmed()!=origPodcastDownloadPath) {
            changed|=DownloadPath;
            Settings::self()->savePodcastDownloadPath(Utils::convertPathFromDisplay(downloadPath->text().trimmed()));
        }
        if (autoDownloadCombo->itemData(autoDownloadCombo->currentIndex()).toInt()!=origPodcastAutoDownload) {
            changed|=AutoDownload;
            Settings::self()->savePodcastAutoDownloadLimit(autoDownloadCombo->itemData(autoDownloadCombo->currentIndex()).toInt());
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

#include "moc_podcastsettingsdialog.cpp"
