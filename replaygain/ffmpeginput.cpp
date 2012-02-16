/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This class is a C++/Qt version of input_ffmpeg.c from libebur128
 */

/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 1
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS
#ifdef _STDINT_H
#undef _STDINT_H
#endif
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif
#include <QtCore/QMutex>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include "ebur128.h"
#include "ffmpeginput.h"

static QMutex mutex;

#define BUFFER_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE)

struct Buffer {
  uint8_t audioBuffer[BUFFER_SIZE];
} __attribute__ ((aligned (8)));

struct FfmpegInput::Handle {
    Handle()
        : formatContext(0)
        , codecContext(0)
        , codec(0)
        , needNewFrame(false)
        , audioStream(0)
        , oldData(0)
        , currentBytes(0) {
        av_init_packet(&packet);
    }
  AVFormatContext* formatContext;
  AVCodecContext* codecContext;
  AVCodec* codec;
  AVPacket packet;
  bool needNewFrame;
  int audioStream;
  uint8_t* oldData;
  struct Buffer audioBuffer;
  float buffer[BUFFER_SIZE / 2 + 1];
  QList<QByteArray> bufferList;
  size_t currentBytes;
};

void FfmpegInput::init()
{
    static int i=false;
    if (!i) {
        av_register_all();
        av_log_set_level(AV_LOG_ERROR);
        i=true;
    }
}

FfmpegInput::FfmpegInput(const QString &fileName)
{
    mutex.lock();
    handle=new Handle;

    bool ok=true;
    #if LIBAVFORMAT_VERSION_MAJOR >= 54 || \
        (LIBAVFORMAT_VERSION_MAJOR == 53 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 2, 0)) || \
        (LIBAVFORMAT_VERSION_MAJOR == 52 && LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(52, 110, 0))
    if (avformat_open_input(&handle->formatContext, QFile::encodeName(fileName).constData(), NULL, NULL) != 0)
    #else
    if (av_open_input_file(&handle->formatContext, QFile::encodeName(fileName).constData(), NULL, 0, NULL) != 0)
    #endif
        {
        mutex.unlock();
        delete handle;
        handle=0;
        return;
    }
    if (ok && av_find_stream_info(handle->formatContext) < 0) {
        ok=false;
    }

    // Find the first audio stream
    if (ok) {
        handle->audioStream = -1;
        for (size_t j = 0; j < handle->formatContext->nb_streams; ++j) {
            if (handle->formatContext->streams[j]->codec->codec_type ==
                #if LIBAVCODEC_VERSION_MAJOR >= 53 || \
                    (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 64, 0))
                    AVMEDIA_TYPE_AUDIO
                    #else
                    CODEC_TYPE_AUDIO
                    #endif
                ) {
                    handle->audioStream = (int) j;
                    break;
            }
        }
        if (-1==handle->audioStream) {
            ok=false;
        }
    }

    if (ok) {
        // Get a pointer to the codec context for the audio stream
        handle->codecContext = handle->formatContext->streams[handle->audioStream]->codec;

        // request float output if supported
        #if LIBAVCODEC_VERSION_MAJOR >= 54 || \
            (LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 4, 0))
        handle->codecContext->request_sample_fmt = SAMPLE_FMT_FLT;
        #endif
        // Find the decoder for the video stream, and open codec...
        handle->codec = avcodec_find_decoder(handle->codecContext->codec_id);
        if (!handle->codec ||
            #if LIBAVCODEC_VERSION_MAJOR >= 53
            avcodec_open2(handle->codecContext, handle->codec, NULL) < 0)
            #else
            avcodec_open(handle->codecContext, handle->codec) < 0)
            #endif
            {
            ok=false;
        }
    }

    if (ok) {
        mutex.unlock();
        handle->needNewFrame = true;
        handle->oldData = NULL;
    } else {
        av_close_input_file(handle->formatContext);
        mutex.unlock();
        delete handle;
        handle=0;
    }
}

FfmpegInput::~FfmpegInput()
{
    if (handle) {
        mutex.lock();
        avcodec_close(handle->codecContext);
        av_close_input_file(handle->formatContext);
        mutex.unlock();
        delete handle;
        handle=0;
    }
}

size_t FfmpegInput::totalFrames() const
{
    if (!handle) {
        return 0;
    }

    double tmp = (double) handle->formatContext->streams[handle->audioStream]->duration
                * (double) handle->formatContext->streams[handle->audioStream]->time_base.num
                / (double) handle->formatContext->streams[handle->audioStream]->time_base.den
                * (double) handle->codecContext->sample_rate;

    return tmp<=0.0 ? 0 : (size_t) (tmp + 0.5);
}

unsigned int FfmpegInput::channels() const
{
    return handle ? handle->codecContext->channels : 0;
}

unsigned long FfmpegInput::sampleRate() const
{
    return handle ? handle->codecContext->sample_rate : 0;
}

float * FfmpegInput::buffer() const
{
    return handle ? handle->buffer : 0;
}

