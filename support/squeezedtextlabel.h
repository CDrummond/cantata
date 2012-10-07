/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef SQUEEZEDTEXTLABEL_H
#define SQUEEZEDTEXTLABEL_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KSqueezedTextLabel>
class SqueezedTextLabel : public KSqueezedTextLabel
{
public:
    SqueezedTextLabel(QWidget *p)
        : KSqueezedTextLabel(p) {
        bool rtl=Qt::RightToLeft==layoutDirection();
        setTextElideMode(rtl ? Qt::ElideLeft : Qt::ElideRight);
        setAlignment((rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    }
};
#else
#include <QtGui/QLabel>
#include <QtGui/QFontMetrics>
class QResizeEvent;

class SqueezedTextLabel : public QLabel
{
public:
    SqueezedTextLabel(QWidget *p)
        : QLabel(p) {
        bool rtl=Qt::RightToLeft==layoutDirection();
        elideMode=rtl ? Qt::ElideLeft : Qt::ElideRight;
        setAlignment((rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    void setText(const QString &text) {
        fullText=text;
        elideText();
    }

protected:
    QSize minimumSizeHint() const {
        QSize sh = QLabel::minimumSizeHint();
        sh.setWidth(-1);
        return sh;
    }

    QSize sizeHint() const {
        return QSize(fontMetrics().width(fullText), QLabel::sizeHint().height());
    }

    void resizeEvent(QResizeEvent *) {
        elideText();
    }

private:
    void elideText() {
        QFontMetrics fm(fontMetrics());
        int labelWidth = size().width();
        int lineWidth = fm.width(fullText);

        if (lineWidth > labelWidth) {
            QLabel::setText(fm.elidedText(fullText, elideMode, labelWidth));
            setToolTip(fullText);
        } else {
            QLabel::setText(fullText);
            setToolTip(QString());
        }
    }

private:
    QString fullText;
    Qt::TextElideMode elideMode;
};
#endif

class PaddedSqueezedTextLabel : public SqueezedTextLabel
{
public:
    PaddedSqueezedTextLabel(QWidget *p)
        : SqueezedTextLabel(p) {
    }

protected:
    QSize sizeHint() const {
        QSize sh(SqueezedTextLabel::sizeHint());;
        return QSize(sh.width(), qMax((int)((fontMetrics().height()*1.1)+0.5), sh.height()));
    }
};

#endif // SQUEEZEDTEXTLABEL_H
