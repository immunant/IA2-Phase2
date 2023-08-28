/*
 * Copyright (c) 2023 Immunant, Inc.
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <sys/param.h>
#include <video_decoder.h>

#include <ia2.h>
#define IA2_COMPARTMENT 2
#define IA2_COMPARTMENT_LIBRARIES "libavcodec.so;libavformat.so"
#include <ia2_compartment_init.inc>

#define AVIO_BUF_SIZE 65536

struct video_decoder {
  const char *file_data;
  size_t file_size;
  AVFormatContext *fmt_ctx;
  AVIOContext *avio_ctx;
  AVCodecContext *codec_ctx;
  int video_stream_idx;
  bool fmt_ctx_opened;
};

static int read_input_buffer(void *opaque, uint8_t *buf, int buf_size) {
  struct video_decoder *d = opaque;
  size_t count = MIN(d->file_size, buf_size);
  if (!count) {
    return AVERROR_EOF;
  }

  memcpy(buf, d->file_data, count);
  d->file_data += count;
  d->file_size -= count;
  return count;
}

struct video_decoder *video_decoder_init(const char *file_data,
                                         size_t file_size) {
  struct video_decoder *d = NULL;
  int ret;

  d = calloc(1, sizeof(struct video_decoder));
  if (!d) {
    goto err_alloc_decoder;
  }

  d->file_data = file_data;
  d->file_size = file_size;

  d->fmt_ctx = avformat_alloc_context();
  if (!d->fmt_ctx) {
    goto err_alloc_fmt_context;
  }

  void *avio_ctx_buffer = av_malloc(AVIO_BUF_SIZE);
  if (!avio_ctx_buffer) {
    goto err_alloc_buffer;
  }

  d->avio_ctx = avio_alloc_context(avio_ctx_buffer, AVIO_BUF_SIZE, 0, d,
                                   IA2_IGNORE(read_input_buffer), NULL, NULL);
  if (!d->avio_ctx) {
    goto err_alloc_avio_context;
  }
  d->fmt_ctx->pb = d->avio_ctx;

  /* d->avio_ctx takes ownership of the buffer */
  avio_ctx_buffer = NULL;

  ret = avformat_open_input(&d->fmt_ctx, NULL, NULL, NULL);
  if (ret) {
    goto err_open_input;
  }
  d->fmt_ctx_opened = true;

  ret = avformat_find_stream_info(d->fmt_ctx, NULL);
  if (ret < 0) {
    goto err_find_stream_info;
  }

  d->video_stream_idx =
      av_find_best_stream(d->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (d->video_stream_idx < 0) {
    goto err_find_stream;
  }

  AVStream *video_stream = d->fmt_ctx->streams[d->video_stream_idx];
  const AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
  if (!codec) {
    goto err_find_codec;
  }

  d->codec_ctx = avcodec_alloc_context3(codec);
  if (!d->codec_ctx) {
    goto err_alloc_codec_context;
  }
  d->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  ret = avcodec_parameters_to_context(d->codec_ctx, video_stream->codecpar);
  if (ret) {
    goto err_params_to_context;
  }

  ret = avcodec_open2(d->codec_ctx, codec, NULL);
  if (ret < 0) {
    goto err_open_codec;
  }

  /* Success */
  return d;

err_open_codec:
err_params_to_context:
  avcodec_free_context(&d->codec_ctx);
err_alloc_codec_context:
err_find_codec:
err_find_stream:
err_find_stream_info:
  avformat_close_input(&d->fmt_ctx);
err_open_input:
  av_free(d->avio_ctx->buffer);
  av_free(d->avio_ctx);
err_alloc_avio_context:
  av_free(avio_ctx_buffer);
err_alloc_buffer:
  avformat_free_context(d->fmt_ctx);
err_alloc_fmt_context:
  free(d);
err_alloc_decoder:
  return NULL;
}

void video_decoder_get_info(struct video_decoder *d, int *width, int *height,
                            enum AVPixelFormat *pix_fmt) {
  if (width) {
    *width = d->codec_ctx->width;
  }
  if (height) {
    *height = d->codec_ctx->height;
  }
  if (pix_fmt) {
    *pix_fmt = d->codec_ctx->pix_fmt;
  }
}

bool video_decoder_decode(struct video_decoder *d, void *ctx,
                          video_decoder_frame_callback_t f) {
  int ret;

  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    goto err_alloc_frame;
  }

  AVPacket *pkt = av_packet_alloc();
  if (!pkt) {
    goto err_alloc_packet;
  }

  for (;;) {
    ret = av_read_frame(d->fmt_ctx, pkt);
    if (ret < 0) {
      break;
    }

    if (pkt->stream_index != d->video_stream_idx) {
      av_packet_unref(pkt);
      continue;
    }

    ret = avcodec_send_packet(d->codec_ctx, pkt);
    av_packet_unref(pkt);
    if (ret < 0) {
      goto err_send_packet;
    }

    while (ret >= 0) {
      ret = avcodec_receive_frame(d->codec_ctx, frame);
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
        break;
      } else if (ret < 0) {
        goto err_receive_frame;
      }

      f(frame, ctx);
      av_frame_unref(frame);
    }

    if (ret == AVERROR_EOF) {
      break;
    }
  }

  av_packet_free(&pkt);
  av_frame_free(&frame);
  return true;

err_receive_frame:
err_send_packet:
  av_packet_free(&pkt);
err_alloc_packet:
  av_frame_free(&frame);
err_alloc_frame:
  return false;
}
