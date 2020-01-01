/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This class is a C++/Qt version of input_ffmpeg.c from libebur128
 */

/* See LICENSE file for copyright and license details. */
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
#include <libavutil/channel_layout.h>
#endif
#ifdef __cplusplus
}
#endif
#include <QMutex>
#include <QFile>
#include <QString>
#include <QList>
#include <QByteArray>
#include "ebur128/ebur128.h"
#include "ffmpeginput.h"

static QMutex mutex;

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#endif

#if defined FF_INPUT_BUFFER_PADDING_SIZE
    #define FFMPEG_INPUT_BUFFER_PADDING_SIZE FF_INPUT_BUFFER_PADDING_SIZE
#elif defined AV_INPUT_BUFFER_PADDING_SIZE
    #define FFMPEG_INPUT_BUFFER_PADDING_SIZE AV_INPUT_BUFFER_PADDING_SIZE
#else
    #define FFMPEG_INPUT_BUFFER_PADDING_SIZE 32
#endif

#define BUFFER_SIZE ((((AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2) * sizeof(int16_t)) + FFMPEG_INPUT_BUFFER_PADDING_SIZE)

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 89, 100) // Not 100% of version here!
#define GET_CODEC_TYPE(A) A->codecpar->codec_type
#else
#define GET_CODEC_TYPE(A) A->codec->codec_type
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 24, 0)
#define AV_FREE(PKT) av_packet_unref(PKT)
#else
#define AV_FREE(PKT) av_free_packet(PKT)
#endif

