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

#ifndef WIKIPEDIA_ENGINE_H
#define WIKIPEDIA_ENGINE_H

#include "contextengine.h"
#include <QStringList>

class WikipediaEngine : public ContextEngine
{
    Q_OBJECT
    
public:
    WikipediaEngine(QObject *p);

    static void setPreferedLangs(const QStringList &l);
    static const QStringList & getPreferedLangs() { return preferredLangs; }

    const QStringList & getLangs() { return getPreferedLangs(); }

public Q_SLOTS:
    void search(const QStringList &query, Mode mode);
    
private:
    void requestTitles(const QStringList &query, Mode mode, const QString &lang);
    void getPage(const QStringList &query, Mode mode, const QString &lang);

private Q_SLOTS:
    void parseTitles();
    void parsePage();

private:
    static QStringList preferredLangs;
    QStringList titles;
};

#endif
