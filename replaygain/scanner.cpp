/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "scanner.h"
#include "ffmpeginput.h"

#define RG_REFERENCE_LEVEL -18.0

double Scanner::clamp(double v)
{
    return v < -51.0 ? -51.0
                      : v > 51.0
                        ? 51.0
                        : v;
}

double Scanner::reference(double v)
{
    return clamp(RG_REFERENCE_LEVEL-v);
}

Scanner::Data Scanner::global(const QList<Scanner *> &scanners)
{
    Data d;
    if (scanners.count()<1) {
        return d;
    } else if(scanners.count()==1) {
        return scanners.first()->results();
    } else {
        ebur128_state **states = new ebur128_state * [scanners.count()];
        for (int i=0; i<scanners.count(); ++i) {
            Scanner *s=scanners.at(i);
            states[i]=s->state;
            if (s->results().peak>d.peak) {
                d.peak=s->results().peak;
            }
            if (s->results().truePeak>d.truePeak) {
                d.truePeak=s->results().truePeak;
            }
        }

        double l=0.0;
        int rv=ebur128_loudness_global_multiple(states, scanners.count(), &l);
        delete [] states;
        d.loudness=0==rv ? l : 0.0;
        return d;
    }
}

Scanner::Scanner(QObject *p)
    : ThreadWeaver::Job(p)
    , state(0)
    , abortRequested(false)
{
    FfmpegInput::init();
}

Scanner::~Scanner()
{
}

void Scanner::setFile(const QString &fileName)
{
    file=fileName;
}

void Scanner::run()
{
    FfmpegInput input(file);

    if (!input) {
        setFinished(false);
        return;
    }
    state=ebur128_init(input.channels(), input.sampleRate(), EBUR128_MODE_M|EBUR128_MODE_I|EBUR128_MODE_SAMPLE_PEAK/*|EBUR128_MODE_TRUE_PEAK|EBUR128_MODE_HISTOGRAM*/);
    int *channelMap=new int [state->channels];
    if (input.setChannelMap(channelMap)) {
        for (unsigned int i = 0; i < state->channels; ++i) {
            ebur128_set_channel(state, i, channelMap[i]);
        }
    }

    delete [] channelMap;

    //if (1==state->channels && opts->force_dual_mono) {
    //    ebur128_set_channel(state, 0, EBUR128_DUAL_MONO);
    //}

    size_t numFramesRead=0;
    size_t totalRead=0;
    while ((numFramesRead = input.readFrames())) {
        if (abortRequested) {
            setFinished(false);
            return;
        }
        totalRead+=numFramesRead;
        emit progress(this, (int)((totalRead*100.0/input.totalFrames())+0.5));
        if (ebur128_add_frames_float(state, input.buffer(), numFramesRead)) {
            setFinished(false);
            return;
        }
    }

    if (abortRequested) {
        setFinished(false);
        return;
    }

    ebur128_loudness_global(state, &data.loudness);
//     if (opts->lra) {
//         result = ebur128_loudness_range(ebur, &lra);
//         if (result) abort();
//     }

    if ((state->mode & EBUR128_MODE_SAMPLE_PEAK) == EBUR128_MODE_SAMPLE_PEAK) {
        for (unsigned i = 0; i < state->channels; ++i) {
            double sp;
            ebur128_sample_peak(state, i, &sp);
            if (sp > data.peak) {
                data.peak = sp;
            }
        }
    }
#ifdef EBUR128_USE_SPEEX_RESAMPLER
    if ((state->mode & EBUR128_MODE_TRUE_PEAK) == EBUR128_MODE_TRUE_PEAK) {
        for (unsigned i = 0; i < state->channels; ++i) {
            double tp;
            ebur128_true_peak(state, i, &tp);
            if (tp > data.truePeak) {
                data.truePeak = tp;
            }
        }
    }
#endif
    setFinished(true);
}
