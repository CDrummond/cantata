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

#ifndef CACHESETTINGS_H
#define CACHESETTINGS_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include "support/squeezedtextlabel.h"

class QPushButton;
class Thread;

class SpaceLabel : public SqueezedTextLabel
{
    Q_OBJECT

public:
    SpaceLabel(QWidget *p);

    void update(int space);
    void update(const QString &text);
};

class CacheItemCounter : public QObject
{
    Q_OBJECT

public:
    CacheItemCounter(const QString &name, const QString &d, const QStringList &t);
    ~CacheItemCounter() override;

Q_SIGNALS:
    void count(int num, quint64 space);

public Q_SLOTS:
    void getCount();
    void deleteAll();

private:
    QString dir;
    QStringList types;
    Thread *thread;
};

class CacheItem : public QObject, public QTreeWidgetItem
{
    Q_OBJECT

public:
    enum Type {
        Type_Covers,
        Type_ScaledCovers,
        Type_Other
    };

public:
    CacheItem(const QString &title, const QString &d, const QStringList &t, QTreeWidget *parent, Type ty=Type_Other);
    ~CacheItem() override;

    void calculate();
    void clean();
    bool isEmpty() const { return empty; }
    QString name() const { return text(0); }
    int spaceUsed() const { return usedSpace; }

Q_SIGNALS:
    void getCount();
    void deleteAll();
    void updated();

private Q_SLOTS:
    void update(int itemCount, quint64 space);

private:
    void setStatus(const QString &str=QString());

private:
    CacheItemCounter *counter;
    bool empty;
    quint64 usedSpace;
    Type type;
};

class CacheTree : public QTreeWidget
{
    Q_OBJECT

public:
    CacheTree(QWidget *parent);
    ~CacheTree() override;

    void showEvent(QShowEvent *e) override;

private:
    bool calculated;
};

class CacheSettings : public QWidget
{
    Q_OBJECT

public:
    CacheSettings(QWidget *parent);
    ~CacheSettings() override;

private Q_SLOTS:
    void controlButton();
    void deleteAll();
    void updateSpace();

private:
    QTreeWidget *tree;
    SpaceLabel *spaceLabel;
    QPushButton *button;
};

#endif
