/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This class is a C++/Qt version of input_ffmpeg.c from libebur128
 */

/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 1
#ifdef __cplusplus
extern "C" {
#endif
#include <mpg123.h>
#ifdef __cplusplus
}
#endif
#include <QFile>
#include <QString>
#include <QByteArray>
#include "ebur128.h"
#include "mpg123input.h"

struct Mpg123Input::Handle {
    Handle()
        : mpg123(0)
        , rate(0)
        , channels(0)
        , encoding(0)
        , buffer(0) {
    }
    ~Handle() {
        if (buffer) {
            free(buffer);
        }
    }
    mpg123_handle *mpg123;
    long rate;
    int channels, encoding;
    float *buffer;
};

void Mpg123Input::init()
{
    static int i=false;
    if (!i) {
        mpg123_init();
    }
}

Mpg123Input::Mpg123Input(const QString &fileName)
{
    handle=new Handle;

    QByteArray fName=QFile::encodeName(fileName);
    bool ok=false;
    int result;
    handle->mpg123 = mpg123_new(NULL, &result);
    if (handle->mpg123 &&
        MPG123_OK==mpg123_open(handle->mpg123, fName.constData()) &&
        MPG123_OK==mpg123_getformat(handle->mpg123, &handle->rate, &handle->channels, &handle->encoding) &&
        MPG123_OK==mpg123_format_none(handle->mpg123) &&
        MPG123_OK==mpg123_format(handle->mpg123, handle->rate, handle->channels, MPG123_ENC_FLOAT_32)) {

        mpg123_close(handle->mpg123);
        if (MPG123_OK==mpg123_open(handle->mpg123, fName.constData()) &&
            MPG123_OK==mpg123_getformat(handle->mpg123, &handle->rate, &handle->channels, &handle->encoding)) {
            ok=true;
        }
    }

    if (!ok) {
        if (handle->mpg123) {
            mpg123_close(handle->mpg123);
            mpg123_delete(handle->mpg123);
            handle->mpg123 = NULL;
        }
        delete handle;
        handle = 0;
    }
}

Mpg123Input::~Mpg123Input()
{
    if (handle) {
        if (handle->mpg123) {
            mpg123_close(handle->mpg123);
            mpg123_delete(handle->mpg123);
            handle->mpg123 = NULL;
        }
        delete handle;
        handle = 0;
    }
}

size_t Mpg123Input::totalFrames() const
{
    if (!handle) {
        return 0;
    }

    off_t length = mpg123_length(handle->mpg123);
    return MPG123_ERR==length ? 0 : (size_t) length;
}

unsigned int Mpg123Input::channels() const
{
    return handle ? (unsigned int)handle->channels : 0;
}

unsigned long Mpg123Input::sampleRate() const
{
    return handle ? (unsigned long)handle->rate : 0;
}

bool Mpg123Input::allocateBuffer()
{
    if (!handle) {
        return false;
    }

    handle->buffer = (float*) malloc((size_t) handle->rate *
                                     (size_t) handle->channels *
                                      sizeof(float));
    return handle->buffer;
}

float * Mpg123Input::buffer() const
{
    return handle ? handle->buffer : 0;
}

bool Mpg123Input::setChannelMap(int *st) const
{
    Q_UNUSED(st);
    return false;
}

size_t Mpg123Input::readFrames()
{
    if (!handle) {
        return 0;
    }

    size_t numberRead;
    int result = mpg123_read(handle->mpg123, (unsigned char*) handle->buffer,
                            (size_t) handle->rate *
                            (size_t) handle->channels * sizeof(float),
                            &numberRead);
    if (MPG123_OK!=result && MPG123_DONE!=result) {
        return 0;
    }
    numberRead /= (size_t) handle->channels * sizeof(float);
    return numberRead;
}
