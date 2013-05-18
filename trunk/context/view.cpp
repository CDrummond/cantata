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
 
#include "view.h"
#include "spinner.h"
#include <QLabel>
#include <QTextBrowser>
#include <QImage>
#include <QPixmap>
#include <QBoxLayout>

View::View(QWidget *parent)
    : QWidget(parent)
    , needToUpdate(false)
    , spinner(0)
{
    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    header=new QLabel(this);
    pic=new QLabel(this);
    text=new QTextBrowser(this);

    int spacing=fontMetrics().height();
    layout->setSpacing(spacing);
    layout->setMargin(0);

    header->setWordWrap(true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    text->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    pic->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    layout->addWidget(header);
    layout->addWidget(pic);
    layout->addWidget(text);

    text->setFrameShape(QFrame::NoFrame);
    text->viewport()->setAutoFillBackground(false);
}

void View::clear()
{
    setHeader(stdHeader);
    setPic(QImage());
    text->clear();
}

void View::setHeader(const QString &str)
{
    header->setText("<h2>"+str+"</h2>");
}

void View::setPicSize(const QSize &sz)
{
    picSize=sz;
    if (pic && (picSize.width()<1 || picSize.height()<1)) {
        pic->deleteLater();
        pic=0;
    }
}

void View::setPic(const QImage &img)
{
    if (!pic) {
        return;
    }

    if (img.isNull()) {
        pic->clear();
        pic->setVisible(false);
    } else {
        pic->setPixmap(QPixmap::fromImage(img.scaled(picSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        pic->setVisible(true);
    }
}


void View::showEvent(QShowEvent *e)
{
    if (needToUpdate) {
        update(currentSong, true);
    }
    needToUpdate=false;
    QWidget::showEvent(e);
}

void View::showSpinner()
{
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(this);
    }
    if (!spinner->isActive()) {
        spinner->start();
    }
}

void View::hideSpinner()
{
    if (spinner) {
        spinner->stop();
    }
}

void View::setEditable(bool e)
{
    text->setReadOnly(e);
    text->setFrameShape(e ? QFrame::StyledPanel : QFrame::NoFrame);
    text->viewport()->setAutoFillBackground(e);
}
