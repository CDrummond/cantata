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

#ifndef CONTEXT_ENGINE_H
#define CONTEXT_ENGINE_H

#include <QObject>
#include <QStringList>

class QNetworkReply;

class ContextEngine : public QObject
{
    Q_OBJECT
    
public:
    enum Mode {
        Artist,
        Album
    };
    
    static void setPreferedLangs(const QStringList &l);
    static const QStringList & getPreferedLangs() { return preferredLangs; }
    static ContextEngine * create(QObject *parent);

    ContextEngine(QObject *p);
    virtual ~ContextEngine();
    
    virtual const QStringList & getLangs()=0;
    virtual QString getPrefix(const QString &key)=0;

    void cancel();

public Q_SLOTS:
    virtual void search(const QStringList &query, Mode mode)=0;
    
Q_SIGNALS:
    void searchResult(const QString &html, const QString &lang);

protected:
    QNetworkReply * getReply(QObject *obj);

protected:
    static QStringList preferredLangs;
    QNetworkReply *job;
};

#endif

