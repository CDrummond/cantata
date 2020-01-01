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

#ifndef PODCAST_SETTINGS_DIALOG_H
#define PODCAST_SETTINGS_DIALOG_H

#include "support/dialog.h"

class QComboBox;
class QCheckBox;
class PathRequester;

class PodcastSettingsDialog : public Dialog
{
    Q_OBJECT
public:
    enum Changes {
        RssUpdate    = 0x01,
        DownloadPath = 0x02,
        AutoDownload = 0x04
    };

    PodcastSettingsDialog(QWidget *p);
    ~PodcastSettingsDialog() override { }

    int changes() const { return changed; }

private Q_SLOTS:
    void checkSaveable();

private:
    void slotButtonClicked(int button) override;

private:
    QComboBox *updateCombo;
    int origRssUpdate;
    PathRequester *downloadPath;
    QComboBox *autoDownloadCombo;
    QString origPodcastDownloadPath;
    int origPodcastAutoDownload;
    int changed;
};


#endif
