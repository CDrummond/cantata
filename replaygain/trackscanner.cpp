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

#include "trackscanner.h"
#include "config.h"
#ifdef MPG123_FOUND
#include "mpg123input.h"
#endif
#ifdef FFMPEG_FOUND
#include "ffmpeginput.h"
#endif

#define RG_REFERENCE_LEVEL -18.0

double TrackScanner::clamp(double v)
{
    return v < -51.0 ? -51.0
                      : v > 51.0
                        ? 51.0
                        : v;
}

double TrackScanner::reference(double v)
{
    return clamp(RG_REFERENCE_LEVEL-v);
}

TrackScanner::Data TrackScanner::global(const QList<TrackScanner *> &scanners)
{
    Data d;
    if (scanners.count()<1) {
        return d;
    } else if(scanners.count()==1) {
        return scanners.first()->results();
    } else {
        ebur128_state **states = new ebur128_state * [scanners.count()];
        for (int i=0; i<scanners.count(); ++i) {
            TrackScanner *s=scanners.at(i);
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

#ifdef EBUR128_USE_SPEEX_RESAMPLER
static int constEbur128Mode=EBUR128_MODE_M|EBUR128_MODE_I|EBUR128_MODE_TRUE_PEAK);
#else
static int constEbur128Mode=EBUR128_MODE_M|EBUR128_MODE_I|EBUR128_MODE_SAMPLE_PEAK;
#endif

void TrackScanner::init()
{
    static bool doneInit=false;
    if (doneInit) {
        return;
    }
    doneInit=true;
    #ifdef MPG123_FOUND
    Mpg123Input::init();
    #endif
    #ifdef FFMPEG_FOUND
    FfmpegInput::init();
    #endif
    ebur128_init_static(constEbur128Mode);
}

TrackScanner::TrackScanner(int i)
    : idx(i)
    , state(0)
    , input(0)
{
}

TrackScanner::~TrackScanner()
{
    delete input;
    if (state) {
        ebur128_destroy(&state);
        state=0;
    }
}

void TrackScanner::setFile(const QString &fileName)
{
    file=fileName;
}

void TrackScanner::run()
{
    bool ffmpegIsFloat=false;
    #ifdef FFMPEG_FOUND
    FfmpegInput *ffmpeg=new FfmpegInput(file);
    if (*ffmpeg) {
        input=ffmpeg;
        ffmpegIsFloat=ffmpeg->isFloatCodec();
    } else {
        delete ffmpeg;
        ffmpeg=0;
    }
    #endif

    #if MPG123_FOUND
    if (file.endsWith(".mp3", Qt::CaseInsensitive) && (!input || !ffmpegIsFloat)) {
        Mpg123Input *mpg123=new Mpg123Input(file);
        if (*mpg123) {
            input=mpg123;
            #ifdef FFMPEG_FOUND
            if (ffmpeg) {
                delete ffmpeg;
            }
            #endif
        } else {
            delete mpg123;
        }
    }
    #endif

    if (!input) {
        setFinishedStatus(false);
        return;
    }

    state=ebur128_init(input->channels(), input->sampleRate(), constEbur128Mode);

    int *channelMap=new int [state->channels];
    if (input->setChannelMap(channelMap)) {
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
    input->allocateBuffer();
    while ((numFramesRead = input->readFrames())) {
        if (abortRequested) {
            setFinishedStatus(false);
            return;
        }
        totalRead+=numFramesRead;
        emit progress((int)((totalRead*100.0/input->totalFrames())+0.5));
        if (ebur128_add_frames_float(state, input->buffer(), numFramesRead)) {
            setFinishedStatus(false);
            return;
        }
    }

    if (abortRequested) {
        setFinishedStatus(false);
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
    setFinishedStatus(true);
}

void TrackScanner::setFinishedStatus(bool f)
{
    delete input;
    input=0;
    setFinished(f);
}
