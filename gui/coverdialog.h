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

#ifndef COVER_DIALOG_H
#define COVER_DIALOG_H

#include "config.h"
#include "support/dialog.h"
#include "mpd-interface/song.h"
#include "ui_coverdialog.h"
#include "covers.h"
#include <QSet>
#include <QList>
#include <QMap>

class NetworkJob;
class QUrl;
class QTemporaryFile;
class QListWidgetItem;
class QLabel;
class QProgressBar;
class QScrollArea;
class QWheelEvent;
class CoverItem;
class ExistingCover;
class Spinner;
class MessageOverlay;
class QAction;
class QMenu;

class CoverPreview : public Dialog
{
    Q_OBJECT

public:
    CoverPreview(QWidget *p);
    ~CoverPreview() override { }
    void showImage(const QImage &img, const QString &u);
    void downloading(const QString &u);
    bool aboutToShow(const QString &u) const { return u==url; }

private Q_SLOTS:
    void progress(qint64 rx, qint64 total);

private:
    void scaleImage(int adjust);
    void wheelEvent(QWheelEvent *event) override;

private:
    QString url;
    QLabel *loadingLabel;
    QProgressBar *pbar;
    QLabel *imageLabel;
    QScrollArea *scrollArea;
    double zoom;
    int imgW;
    int imgH;
};

class CoverDialog : public Dialog, public Ui::CoverDialog
{
    Q_OBJECT

public:
    static int instanceCount();

    enum DownloadType {
        DL_Query,
        DL_Thumbnail,
        DL_LargePreview,
        DL_LargeSave
    };

    CoverDialog(QWidget *parent);
    ~CoverDialog() override;

    void show(const Song &s, const Covers::Image &current=Covers::Image());
    int imageSize() const { return iSize; }

Q_SIGNALS:
    void selectedCover(const QImage &img, const QString &fileName);

private Q_SLOTS:
    void queryJobFinished();
    void downloadJobFinished();
    void showImage(QListWidgetItem *item);
    void sendQuery();
    void cancelQuery();
    void checkStatus();
    void addLocalFile();
    void menuRequested(const QPoint &pos);
    void showImage();
    void removeImages();
    void updateProviders();

private:
    void sendLastFmQuery(const QString &fixedQuery, int page);
    void sendGoogleQuery(const QString &fixedQuery, int page);
    void sendSpotifyQuery(const QString &fixedQuery);
    void sendITunesQuery(const QString &fixedQuery);
    void sendDeezerQuery(const QString &fixedQuery);
    CoverPreview *previewDialog();
    void insertItem(CoverItem *item);
    NetworkJob * downloadImage(const QString &url, DownloadType dlType);
    void downloadThumbnail(const QString &thumbUrl, const QString &largeUrl, const QString &host, int w=-1, int h=-1, int sz=-1);
    void clearTempFiles();
    void sendQueryRequest(const QUrl &url, const QString &host=QString());
    void parseLastFmQueryResponse(const QByteArray &resp);
    void parseGoogleQueryResponse(const QByteArray &resp);
    void parseCoverArtArchiveQueryResponse(const QByteArray &resp);
    void parseSpotifyQueryResponse(const QByteArray &resp);
    void parseITunesQueryResponse(const QByteArray &resp);
    void parseDeezerQueryResponse(const QByteArray &resp);
    void slotButtonClicked(int button) override;
    bool saveCover(const QString &src, const QImage &img);
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void setSearching(bool s);
    void addProvider(QMenu *mnu, const QString &name, int bit, int value);

private:
    int enabledProviders;
    Song song;
    ExistingCover *existing;
    QString currentQueryString;
    int currentQueryProviders;
    QSet<NetworkJob *> currentQuery;
    QSet<QString> currentUrls;
    QSet<QString> currentLocalCovers;
    QList<QTemporaryFile *> tempFiles;
    CoverPreview *preview;
    bool saving;
    bool isArtist;
    bool isComposer;
    int iSize;
    Spinner *spinner;
    MessageOverlay *msgOverlay;
    int page;
    QMenu *menu;
    QAction *showAction;
    QAction *removeAction;
    QMap<int, QAction *> providers;
};

#endif
