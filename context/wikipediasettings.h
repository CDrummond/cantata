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

#include "contextengine.h"
#include "ui_wikipediasettings.h"

class QNetworkReply;
class QShowEvent;
class Spinner;

class WikipediaSettings : public QWidget, private Ui::WikipediaSettings
{
    Q_OBJECT
    
public:
    WikipediaSettings(QWidget *p);
    
    void load();
    void save();
    void cancel();
    void showEvent(QShowEvent *e);

private Q_SLOTS:
    void getLangs();
    void parseLangs();
    void moveUp();
    void moveDown();
    void move(int d);
    void add();
    void remove();
    void currentLangChanged(QListWidgetItem *item);
    void currentPreferredLangChanged(QListWidgetItem *item);

private:
    void parseLangs(const QByteArray &data);

private:
    bool loaded;
    QNetworkReply *job;
    Spinner *spinner;
};

#endif
