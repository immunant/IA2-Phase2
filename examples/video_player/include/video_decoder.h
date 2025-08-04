/*
 * Copyright (c) 2023 Immunant, Inc.
 */
#pragma once

#include <libavcodec/avcodec.h>
#include <stdbool.h>
#include <stddef.h>

struct video_decoder;

typedef void (*video_decoder_frame_callback_t)(AVFrame *, void *);

struct video_decoder *video_decoder_init(const char *file_data,
                                         size_t file_size);

void video_decoder_get_info(struct video_decoder *, int *width, int *height,
                            enum AVPixelFormat *pix_fmt);

bool video_decoder_decode(struct video_decoder *, void *,
                          video_decoder_frame_callback_t);
