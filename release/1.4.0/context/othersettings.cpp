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

#include "othersettings.h"
#include "gui/settings.h"
#include "support/pathrequester.h"
#include "widgets/playqueueview.h"
#include "support/localize.h"

static const char *constValueProperty="value";

OtherSettings::OtherSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    connect(wikipediaIntroOnly, SIGNAL(toggled(bool)), SLOT(toggleWikiNote()));

    contextBackdrop_none->setProperty(constValueProperty, PlayQueueView::BI_None);
    contextBackdrop_artist->setProperty(constValueProperty, PlayQueueView::BI_Cover);
    contextBackdrop_custom->setProperty(constValueProperty, PlayQueueView::BI_Custom);
    contextBackdropFile->setDirMode(false);
    #ifdef ENABLE_KDE_SUPPORT
    contextBackdropFile->setFilter("image/jpeg image/png");
    #else
    contextBackdropFile->setFilter(i18n("Images (*.png *.jpg)"));
    #endif
    int labelWidth=qMax(fontMetrics().width(QLatin1String("100%")), fontMetrics().width(i18nc("pixels", "10px")));
    contextBackdropOpacityLabel->setFixedWidth(labelWidth);
    contextBackdropBlurLabel->setFixedWidth(labelWidth);
    connect(contextBackdropOpacity, SIGNAL(valueChanged(int)), SLOT(setContextBackdropOpacityLabel()));
    connect(contextBackdropBlur, SIGNAL(valueChanged(int)), SLOT(setContextBackdropBlurLabel()));
    connect(contextBackdrop_none, SIGNAL(toggled(bool)), SLOT(enableContextBackdropOptions()));
    connect(contextBackdrop_artist, SIGNAL(toggled(bool)), SLOT(enableContextBackdropOptions()));
    connect(contextBackdrop_custom, SIGNAL(toggled(bool)), SLOT(enableContextBackdropOptions()));
}

void OtherSettings::load()
{
    wikipediaIntroOnly->setChecked(Settings::self()->wikipediaIntroOnly());
    contextDarkBackground->setChecked(Settings::self()->contextDarkBackground());
    contextAlwaysCollapsed->setChecked(Settings::self()->contextAlwaysCollapsed());

    int bgnd=Settings::self()->contextBackdrop();
    contextBackdrop_none->setChecked(bgnd==contextBackdrop_none->property(constValueProperty).toInt());
    contextBackdrop_artist->setChecked(bgnd==contextBackdrop_artist->property(constValueProperty).toInt());
    contextBackdrop_custom->setChecked(bgnd==contextBackdrop_custom->property(constValueProperty).toInt());
    contextBackdropOpacity->setValue(Settings::self()->contextBackdropOpacity());
    contextBackdropBlur->setValue(Settings::self()->contextBackdropBlur());
    contextBackdropFile->setText(Settings::self()->contextBackdropFile());
    contextSwitchTime->setValue(Settings::self()->contextSwitchTime());

    toggleWikiNote();
    setContextBackdropOpacityLabel();
    setContextBackdropBlurLabel();
    enableContextBackdropOptions();
}

void OtherSettings::save()
{
    Settings::self()->saveWikipediaIntroOnly(wikipediaIntroOnly->isChecked());
    Settings::self()->saveContextDarkBackground(contextDarkBackground->isChecked());
    Settings::self()->saveContextAlwaysCollapsed(contextAlwaysCollapsed->isChecked());

    if (contextBackdrop_none->isChecked()) {
        Settings::self()->saveContextBackdrop(contextBackdrop_none->property(constValueProperty).toInt());
    } else if (contextBackdrop_artist->isChecked()) {
        Settings::self()->saveContextBackdrop(contextBackdrop_artist->property(constValueProperty).toInt());
    } else if (contextBackdrop_custom->isChecked()) {
        Settings::self()->saveContextBackdrop(contextBackdrop_custom->property(constValueProperty).toInt());
    }
    Settings::self()->saveContextBackdropOpacity(contextBackdropOpacity->value());
    Settings::self()->saveContextBackdropBlur(contextBackdropBlur->value());
    Settings::self()->saveContextBackdropFile(contextBackdropFile->text().trimmed());
    Settings::self()->saveContextSwitchTime(contextSwitchTime->value());
}

void OtherSettings::toggleWikiNote()
{
    wikipediaIntroOnlyNote->setOn(!wikipediaIntroOnly->isChecked());
}

void OtherSettings::setContextBackdropOpacityLabel()
{
    contextBackdropOpacityLabel->setText(i18nc("value%", "%1%", contextBackdropOpacity->value()));
}

void OtherSettings::setContextBackdropBlurLabel()
{
    contextBackdropBlurLabel->setText(i18nc("pixels", "%1px", contextBackdropBlur->value()));
}

void OtherSettings::enableContextBackdropOptions()
{
    contextBackdropOpacity->setEnabled(!contextBackdrop_none->isChecked());
    contextBackdropOpacityLabel->setEnabled(contextBackdropOpacity->isEnabled());
    contextBackdropBlur->setEnabled(contextBackdropOpacity->isEnabled());
    contextBackdropBlurLabel->setEnabled(contextBackdropOpacity->isEnabled());
}
