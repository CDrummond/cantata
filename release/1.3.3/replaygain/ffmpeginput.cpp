/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#if LIBAVFORMAT_VERSION_MAJOR >= 54
#include <libavutil/audioconvert.h>
#endif
#ifdef __cplusplus
}
#endif
#include <QMutex>
#include <QFile>
#include <QString>
#include <QList>
#include <QByteArray>
#include "ebur128.h"
#include "ffmpeginput.h"

static QMutex mutex;

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#endif

#define BUFFER_SIZE ((((AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2) * sizeof(int16_t)) + FF_INPUT_BUFFER_PADDING_SIZE)

struct FfmpegInput::Handle {
    Handle()
        : formatContext(0)
        , codecContext(0)
        , codec(0)
        #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
        , frame(0)
        #endif
        , audioStream(0)
        , currentBytes(0) {
        av_init_packet(&packet);
        audioBuffer = (int16_t*)av_malloc(BUFFER_SIZE);
    }
    ~Handle() {
        if (audioBuffer) {
            av_free(audioBuffer);
        }
        #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
        if (frame) {
            av_free(frame);
        }
        #endif
    }
    AVFormatContext *formatContext;
    AVCodecContext *codecContext;
    AVCodec *codec;
    AVPacket packet;
    #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
    AVFrame *frame;
    #endif
    int audioStream;
    int16_t *audioBuffer;
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
    #if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 21, 0)
    if (ok && avformat_find_stream_info(handle->formatContext, 0) < 0) {
        ok=false;
    }
    #else
    if (ok && av_find_stream_info(handle->formatContext) < 0) {
        ok=false;
    }
    #endif

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
        #if LIBAVCODEC_VERSION_MAJOR >= 54
        handle->codecContext->request_sample_fmt = AV_SAMPLE_FMT_FLT;
        #elif(LIBAVCODEC_VERSION_MAJOR == 53 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 4, 0))
        handle->codecContext->request_sample_fmt = SAMPLE_FMT_FLT;
        #endif
        // Find the decoder for the video stream, and open codec...
        handle->codec = avcodec_find_decoder(handle->codecContext->codec_id);

