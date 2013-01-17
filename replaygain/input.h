/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This class is a C++/Qt version of input_mpg123.c from libebur128
 */

#ifndef _INPUT_H_
#define _INPUT_H_

class QString;

class Input
{
public:
    Input() {
    }
    virtual ~Input() {
    }

    virtual operator bool() const=0;

    virtual size_t totalFrames() const=0;
    virtual unsigned int channels() const=0;
    virtual unsigned long sampleRate() const=0;
    virtual bool allocateBuffer() {
        return true;
    }
    virtual float * buffer() const=0;
    virtual bool setChannelMap(int *st) const=0;
    virtual size_t readFrames()=0;
};

#endif
