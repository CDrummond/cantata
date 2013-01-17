/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This class is a C++/Qt version of input_ffmpeg.c from libebur128
 */

#ifndef _FFMPEG_INPUT_H_
#define _FFMPEG_INPUT_H_

#include "input.h"

class FfmpegInput : public Input
{
    struct Handle;

public:
    static void init();

    FfmpegInput(const QString &fileName);
    ~FfmpegInput();

    operator bool() const { return 0!=handle; }

    size_t totalFrames() const;
    unsigned int channels() const;
    unsigned long sampleRate() const;
    float * buffer() const;
    bool setChannelMap(int *st) const;
    size_t readFrames();

private:
    size_t readOnePacket();

private:
    Handle *handle;
};

#endif
