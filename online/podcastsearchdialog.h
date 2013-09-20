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

#ifndef PODCAST_SEARCH_DIALOG_H
#define PODCAST_SEARCH_DIALOG_H

#include "dialog.h"
#include "icon.h"
#include <QList>
#include <QUrl>

class QPushButton;
class LineEdit;
class QTreeWidget;
class NetworkJob;
class QIODevice;
class Spinner;
class QTreeWidgetItem;
class TextBrowser;

namespace OpmlParser
{
struct Category;
struct Podcast;
}

class PodcastPage : public QWidget
{
    Q_OBJECT
public:
    PodcastPage(QWidget *p, const QString &n);
    virtual ~PodcastPage() { cancel(); cancelImage(); }
    
    const Icon & icon() const { return icn; }
    const QString & name() const { return pageName; }

Q_SIGNALS:
    void rssSelected(const QUrl &url);

protected:
    void fetch(const QUrl &url);
    void fetchImage(const QUrl &url);
    void cancel();
    void cancelImage();
    void addPodcast(const QString &title, const QUrl &url, const QUrl &image, const QString &description, const QString &webPage, QTreeWidgetItem *p);

private Q_SLOTS:
    void selectionChanged();
    void jobFinished();
    void imageJobFinished();
    void openLink(const QUrl &url);

private:
    void updateText();
    virtual void parseResonse(QIODevice *dev) = 0;

protected:
    QString pageName;
    Spinner *spinner;
    Spinner *imageSpinner;
    QTreeWidget *tree;
    TextBrowser *text;
    NetworkJob *job;
    NetworkJob *imageJob;
    Icon icn;
};

class PodcastSearchPage : public PodcastPage
{
    Q_OBJECT
public:
    PodcastSearchPage(QWidget *p, const QString &n, const QString &i, const QUrl &qu, const QString &qk, const QStringList &other=QStringList());
    virtual ~PodcastSearchPage() { }
    
    void showEvent(QShowEvent *e);

private:
    void parseResonse(QIODevice *dev);

private Q_SLOTS:
    virtual void doSearch();
    virtual void parse(const QVariant &data)=0;

protected:
    LineEdit *search;
    QPushButton *searchButton;
    QString currentSearch;
    QUrl queryUrl;
    QString queryKey;
    QStringList otherArgs;
};

class OpmlBrowsePage : public PodcastPage
{
    Q_OBJECT
public:
    OpmlBrowsePage(QWidget *p, const QString &n, const QString &i, const QUrl &u);
    virtual ~OpmlBrowsePage() { }

    void showEvent(QShowEvent *e);

private Q_SLOTS:
    void reload();

private:
    void parseResonse(QIODevice *dev);
    void addCategory(const OpmlParser::Category &cat, QTreeWidgetItem *p);
    void addPodcast(const OpmlParser::Podcast &pod, QTreeWidgetItem *p);

private:
    bool loaded;
    QUrl url;
};

class PodcastSearchDialog : public Dialog
{
    Q_OBJECT
public:
    static int instanceCount();
    static QString constCacheDir;
    static QString constExt;

    PodcastSearchDialog(QWidget *parent);
    virtual ~PodcastSearchDialog();

private Q_SLOTS:
    void rssSelected(const QUrl &url);

private:
    void slotButtonClicked(int button);

private:
    QUrl currentUrl;
};

#endif

