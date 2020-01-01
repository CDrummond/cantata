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

#ifndef TEXTBROWSER_H
#define TEXTBROWSER_H

#include <QTextBrowser>

class QTimer;

class TextBrowser : public QTextBrowser
{
    Q_OBJECT
public:
    TextBrowser(QWidget *p);
    QVariant loadResource(int type, const QUrl &name) override;

    void setPicSize(const QSize &p) { pSize=p; }
    QSize picSize() const { return pSize; }
    void setFullWidthImage(bool s);
    bool fullWidthImage() const { return fullWidthImg; }
    void setPal(const QPalette &pal);
    void setHtml(const QString &s) { haveImg=false; QTextBrowser::setHtml(s); }
    void setText(const QString &s) { haveImg=false; QTextBrowser::setText(s); }

    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private Q_SLOTS:
    void refreshHtml();

private:
    void handleSizeChange();
    QSize imageSize() const;

private:
    QTimer *timer;
    int lastImageSize;
    bool fullWidthImg;
    bool haveImg;
    QSize pSize;
};

#endif
