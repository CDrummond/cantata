/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#ifdef ENABLE_KDE_SUPPORT
#include <KDialog>
#else
#include <QDialog>
#endif

class ServerSettings;
class PlaybackSettings;
class InterfaceSettings;

#ifdef ENABLE_KDE_SUPPORT
class PreferencesDialog : public KDialog
#else
class PreferencesDialog : public QDialog
#endif
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent);

private:
    void writeSettings();

#ifdef ENABLE_KDE_SUPPORT
    void slotButtonClicked(int button);
#endif

private Q_SLOTS:
#ifndef ENABLE_KDE_SUPPORT
    void buttonPressed(QAbstractButton *button);
#endif

Q_SIGNALS:
    void systemTraySet(bool visible);

private:
    ServerSettings *server;
    PlaybackSettings *playback;
    InterfaceSettings *interface;
};

#endif
