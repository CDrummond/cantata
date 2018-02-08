/*
 * Cantata
 *
 * Copyright (c) 2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "volumecontrol.h"
#include "volumeslider.h"
#include "widgets/selectorlabel.h"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QFontMetrics>

VolumeControl::VolumeControl(QWidget *p)
    : QWidget(p)
{
    stack = new QStackedWidget(this);
    label = new SelectorLabel(this);
    mpdVol = new VolumeSlider(true, stack);
    httpVol = new VolumeSlider(false, stack);
    stack->addWidget(mpdVol);
    stack->addWidget(httpVol);
    label->ensurePolished();
    int size = QFontMetrics(label->font()).height()+4;
    label->setFixedHeight(size);
    label->setUseArrow(true);
    label->addItem(tr("MPD"), "mpd");
    label->addItem(tr("HTTP"), "http");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->addItem(new QSpacerItem(0, size-2, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(stack);
    layout->addWidget(label);
    mpdVol->ensurePolished();
    setFixedSize(mpdVol->width(), (mpdVol->height()*2)+label->height());
    connect(mpdVol, SIGNAL(stateChanged()), SLOT(stateChanged()));
    connect(httpVol, SIGNAL(stateChanged()), SLOT(stateChanged()));
    mpdVol->setEnabled(true);
    httpVol->setEnabled(false);
    label->setAlignment(Qt::AlignCenter);
    stateChanged();
    connect(label, SIGNAL(activated(int)), SLOT(itemSelected(int)));
}

VolumeControl::~VolumeControl()
{

}

void VolumeControl::setColor(const QColor &col)
{
    mpdVol->setColor(col);
    httpVol->setColor(col);
}

void VolumeControl::initActions()
{
    mpdVol->initActions();
    httpVol->initActions();
}

void VolumeControl::setPageStep(int step)
{
    mpdVol->setPageStep(step);
    httpVol->setPageStep(step);
}

void VolumeControl::stateChanged()
{
    stack->setCurrentIndex((sender()==httpVol && httpVol->isEnabled()) ||
                           (sender()==mpdVol && !mpdVol->isEnabled() && httpVol->isEnabled()) ? 1 : 0);
    mpdVol->setActive(0==stack->currentIndex());
    httpVol->setActive(1==stack->currentIndex());
    label->setVisible(mpdVol->isEnabled() && httpVol->isEnabled());
    label->blockSignals(true);
    label->setCurrentIndex(stack->currentIndex());
    label->blockSignals(false);
}

void VolumeControl::itemSelected(int i)
{
    if (i!=stack->currentIndex()) {
        stack->setCurrentIndex(i);
        mpdVol->setActive(0==stack->currentIndex());
        httpVol->setActive(1==stack->currentIndex());
    }
}
