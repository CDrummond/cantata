/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "selectorlabel.h"
#include "support/configuration.h"
#include "support/utils.h"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QFontMetrics>
#include <QTimer>

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
    QFont f(Utils::smallFont(label->font()));
    int size = QFontMetrics(f).height()-2;
    label->setFont(f);
    label->setFixedHeight(size);
    label->setUseArrow(true);
    label->addItem("MPD", "mpd");
    label->addItem("HTTP", "http");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->addItem(new QSpacerItem(0, size, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(stack);
    layout->addWidget(label);
    mpdVol->ensurePolished();
    setFixedSize(mpdVol->width(), mpdVol->height()+(size*2));
    connect(httpVol, SIGNAL(stateChanged()), SLOT(stateChanged()));
    mpdVol->setEnabled(true);
    httpVol->setEnabled(false);
    label->setAlignment(Qt::AlignCenter);
    label->setBold(false);

    stack->setCurrentIndex(0);
    label->setCurrentIndex(0);
    mpdVol->setActive(true);
    httpVol->setActive(false);
    label->setVisible(false);

    connect(label, SIGNAL(activated(int)), SLOT(itemSelected(int)));
    QTimer::singleShot(500, this, SLOT(selectControl()));
}

VolumeControl::~VolumeControl()
{
    Configuration(metaObject()->className()).set("control", label->itemData(stack->currentIndex()));
}

void VolumeControl::selectControl()
{
    QString ctrl = Configuration(metaObject()->className()).get("control", QString());
    if (!ctrl.isEmpty()) {
        for (int i=0; i<label->count(); ++i) {
            if (label->itemData(i) == ctrl) {
                label->setCurrentIndex(i);
                break;
            }
        }
    }
}

void VolumeControl::setColor(const QColor &col)
{
    QColor c=Utils::clampColor(col);
    mpdVol->setColor(col);
    httpVol->setColor(col);
    label->setColor(col);
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
    stack->setCurrentIndex(httpVol->isEnabled() ? 1 : 0);
    mpdVol->setActive(0==stack->currentIndex());
    httpVol->setActive(1==stack->currentIndex());
    label->setVisible(httpVol->isEnabled());
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

#include "moc_volumecontrol.cpp"
