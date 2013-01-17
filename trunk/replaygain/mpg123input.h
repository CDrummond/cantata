/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This class is a C++/Qt version of input_mpg123.c from libebur128
 */

#ifndef _MPG123_INPUT_H_
#define _MPG123_INPUT_H_

#include "input.h"

class Mpg123Input : public Input
{
    struct Handle;

public:
    static void init();

    Mpg123Input(const QString &fileName);
    ~Mpg123Input();

    operator bool() const { return 0!=handle; }

    size_t totalFrames() const;
    unsigned int channels() const;
    unsigned long sampleRate() const;
    bool allocateBuffer();
    float * buffer() const;
    bool setChannelMap(int *st) const;
    size_t readFrames();

private:
    Handle *handle;
};

#endif