struct FfmpegInput::Handle {
    Handle()
        : formatContext(0)
        , codecContext(0)
        , codec(0)
        #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
        , frame(0)
        , gotFrame(0)
        , packetLeft(false)
        , flushing(false)
        #endif
        , audioStream(0)
        , currentBytes(0) {
        av_init_packet(&packet);
        audioBuffer = (int16_t*)av_malloc(BUFFER_SIZE);
        #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
        #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 39, 101)
        frame = avcodec_alloc_frame();
        #else
        frame = av_frame_alloc();
        packet.data = NULL;
        origPacket.size = 0;
        origPacket.data=NULL;
        #endif
        #endif
    }
    ~Handle() {
        if (audioBuffer) {
            av_free(audioBuffer);
        }
        #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
        if (frame) {
            #if LIBAVCODEC_VERSION_INT <= AV_VERSION_INT(54, 23, 100)
            av_free(&frame);
            #elif LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 39, 101)
            avcodec_free_frame(&frame);
            #else
            av_frame_free(&frame);
            #endif
        }
        #endif
    }
    AVFormatContext *formatContext;
    AVCodecContext *codecContext;
    AVCodec *codec;
    #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
    AVFrame *frame;
    int gotFrame;
    bool packetLeft;
    bool flushing;
    AVPacket origPacket;
    #endif
    AVPacket packet;
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
        #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 18, 100)
        av_register_all();
        #endif
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
            if (GET_CODEC_TYPE(handle->formatContext->streams[j]) ==
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
        #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 89, 100) // Not 100% of version here!
        handle->codec=avcodec_find_decoder(handle->formatContext->streams[handle->audioStream]->codecpar->codec_id);
        handle->codecContext=avcodec_alloc_context3(handle->codec);
        if (avcodec_parameters_to_context(handle->codecContext, handle->formatContext->streams[handle->audioStream]->codecpar)<0) {
            ok = false;
        }
        #else
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
        #endif

        if (ok) {
            QString floatCodec=QLatin1String(handle->codec->name)+QLatin1String("float");
            AVCodec *possibleFloatCodec = avcodec_find_decoder_by_name(floatCodec.toLatin1().constData());
            if (possibleFloatCodec) {
                handle->codec = possibleFloatCodec;
                #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 89, 100) // Not 100% of version here!
                avcodec_free_context(&handle->codecContext);
                handle->codecContext=avcodec_alloc_context3(handle->codec);
                if (avcodec_parameters_to_context(handle->codecContext, handle->formatContext->streams[handle->audioStream]->codecpar)<0) {
                    ok = false;
                }
                #endif
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

#if LIBAVCODEC_VERSION_MAJOR >= 54
static int decodePacket(FfmpegInput::Handle *handle)
{
    #if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 89, 100) // Not 100% of version here!
    int ret = avcodec_receive_frame(handle->codecContext, handle->frame);
    if (0 == ret) {
        handle->gotFrame = true;
    }
    if (AVERROR(EAGAIN) == ret) {
        ret = 0;
    }
    if (0 == ret) {
        ret = avcodec_send_packet(handle->codecContext, &handle->packet);
    }
    if (AVERROR(EAGAIN) == ret) {
        ret = 0;
    }
    return ret<0 ? ret : handle->packet.size;
    #else
    int ret = avcodec_decode_audio4(handle->codecContext, handle->frame, &handle->gotFrame, &handle->packet);
    if (ret < 0) {
        return ret;
    }
    return FFMIN(ret, handle->packet.size);
    #endif
}

size_t FfmpegInput::readOnePacket()
{
    if (!handle) {
        return 0;
    }

start:
    if (handle->flushing) {
        handle->packet.data = NULL;
        handle->packet.size = 0;
        decodePacket(handle);
        if (!handle->gotFrame) {
            return 0;
        } else {
            goto write_to_buffer;
        }
    }

    if (handle->packetLeft) {
        goto packetLeft;
    }

//next_frame:
    for (;;) {
        if (av_read_frame(handle->formatContext, &handle->packet) < 0) {
            handle->flushing = true;
            goto start;
        }
        if (handle->packet.stream_index != handle->audioStream) {
            AV_FREE(&handle->packet);
            continue;
        } else {
            break;
        }
    }

    int ret;
    handle->origPacket = handle->packet;
packetLeft:
    ret = decodePacket(handle);
    if (ret < 0) {
        goto free_packet;
    }
    handle->packet.data += ret;
    handle->packet.size -= ret;
    if (handle->packet.size > 0) {
        handle->packetLeft = true;
    } else {
free_packet:
        AV_FREE(&handle->origPacket);
        handle->packetLeft = false;
    }

    if (!handle->gotFrame) {
        goto start;
    }

write_to_buffer: ;
    size_t numberRead=handle->frame->nb_samples;
    /* TODO: fix this */
    int numChannels = handle->codecContext->channels;
    // channels = handle->frame->channels;

    if (handle->frame->nb_samples * numChannels > (int)sizeof handle->buffer) {
        return 0;
    }

    switch (handle->codecContext->sample_fmt) {
    case AV_SAMPLE_FMT_S16: {
        int16_t *dataShort = (int16_t*)handle->frame->extended_data[0];
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            handle->buffer[i] = ((float) dataShort[i])/qMax(-(float) SHRT_MIN, (float) SHRT_MAX);
        }
        break;
    }
    case AV_SAMPLE_FMT_S32: {
        int32_t *dataInt = (int32_t*)handle->frame->extended_data[0];
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            handle->buffer[i] = ((float) dataInt[i])/qMax(-(float) INT_MIN, (float) INT_MAX);
        }
        break;
    }
    case AV_SAMPLE_FMT_FLT: {
        float *dataFloat = (float*)handle->frame->extended_data[0];
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            handle->buffer[i] = dataFloat[i];
        }
        break;
    }
    case AV_SAMPLE_FMT_DBL: {
        double *dataDouble = (double*)handle->frame->extended_data[0];
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            handle->buffer[i] = (float)dataDouble[i];
        }
        break;
    }
    case AV_SAMPLE_FMT_S16P: {
        uint8_t **ed = handle->frame->extended_data;
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            int currentChannel = i / handle->frame->nb_samples;
            int currentSample = i % handle->frame->nb_samples;
            handle->buffer[currentSample * numChannels + currentChannel] = ((float)((int16_t*) ed[currentChannel])[currentSample])/qMax(-(float)SHRT_MIN, (float)SHRT_MAX);
        }
        break;
    }
    case AV_SAMPLE_FMT_S32P: {
        uint8_t **ed = handle->frame->extended_data;
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            int currentChannel = i / handle->frame->nb_samples;
            int currentSample = i % handle->frame->nb_samples;
            handle->buffer[currentSample * numChannels + currentChannel] = ((float) ((int32_t*) ed[currentChannel])[currentSample])/qMax(-(float)INT_MIN, (float)INT_MAX);
        }
        break;
    }
    case AV_SAMPLE_FMT_FLTP: {
        uint8_t **ed = handle->frame->extended_data;
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            int currentChannel = i / handle->frame->nb_samples;
            int currentSample = i % handle->frame->nb_samples;
            handle->buffer[currentSample * numChannels + currentChannel] = ((float*) ed[currentChannel])[currentSample];
        }
        break;
    }
    case AV_SAMPLE_FMT_DBLP:{
        uint8_t **ed = handle->frame->extended_data;
        for (int i = 0; i < handle->frame->nb_samples * numChannels; ++i) {
            int currentChannel = i / handle->frame->nb_samples;
            int currentSample = i % handle->frame->nb_samples;
            handle->buffer[currentSample * numChannels + currentChannel] = ((double*) ed[currentChannel])[currentSample];
        }
        break;
    }
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_NONE:
    case AV_SAMPLE_FMT_NB:
    default:
        numberRead=0;
        goto out;
    }

    out:
    return numberRead;
}
#else
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53, 35, 0)
static int decodeAudio(FfmpegInput::Handle *handle, int *frame_size_ptr)
{
    int ret, got_frame = 0;

    if (!handle->frame) {
         return AVERROR(ENOMEM);
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
        AV_FREE(&handle->packet);
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
        AV_FREE(&handle->packet);
        goto next_frame;
    }
    switch (handle->codecContext->sample_fmt) {
    case SAMPLE_FMT_S16: {
        int16_t *dataShort = (int16_t*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(int16_t) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(int16_t); ++i) {
            handle->buffer[i] = ((float) dataShort[i]) / qMax(-(float) SHRT_MIN, (float) SHRT_MAX);
        }
        break;
    }
    case SAMPLE_FMT_S32: {
        int32_t *dataInt = (int32_t*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(int32_t) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(int32_t); ++i) {
            handle->buffer[i] = ((float) dataInt[i]) / qMax(-(float) INT_MIN, (float) INT_MAX);
        }
        break;
    }
    case SAMPLE_FMT_FLT: {
        float *dataFloat = (float*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(float) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(float); ++i) {
            handle->buffer[i] = dataFloat[i];
        }
        break;
    }
    case SAMPLE_FMT_DBL: {
        double *dataDouble = (double*) handle->audioBuffer;
        numberRead = (size_t) dataSize / sizeof(double) / (size_t) handle->codecContext->channels;
        for (unsigned int i = 0; i < (size_t) dataSize / sizeof(double); ++i) {
            handle->buffer[i] = (float) dataDouble[i];
        }
        break;
    }
    case SAMPLE_FMT_U8:
    case SAMPLE_FMT_NONE:
    case SAMPLE_FMT_NB:
    default:
        numberRead=0;
        goto out;
    }

    out:
    AV_FREE(&handle->packet);
    return numberRead;
}
#endif

bool FfmpegInput::isFloatCodec() const
{
    return handle && handle->codec && QString(QLatin1String(handle->codec->name)).endsWith(QLatin1String("float"));
}
