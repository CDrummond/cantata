/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "songlabel.h"
#include "localize.h"
#include "song.h"
#include <QFontMetrics>

QFontMetrics bm(QFont f)
{
    f.setBold(true);
    return QFontMetrics(f);
}

SongLabel::SongLabel(QWidget *p)
    : QLabel(p)
    , boldMetrics(bm(font()))
{
    bool rtl=Qt::RightToLeft==layoutDirection();

    elideMode=rtl ? Qt::ElideLeft : Qt::ElideRight;
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setTextInteractionFlags(Qt::NoTextInteraction);
    update(Song());
    ensurePolished();
    setFixedHeight(height());
}

void SongLabel::update(const Song &song)
{
    if (song.isEmpty()) {
        plainText=text=QString();
    } else {
        plainText=song.describe(false, false);
        text=song.describe(true, false);
    }

    open.clear();
    close.clear();
    elideText();
}

void SongLabel::calcTagPositions()
{
    int pos=0;
    int adjust=0;
    while (-1!=(pos=text.indexOf("<b>", pos))) {
        open.append(pos-adjust);
        adjust+=3;
        pos+=3;
        pos=text.indexOf("</b>", pos);
        if (pos>adjust) {
            close.append(pos-adjust);
            adjust+=4;
            pos+=4;
        } else {
            break;
        }
    }
}

void SongLabel::elideText()
{
    // Check space size assuming all text is bold (most is)
    int labelWidth = size().width();
    int lineWidth = boldMetrics.width(plainText);

    if (lineWidth > labelWidth) {
        if (open.isEmpty()) {
            calcTagPositions();
        }

        // Elide the plain text
        QString elided=boldMetrics.elidedText(plainText, elideMode, labelWidth);
        // Now add tags back into text
        if (open.size()==close.size()) {
            int adjust=0;
            for (int i=0; i<open.size(); ++i) {
                int o=open.at(i)+adjust;
                if (o<elided.length()-3) {
                    if (0==o) {
                        elided=("<b>")+elided;
                    } else {
                        elided=elided.left(o)+("<b>")+elided.mid(o);
                    }
                    adjust+=3;
                    int c=close.at(i)+adjust;
                    if (c<elided.length()-3) {
                        elided=elided.left(c)+("</b>")+elided.mid(c);
                        adjust+=4;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        QLabel::setText(elided);
        setToolTip(plainText);
    } else {
        QLabel::setText(text);
        setToolTip(QString());
    }
}
