/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CACHESETTINGS_H
#define CACHESETTINGS_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QTreeWidget>

class QPushButton;
class QThread;

class CacheItemCounter : public QObject
{
    Q_OBJECT

public:
    CacheItemCounter(const QString &d, const QStringList &t);
    ~CacheItemCounter();

Q_SIGNALS:
    void count(int num, int space);

public Q_SLOTS:
    void getCount();
    void deleteAll();

private:
    QString dir;
    QStringList types;
    QThread *thread;
};

class CacheItem : public QObject, public QTreeWidgetItem
{
    Q_OBJECT

public:
    CacheItem(const QString &title, const QString &d, const QStringList &t, QTreeWidget *parent);
    ~CacheItem();

    void calculate();
    void clean();
    bool isEmpty() const { return empty; }
    QString name() const { return text(0); }

Q_SIGNALS:
    void getCount();
    void deleteAll();
    void updated();

private Q_SLOTS:
    void update(int itemCount, int space);

private:
    void setStatus(const QString &str=QString());

private:
    CacheItemCounter *counter;
    bool empty;
};

class CacheTree : public QTreeWidget
{
public:
    CacheTree(QWidget *parent);
    ~CacheTree();

    void showEvent(QShowEvent *e);

private:
    bool calculated;
};

class CacheSettings : public QWidget
{
    Q_OBJECT

public:
    CacheSettings(QWidget *parent);
    ~CacheSettings();

private Q_SLOTS:
    void controlButton();
    void deleteAll();

private:
    QTreeWidget *tree;
    QPushButton *button;
};

#endif
