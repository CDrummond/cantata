/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "notelabel.h"
#include "support/utils.h"
#include <QVBoxLayout>
#include <QFont>

static void init(QLabel *label)
{
    static const int constMinFontSize=9;

    label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    if (label->font().pointSize()>constMinFontSize) {
        label->setFont(Utils::smallFont(label->font()));
    }
}

static QLabel * init(QWidget *p, bool url)
{
    int layoutSpacing=Utils::layoutSpacing(p);
    int spacing=p->fontMetrics().height()*(Utils::limitedHeight(p) ? 0.25 : 1.0)-layoutSpacing;
    if (spacing<layoutSpacing) {
        spacing=layoutSpacing;
    }

    QVBoxLayout *l=new QVBoxLayout(p);
    l->setMargin(0);
    l->setSpacing(0);
    QLabel *label;
    if (url) {
        label=new UrlLabel(p);
    } else {
        label=new StateLabel(p);
    }
    init(label);
    l->addItem(new QSpacerItem(2, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed));
    l->addWidget(label);
    return label;
}

void NoteLabel::setText(QLabel *l, const QString &text)
{
    l->setText(tr("<i><b>NOTE:</b> %1</i>").arg(text));
}

NoteLabel::NoteLabel(QWidget *parent)
    : QWidget(parent)
{
    label=static_cast<StateLabel *>(init(this, false));
}

UrlNoteLabel::UrlNoteLabel(QWidget *parent)
    : QWidget(parent)
{
    label=static_cast<UrlLabel *>(init(this, true));
    connect(label, SIGNAL(leftClickedUrl()), this, SIGNAL(leftClickedUrl()));
}

PlainNoteLabel::PlainNoteLabel(QWidget *parent)
    : StateLabel(parent)
{
    init(this);
}

PlainUrlNoteLabel::PlainUrlNoteLabel(QWidget *parent)
    : UrlLabel(parent)
{
    init(this);
}
