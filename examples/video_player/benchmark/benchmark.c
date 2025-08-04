/*
 * Copyright (c) 2023 Immunant, Inc.
 */

#include <err.h>
#include <fcntl.h>
#include <ia2.h>
#include <libswscale/swscale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <video_decoder.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
#if IA2_ENABLE
  ia2_register_compartment("main", 1, NULL);
#endif
}

static char *video_data;
static size_t video_size;
static int video_width IA2_SHARED_DATA;
static int video_height IA2_SHARED_DATA;
static enum AVPixelFormat video_pix_fmt IA2_SHARED_DATA;
static struct SwsContext *sws_ctx;
static uint8_t *y_plane, *u_plane, *v_plane;

static void process_frame(AVFrame *frame, void *ctx) {}

void decode() {
  struct video_decoder *decoder;

  decoder = video_decoder_init(video_data, video_size);
  if (!decoder) {
    errx(1, "Failed to initialize decoder");
  }
  video_decoder_get_info(decoder, &video_width, &video_height, &video_pix_fmt);
  printf("Video dimensions: %dx%d\n", video_width, video_height);

  if (!video_decoder_decode(decoder, NULL, process_frame)) {
    errx(1, "Failed to decode frame");
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    errx(1, "Usage: video_decoder <file>");
  }

  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    err(1, "Failed to open file: %s", argv[1]);
  }

  struct stat buf;
  if (fstat(fd, &buf) == -1) {
    err(1, "stat() failed on file: %s", argv[1]);
  }

  video_size = buf.st_size;
  printf("Playing video: %zu bytes\n", video_size);

  video_data = mmap(NULL, video_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (video_data == MAP_FAILED) {
    err(1, "Failed to mmap file: %s", argv[1]);
  }

  decode();

  munmap(video_data, video_size);
  close(fd);
  return 0;
}