        QString floatCodec=QLatin1String(handle->codec->name)+QLatin1String("float");
        AVCodec *possibleFloatCodec = avcodec_find_decoder_by_name(floatCodec.toLatin1().constData());
        if (possibleFloatCodec) {
            handle->codec = possibleFloatCodec;
        }

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
    } else {
        #if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 21, 0)
        avformat_close_input(&handle->formatContext);
        #else
        av_close_input_file(handle->formatContext);
        #endif
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
        #if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53, 21, 0)
        avformat_close_input(&handle->formatContext);
        #else
        av_close_input_file(handle->formatContext);
        #endif
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

bool FfmpegInput::setChannelMap(int *st) const
{
    if (handle && handle->codecContext->channel_layout) {
        unsigned int mapIndex = 0;
        int bitCounter = 0;
        while (mapIndex < (unsigned) handle->codecContext->channels) {
            if (handle->codecContext->channel_layout & (1 << bitCounter)) {
                switch (1 << bitCounter) {
                #if LIBAVFORMAT_VERSION_MAJOR >= 54
                case AV_CH_FRONT_LEFT:
                #else
                case CH_FRONT_LEFT:
                #endif
                    st[mapIndex] = EBUR128_LEFT;
                    break;
                #if LIBAVFORMAT_VERSION_MAJOR >= 54
                case AV_CH_FRONT_RIGHT:
                #else
                case CH_FRONT_RIGHT:
                #endif
                    st[mapIndex] = EBUR128_RIGHT;
                    break;
                #if LIBAVFORMAT_VERSION_MAJOR >= 54
                case AV_CH_FRONT_CENTER:
                #else
                case CH_FRONT_CENTER:
                #endif
                    st[mapIndex] = EBUR128_CENTER;
                    break;
                #if LIBAVFORMAT_VERSION_MAJOR >= 54
                case AV_CH_BACK_LEFT:
                #else
                case CH_BACK_LEFT:
                #endif
                    st[mapIndex] = EBUR128_LEFT_SURROUND;
                    break;
                #if LIBAVFORMAT_VERSION_MAJOR >= 54
                case AV_CH_BACK_RIGHT:
                #else
                case CH_BACK_RIGHT:
                #endif
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

    size_t bufferPosition=0, numberRead=0;

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

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
static int decodeAudio(FfmpegInput::Handle *handle, int *frame_size_ptr)
{
    int ret, got_frame = 0;

    if (!handle->frame) {
        handle->frame = avcodec_alloc_frame();
        if (!handle->frame) {
            return AVERROR(ENOMEM);
        }
    } else {
        avcodec_get_frame_defaults(handle->frame);
    }

    #if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(55, 0, 0)
    if (handle->codecContext->get_buffer != avcodec_default_get_buffer) {
        handle->codecContext->get_buffer = avcodec_default_get_buffer;
        handle->codecContext->release_buffer = avcodec_default_release_buffer;
    }
    #endif

    ret = avcodec_decode_audio4(handle->codecContext, handle->frame, &got_frame, &handle->packet);

    if (ret >= 0 && got_frame) {
        int ch, plane_size;
        int planar = av_sample_fmt_is_planar(handle->codecContext->sample_fmt);
        int data_size = av_samples_get_buffer_size(&plane_size, handle->codecContext->channels,
                                                   handle->frame->nb_samples, handle->codecContext->sample_fmt, 1);
        if (*frame_size_ptr < data_size) {
            return AVERROR(EINVAL);
        }

        memcpy(handle->audioBuffer, handle->frame->extended_data[0], plane_size);

        if (planar && handle->codecContext->channels > 1) {
            uint8_t *out = ((uint8_t *)(handle->audioBuffer)) + plane_size;
            for (ch = 1; ch < handle->codecContext->channels; ch++) {
                memcpy(out, handle->frame->extended_data[ch], plane_size);
                out += plane_size;
            }
        }
        *frame_size_ptr = data_size;
    } else {
        *frame_size_ptr = 0;
    }
    return ret;
}
#endif

size_t FfmpegInput::readOnePacket()
{
    if (!handle) {
        return 0;
    }

    next_frame:
    for (;;) {
        if (av_read_frame(handle->formatContext, &handle->packet) < 0) {
            return 0;
        }
        if (handle->packet.stream_index == handle->audioStream) {
            break;
        }
        av_free_packet(&handle->packet);
    }

    size_t numberRead=0;
    int dataSize=BUFFER_SIZE;
    #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
    int len = decodeAudio(handle, &dataSize);
    #elif LIBAVCODEC_VERSION_MAJOR >= 53 || \
        (LIBAVCODEC_VERSION_MAJOR == 52 && LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 23, 0))
    int len = avcodec_decode_audio3(handle->codecContext, handle->audioBuffer, &dataSize, &handle->packet);
    #else
    int len = avcodec_decode_audio2(handle->codecContext, handle->audioBuffer, &dataSize, handle->packet.data, handle->packet.size);
    #endif

    if (len < 0) {
        goto out;
    }

    /* No data used, (happens with metadata frames for example) */
    if (len <= 0) {
        av_free_packet(&handle->packet);
        goto next_frame;
    }

    switch (handle->codecContext->sample_fmt) {
    #if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_SAMPLE_FMT_S16:
    #else
    case SAMPLE_FMT_S16:
    #endif
    {
        int16_t *dataShort = (int16_t*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(int16_t) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(int16_t); ++i) {
            handle->buffer[i] = ((float) dataShort[i]) / qMax(-(float) SHRT_MIN, (float) SHRT_MAX);
        }
        break;
    }
    #if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_SAMPLE_FMT_S32:
    #else
    case SAMPLE_FMT_S32:
    #endif
    {
        int32_t *dataInt = (int32_t*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(int32_t) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(int32_t); ++i) {
            handle->buffer[i] = ((float) dataInt[i]) / qMax(-(float) INT_MIN, (float) INT_MAX);
        }
        break;
    }
    #if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_SAMPLE_FMT_FLT:
    #else
    case SAMPLE_FMT_FLT:
    #endif
    {
        float *dataFloat = (float*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(float) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(float); ++i) {
            handle->buffer[i] = dataFloat[i];
        }
        break;
    }
    #if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_SAMPLE_FMT_DBL:
    #else
    case SAMPLE_FMT_DBL:
    #endif
    {
        double *dataDouble = (double*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(double) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(double); ++i) {
            handle->buffer[i] = (float) dataDouble[i];
        }
        break;
    }
    #if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_NONE:
    case AV_SAMPLE_FMT_NB:
    #else
    case SAMPLE_FMT_U8:
    case SAMPLE_FMT_NONE:
    case SAMPLE_FMT_NB:
    #endif
    default:
        numberRead=0;
        goto out;
    }

    out:
    av_free_packet(&handle->packet);
    return numberRead;
}

bool FfmpegInput::isFloatCodec() const
{
    return handle && handle->codec && QString(QLatin1String(handle->codec->name)).endsWith(QLatin1String("float"));
}
