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

#ifndef NOTELABEL_H
#define NOTELABEL_H

#include <QWidget>
#include "statelabel.h"
#include "support/urllabel.h"

class NoteLabel : public QWidget
{
    Q_OBJECT
public:
    static QString formatText(const QString &text);
    NoteLabel(QWidget *parent=nullptr);
    void setText(const QString &text) { label->setText(formatText(text)); }
    void appendText(const QString &text) { label->setText(label->text()+text); }
    QString text() const { return label->text(); }
    void setProperty(const char *name, const QVariant &value);
    void setOn(bool o) { label->setOn(o); }
private:
    StateLabel *label;
};

class UrlNoteLabel : public QWidget
{
    Q_OBJECT
public:
    UrlNoteLabel(QWidget *parent=nullptr);
    void setText(const QString &text) { label->setText(NoteLabel::formatText(text)); }
    void appendText(const QString &text) { label->setText(label->text()+text); }
    QString text() const { return label->text(); }
    void setProperty(const char *name, const QVariant &value);
Q_SIGNALS:
    void leftClickedUrl();
private:
    UrlLabel *label;
};

class PlainNoteLabel : public StateLabel
{
public:
    PlainNoteLabel(QWidget *parent=nullptr);
    void setText(const QString &text) { StateLabel::setText(NoteLabel::formatText(text)); }
};

class PlainUrlNoteLabel : public UrlLabel
{
public:
    PlainUrlNoteLabel(QWidget *parent=nullptr);
    void setText(const QString &text) { UrlLabel::setText(NoteLabel::formatText(text)); }
};

#endif
