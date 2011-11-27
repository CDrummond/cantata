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

#ifndef MPDRESULT_H
#define MPDRESULT_H

#include <QtCore/QString>

class MPDResult
{
public:
    enum State {
        State_OK, /* There's no error, everything is fine */
        State_Error
    };

    MPDResult(State state=State_OK, const QString &error=QString()) : m_state(state), m_error(error) { }

    operator bool() const { return State_OK==m_state; }
    State state() const { return m_state; }
    QString errorString() const { return m_error; }
    bool isError() const { return State_Error==m_state; }

private:
    State m_state;
    QString m_error;
};

#endif
