/*
 * Copyright (c) 2011 Reinhard Tartler
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
 * Shows how the metadata API can be used in application programs.
 * @example metadata.c
 */

#include <stdio.h>

#include "libavutil/mem.h"
#include <ia2.h>
#include <libavformat/avformat.h>
#include <libavutil/common.h>
#include <libavutil/dict.h>

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

int main(int argc, char **argv) {
  AVFormatContext *fmt_ctx = NULL;
  AVDictionaryEntry *tag = NULL;
  int ret;

  if (argc != 2) {
    printf(
        "usage: %s <input_file>\n"
        "example program to demonstrate the use of the libavformat metadata API.\n"
        "\n",
        argv[0]);
    return 1;
  }

  if ((ret = avformat_open_input(&fmt_ctx, argv[1], NULL, NULL)))
    return ret;

  if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
    return ret;
  }

  while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    printf("%s=%s\n", tag->key, tag->value);

  avformat_close_input(&fmt_ctx);
  return 0;
}
