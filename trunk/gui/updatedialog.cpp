/*
 * Cantata
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include <QtGui/QBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QIcon>
#include <QtCore/QTimer>
#include "updatedialog.h"
#include "mainwindow.h"
#include "settings.h"
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KLocale>
#endif

UpdateDialog::UpdateDialog(QWidget *parent)
#ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
#else
    : QDialog(parent)
#endif
    , done(false)
{
#ifdef ENABLE_KDE_SUPPORT
    QWidget *wid = new QWidget(this);
#else
    QWidget *wid = this;
#endif
    QBoxLayout *layout=new QBoxLayout(QBoxLayout::LeftToRight, wid);

    QLabel *icon = new QLabel(wid);
    icon->setPixmap(QIcon::fromTheme("audio-x-generic").pixmap(32, 32));
    label = new QLabel(wid);
    layout->addWidget(icon);
    layout->addWidget(label);

#ifdef ENABLE_KDE_SUPPORT
    setMainWidget(wid);
    setButtons(KDialog::Close);
    setCaption(i18n("Updating"));
#else
    setWindowTitle(tr("Updating"));
#endif
    timer=new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(display()));
}

void UpdateDialog::show()
{
#ifdef ENABLE_KDE_SUPPORT
    label->setText(i18n("<b>Updating backend database</b><br><i>Please wait...</i>"));
#else
    label->setText(tr("<b>Updating backend database</b><br><i>Please wait...</i>"));
#endif
    timer->start(1000);
}

void UpdateDialog::hide()
{
    setVisible(false);
}

void UpdateDialog::display()
{
    if(!done) {
        setVisible(true);
        timer->stop();
    }
}

void UpdateDialog::complete()
{
    done=true;
#ifdef ENABLE_KDE_SUPPORT
    label->setText(i18n("<b>Finished</b><br><i>Database updated.</i>"));
#else
    label->setText(tr("<b>Finished</b><br><i>Database updated.</i>"));
#endif
}

