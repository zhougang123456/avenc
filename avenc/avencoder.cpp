﻿#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avencoder.hpp"
#include "libyuv.h"
#include <time.h>
#include <Windows.h>

#define DEFAULT_BIT_RATE 10000000
#define DEFAULT_FRAME_RATE 33
#define DUMP_H264
#ifdef  DUMP_H264
#define DUMP_FILE_NAME "output.h264"
static FILE* f = NULL;
#endif //  DUMP_H264

static inline uint64_t get_time()
{
    time_t clock;
    timeval now;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    now.tv_sec = clock;
    now.tv_usec = wtm.wMilliseconds * 1000;
    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;
}

AvEncoder::AvEncoder()
{
}

AvEncoder::~AvEncoder()
{   
    if (context) {
        avcodec_free_context(&context);
    }
    if (frm420p) {
        av_frame_free(&frm420p);
    }
    if (frmArgb) {
        av_frame_free(&frmArgb);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
#ifdef DUMP_H264
    if (f) {
        fclose(f);
    }
#endif // DUMP_H264
}
bool AvEncoder::init(char* codec_name, int width, int height)
{   
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        printf("Codec '%s' not found\n", codec_name);
        return false;
    }
    context = avcodec_alloc_context3(codec);
    if (!context) {
        printf("Could not allocate video codec context\n");
        return false;
    }
    context->bit_rate = DEFAULT_BIT_RATE;
    context->width = width;
    context->height = height;
    AVRational time_base = { 1, DEFAULT_FRAME_RATE };
    context->time_base = time_base;
    AVRational framerate = { DEFAULT_FRAME_RATE, 1 };
    context->framerate = framerate;
    context->gop_size = 100;
    context->max_b_frames = 0;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->qmin = 15;
    context->qmax = 35;
    context->thread_count = 4;
    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(context->priv_data, "preset", "ultrafast", 0);
        av_opt_set(context->priv_data, "tune", "zerolatency", 0);
        av_opt_set(context->priv_data, "profile", "baseline", 0);
    }
    int ret = avcodec_open2(context, codec, NULL);
    if (ret < 0) {
        printf("Could not open codec: %d\n", ret);
        return false;
    }
#ifdef DUMP_H264
    /*f = fopen(DUMP_FILE_NAME, "wb");
    if (!f) {
        printf("Could not open %s\n", DUMP_FILE_NAME);
        return false;
    }*/
#endif // DUMP_H264

    pkt = av_packet_alloc();
    if (!pkt) {
        return false;
    }

    frm420p = av_frame_alloc();
    if (!frm420p) {
        printf("Could not allocate video frame\n");
        return false;
    }
    frmArgb = av_frame_alloc();
    if (!frmArgb) {
        printf("Could not allocate video frame\n");
        return false;
    }
    frm420p->format = context->pix_fmt;
    frm420p->width = context->width;
    frm420p->height = context->height;
    frmArgb->width = context->width;
    frmArgb->height = context->height;
    frmArgb->format = AV_PIX_FMT_RGBA;
    ret = av_frame_get_buffer(frm420p, 32);
    if (ret < 0) {
        printf("Could not allocate the video frame data\n");
        return false;
    }
    return true;
}
void AvEncoder::encode(unsigned char* buffer)
{
    av_image_fill_arrays(frmArgb->data, frmArgb->linesize, buffer, AV_PIX_FMT_RGBA, context->width, context->height, 32);
    int ret;
    static int j = 0;
        rgba_to_yuv420(frmArgb, frm420p);
    
    
    frm420p->pts = ++j;
    /* send the frame to the encoder */
    if (frm420p) {
        printf("Send frame %3lld\n", frm420p->pts);
    }
    ret = avcodec_send_frame(context, frm420p);
    if (ret < 0) {
        printf("Error sending a frame for encoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(context, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            exit(1);
        }

        printf("Write packet %3lld(size=%5d)\n", pkt->pts, pkt->size);
#ifdef DUMP_H264
        char file[200];
        sprintf(file, "%d.h264", j);
        f = fopen(file, "wb");
        fwrite(pkt->data, 1, pkt->size, f);
        fclose(f);
#endif // DUMP_H264

        av_packet_unref(pkt);
    }
}
bool AvEncoder::rgba_to_yuv420(AVFrame* frmArgb, AVFrame* frm420p)
{   
    uint64_t start = get_time();
    libyuv::ARGBToI420(frmArgb->data[0], frmArgb->linesize[0], frm420p->data[0], frm420p->linesize[0], frm420p->data[1], frm420p->linesize[1], frm420p->data[2], frm420p->linesize[2], context->width, context->height);
    uint64_t end = get_time();
    printf("yuv time is %u", end - start);
    //return true;
    struct SwsContext* sws = sws_getContext(context->width, context->height, AV_PIX_FMT_RGBA, 
        context->width, context->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL);
    int ret = sws_scale(sws, frmArgb->data, frmArgb->linesize, 0, context->height, frm420p->data, frm420p->linesize);
    printf("scale time is %u", get_time() - end);
    sws_freeContext(sws);
    return  (ret == context->height) ? true : false;
}

