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

#ifndef WIKIPEDIA_SETTINGS_H
#define WIKIPEDIA_SETTINGS_H

#include "togglelist.h"
#include <QMap>

class NetworkJob;
class QShowEvent;
class QListWidgetItem;
class Spinner;
class Action;
class Thread;

class WikipediaLoader : public QObject
{
    Q_OBJECT
public:
    WikipediaLoader();
    virtual ~WikipediaLoader();

public Q_SLOTS:
    void load(const QByteArray &data);

Q_SIGNALS:
    void entry(const QString &prefix, const QString &urlPrefix, const QString &lang, int prefIndex);
    void finished();

private:
    Thread *thread;
};

class WikipediaSettings : public ToggleList
{
    Q_OBJECT

    enum State {
        Initial,
        Loading,
        Loaded
    };

public:
    WikipediaSettings(QWidget *p);
    virtual ~WikipediaSettings();
    
    void load();
    void save();
    void cancel();
    void showEvent(QShowEvent *e);

Q_SIGNALS:
    void load(const QByteArray &data);

private Q_SLOTS:
    void getLangs();
    void parseLangs();
    void addEntry(const QString &prefix, const QString &urlPrefix, const QString &lang, int prefIndex);
    void loaderFinished();

private:
    void parseLangs(const QByteArray &data);
    void showSpinner();
    void hideSpinner();

private:
    State state;
    NetworkJob *job;
    Spinner *spinner;
    Action *reload;
    QMap<int, QListWidgetItem *> prefMap;
    WikipediaLoader *loader;
};

#endif
