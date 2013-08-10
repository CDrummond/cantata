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

#ifndef _SCANNER_H_
#define _SCANNER_H_

#include "jobcontroller.h"
#include "ebur128.h"

class Input;

class Scanner : public Job
{
    Q_OBJECT

public:
    struct Data
    {
        Data()
            : loudness(0.0)
            , peak(0.0)
            , truePeak(0.0) {
        }
        double peakValue() const { return truePeak>peak ? truePeak : peak; }
        double loudness;
        double peak;
        double truePeak;
    };

    static Data global(const QList<Scanner *> &scanners);
    static double clamp(double v);
    static double reference(double v);

    static void init();

    Scanner(int i);
    ~Scanner();

    void setFile(const QString &fileName);
    const Data & results() const { return data; }
    int index() const { return idx; }

private:
    void run();
    void setFinishedStatus(bool f);

private:
    int idx;
    ebur128_state *state;
    Data data;
    QString file;
    Input *input;
};

#endif
