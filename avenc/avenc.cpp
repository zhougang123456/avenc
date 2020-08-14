/*
 * Copyright (c) 2001 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

 /**
  * @file
  * video encoding with libavcodec API example
  *
  * @example encode_video.c
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
};

static void encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt,
	FILE* outfile)
{
	int ret;

	/* send the frame to the encoder */
	if (frame)
		printf("Send frame %3lld\n", frame->pts);

	ret = avcodec_send_frame(enc_ctx, frame);
	if (ret < 0) {
		fprintf(stderr, "Error sending a frame for encoding\n");
		exit(1);
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(enc_ctx, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during encoding\n");
			exit(1);
		}

		printf("Write packet %3lld(size=%5d)\n", pkt->pts, pkt->size);
		fwrite(pkt->data, 1, pkt->size, outfile);
		av_packet_unref(pkt);
	}
}

static int rgba_to_yuv420(AVFrame* frmArgb, AVFrame* frm420p, int w, int h)
{
	

	/*绑定数据缓冲区
	avpicture_fill((AVPicture*)frmArgb, buf_rgba, AV_PIX_FMT_RGBA, w, h);
	avpicture_fill((AVPicture*)frm420p, buf_yuv, AV_PIX_FMT_YUV420P, w, h);*/

	//指定原数据格式，分辨率及目标数据格式，分辨率
	struct SwsContext* sws = sws_getContext(w, h, AV_PIX_FMT_RGBA, w, h, AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, NULL, NULL, NULL);

	//转换
	int ret = sws_scale(sws, frmArgb->data, frmArgb->linesize, 0, h, frm420p->data, frm420p->linesize);
	//av_frame_free(&frmArgb);
	//av_frame_free(&frm420p);
	sws_freeContext(sws);
	return  (ret == h) ? 0 : -1;
}

int main(int argc, char** argv)
{
	const char* filename, * codec_name;
	const AVCodec* codec;
	AVCodecContext* c = NULL;
	int i, ret, x, y;
	FILE* f;
	AVFrame* frmArgb;
	AVFrame* frm420p;
	
	AVPacket* pkt;
	uint8_t endcode[] = { 0, 0, 1, 0xb7 };

	filename = "output.h264";
	codec_name = "libx264";

	/* find the mpeg1video encoder */
	codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "Codec '%s' not found\n", codec_name);
		exit(1);
	}

	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}

	pkt = av_packet_alloc();
	if (!pkt)
		exit(1);

	/* put sample parameters */
	c->bit_rate = 400000;
	/* resolution must be a multiple of two */
	c->width = 352;
	c->height = 288;
	/* frames per second */
	AVRational time_base = { 1, 25 };
	c->time_base = time_base;
	AVRational framerate = { 25, 1 };
	c->framerate = framerate;

	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV420P;

	if (codec->id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		fprintf(stderr, "Could not open codec: %d\n", ret);
		exit(1);
	}

	f = fopen(filename, "wb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}

	frm420p = av_frame_alloc();
	if (!frm420p) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	frmArgb = av_frame_alloc();
	
	frm420p->format = c->pix_fmt;
	frm420p->width = c->width;
	frm420p->height = c->height;

	frmArgb->width = c->width;
	frmArgb->height = c->height;
	frmArgb->format = AV_PIX_FMT_RGBA;

	ret = av_frame_get_buffer(frm420p, 32);
	ret = av_frame_get_buffer(frmArgb, 32);

	if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		exit(1);
	}

	/* encode 1 second of video */
	for (i = 0; i < 25; i++) {
		fflush(stdout);

		/* make sure the frame data is writable */
		ret = av_frame_make_writable(frmArgb);
		if (ret < 0)
			exit(1);

		/* prepare a dummy image */
		/* Y */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				frmArgb->data[0][y * frmArgb->linesize[0] + x * 4] = i * 10;
				frmArgb->data[0][y * frmArgb->linesize[0] + x * 4 + 1] = 0;
				frmArgb->data[0][y * frmArgb->linesize[0] + x * 4  + 2] = 0;
			}
		}


		///* Cb and Cr */
		//for (y = 0; y < c->height / 2; y++) {
		//	for (x = 0; x < c->width / 2; x++) {
		//		frm420p->data[1][y * frm420p->linesize[1] + x] = 128 + y + i * 2;
		//		frm420p->data[2][y * frm420p->linesize[2] + x] = 64 + x + i * 5;
		//	}
		//}

		frm420p->pts = i;

		rgba_to_yuv420(frmArgb, frm420p, c->width, c->height);
		/* encode the image */
		encode(c, frm420p, pkt, f);
	}

	/* flush the encoder */
	encode(c, NULL, pkt, f);

	/* add sequence end code to have a real MPEG file */
	fwrite(endcode, 1, sizeof(endcode), f);
	fclose(f);

	avcodec_free_context(&c);
	av_frame_free(&frm420p);
	av_packet_free(&pkt);

	return 0;
}