bool FfmpegInput::setChannelMap(int *st)
{
    if (handle && handle->codecContext->channel_layout) {
        unsigned int mapIndex = 0;
        int bitCounter = 0;
        while (mapIndex < (unsigned) handle->codecContext->channels) {
            if (handle->codecContext->channel_layout & (1 << bitCounter)) {
                switch (1 << bitCounter) {
                case CH_FRONT_LEFT:
                    st[mapIndex] = EBUR128_LEFT;
                    break;
                case CH_FRONT_RIGHT:
                    st[mapIndex] = EBUR128_RIGHT;
                    break;
                case CH_FRONT_CENTER:
                    st[mapIndex] = EBUR128_CENTER;
                    break;
                case CH_BACK_LEFT:
                    st[mapIndex] = EBUR128_LEFT_SURROUND;
                    break;
                case CH_BACK_RIGHT:
                    st[mapIndex] = EBUR128_RIGHT_SURROUND;
                    break;
                default:
                    st[mapIndex] = EBUR128_UNUSED;
                    break;
                }
                ++mapIndex;
            }
            ++bitCounter;
        }
        return true;
    }

    return false;
}

size_t FfmpegInput::readFrames()
{
    if (!handle || !channels()) {
        return 0;
    }

    size_t bufferPosition = 0, numberRead;

    while (handle->currentBytes < BUFFER_SIZE) {
        numberRead = readOnePacket();
        if (!numberRead) {
            break;
        }
        size_t bufferSize=numberRead * channels() * sizeof(float);
        handle->bufferList.append(QByteArray((const char *)(handle->buffer), bufferSize));
        handle->currentBytes += bufferSize;
    }

    while (handle->bufferList.count() && handle->bufferList.first().size() + bufferPosition <= BUFFER_SIZE) {
        QByteArray b=handle->bufferList.takeAt(0);
        memcpy((char *) handle->buffer + bufferPosition, b.constData(), b.size());
        bufferPosition += b.size();
        handle->currentBytes -= b.size();
    }

    return bufferPosition / sizeof(float) / channels();
}

size_t FfmpegInput::readOnePacket()
{
    if (!handle) {
        return 0;
    }

    for (;;) {
        if (handle->needNewFrame && av_read_frame(handle->formatContext, &handle->packet) < 0) {
            return 0;
        }
        handle->needNewFrame = FALSE;
        if (handle->packet.stream_index == handle->audioStream) {

            if (!handle->oldData) {
                handle->oldData = handle->packet.data;
            }
            while (handle->packet.size > 0) {
                int dataSize = sizeof(handle->audioBuffer);
                #if LIBAVCODEC_VERSION_MAJOR >= 53 || \
                    (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 23, 0))
                int len = avcodec_decode_audio3(handle->codecContext, (int16_t*) &handle->audioBuffer, &dataSize, &handle->packet);
                #else
                int len = avcodec_decode_audio2(handle->codecContext, (int16_t*) &handle->audioBuffer, &dataSize, handle->packet.data, handle->packet.size);
                #endif
                if (len < 0) {
                    handle->packet.size = 0;
                    break;
                }
                handle->packet.data += len;
                handle->packet.size -= len;
                if (!dataSize) {
                    continue;
                }
                if (handle->packet.size < 0) {
    //                 fprintf(stderr, "Error in decoder!\n");
                    return 0;
                }
                size_t numberRead, i;
                switch (handle->codecContext->sample_fmt) {
                case SAMPLE_FMT_U8:
    //                 fprintf(stderr, "8 bit audio not supported by libebur128!\n");
                    return 0;
                case SAMPLE_FMT_S16: {
                    int16_t *dataShort =  (int16_t*) &handle->audioBuffer;
                    numberRead = (size_t) dataSize / sizeof(int16_t) / (size_t) handle->codecContext->channels;
                    for (i = 0; i < (size_t) dataSize / sizeof(int16_t); ++i) {
                        handle->buffer[i] = ((float) dataShort[i]) / qMax(-(float) SHRT_MIN, (float) SHRT_MAX);
                    }
                    break;
                }
                case SAMPLE_FMT_S32: {
                    int32_t *dataInt =    (int32_t*) &handle->audioBuffer;
                    numberRead = (size_t) dataSize / sizeof(int32_t) / (size_t) handle->codecContext->channels;
                    for (i = 0; i < (size_t) dataSize / sizeof(int32_t); ++i) {
                        handle->buffer[i] = ((float) dataInt[i]) / qMax(-(float) INT_MIN, (float) INT_MAX);
                    }
                    break;
                }
                case SAMPLE_FMT_FLT: {
                    float *dataFloat =  (float*)   &handle->audioBuffer;
                    numberRead = (size_t) dataSize / sizeof(float) / (size_t) handle->codecContext->channels;
                    for (i = 0; i < (size_t) dataSize / sizeof(float); ++i) {
                        handle->buffer[i] = dataFloat[i];
                    }
                    break;
                }
                case SAMPLE_FMT_DBL: {
                    double *dataDouble = (double*)  &handle->audioBuffer;
                    numberRead = (size_t) dataSize / sizeof(double) / (size_t) handle->codecContext->channels;
                    for (i = 0; i < (size_t) dataSize / sizeof(double); ++i) {
                        handle->buffer[i] = (float) dataDouble[i];
                    }
                    break;
                }
                case SAMPLE_FMT_NONE:
                case SAMPLE_FMT_NB:
                default:
    //                 fprintf(stderr, "Unknown sample format!\n");
                    return 0;
                }
                return numberRead;
            }
            handle->packet.data = handle->oldData;
            handle->oldData = NULL;
        }
        av_free_packet(&handle->packet);
        handle->needNewFrame = TRUE;
    }
}
