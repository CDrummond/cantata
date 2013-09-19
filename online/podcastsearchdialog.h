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
#include <QList>
#include <QUrl>

class QPushButton;
class LineEdit;
class QTreeWidget;
class NetworkJob;
class QIODevice;
class Spinner;
class QTreeWidgetItem;

class PodcastPage : public QWidget
{
    Q_OBJECT
public:
    PodcastPage(QWidget *p);
    virtual ~PodcastPage() { cancel(); }
    
Q_SIGNALS:
    void rssSelected(const QUrl &url);

protected:
    void fetch(const QUrl &url);
    void cancel();
    void addPodcast(const QString &name, const QUrl &url, const QString &description, QTreeWidgetItem *p);

private Q_SLOTS:
    void selectionChanged();
    void jobFinished();

private:
    virtual void parseResonse(QIODevice *dev) = 0;

protected:
    Spinner *spinner;
    QTreeWidget *tree;
    NetworkJob *job;
};

class PodcastSearchPage : public PodcastPage
{
    Q_OBJECT
public:
    PodcastSearchPage(QWidget *p);
    virtual ~PodcastSearchPage() { }
    
private Q_SLOTS:
    virtual void doSearch() = 0;

protected:
    LineEdit *search;
    QPushButton *searchButton;
};

class PodcastSearchDialog : public Dialog
{
    Q_OBJECT
public:
    static int instanceCount();

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

