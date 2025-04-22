/*
 * Copyright (c) 2023 Immunant, Inc.
 */

#include <SDL.h>
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

#if IA2_LIBRARY_ONLY_MODE
INIT_RUNTIME(1);
#else
INIT_RUNTIME(2);
#endif
#define IA2_COMPARTMENT 1
#define IA2_COMPARTMENT_LIBRARIES "libSDL2-2.0.so;libswscale.so"
#include <ia2_compartment_init.inc>

static char *video_data;
static size_t video_size;
static struct video_decoder *decoder;
static int video_width IA2_SHARED_DATA;
static int video_height IA2_SHARED_DATA;
static enum AVPixelFormat video_pix_fmt IA2_SHARED_DATA;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static struct SwsContext *sws_ctx;
static uint8_t *y_plane, *u_plane, *v_plane;

/* The secrets */
static uint8_t KEY[32] = {
  0x45, 0x33, 0xCC, 0xB5, 0xEA, 0xB4, 0x10, 0xCC,
  0x03, 0x88, 0xA8, 0x3D, 0x5F, 0x93, 0x82, 0x09,
  0x19, 0xC5, 0x6F, 0xF3, 0x30, 0x7C, 0xF6, 0xF6,
  0x72, 0x42, 0x69, 0xF1, 0x9A, 0xE5, 0xE7, 0x0C,
};
static uint8_t IV[16] = {
  0xB9, 0x1D, 0x41, 0xF4, 0xEA, 0xBF, 0xB9, 0xE8,
  0x63, 0xC1, 0x6B, 0xAF, 0xE5, 0x14, 0x5C, 0x7E,
};

void init_sdl(void) {
  int ret;
  if ((ret = SDL_Init(SDL_INIT_VIDEO)) != 0) {
    errx(1, "Failed to init SDL %d: %s", ret, SDL_GetError());
  }

  window =
      SDL_CreateWindow("IA2 video player", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, video_width, video_height, 0);
  if (!window) {
    errx(1, "Failed to create window: %s", SDL_GetError());
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    errx(1, "Failed to create renderer: %s", SDL_GetError());
  }

  texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                        SDL_TEXTUREACCESS_STREAMING, video_width, video_height);
  if (!texture) {
    errx(1, "Failed to create texture: %s", SDL_GetError());
  }

  sws_ctx = sws_getContext(video_width, video_height, video_pix_fmt,
                           video_width, video_height, AV_PIX_FMT_YUV420P,
                           SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    errx(1, "Failed to create swscale context");
  }

  size_t y_plane_sz = video_width * video_height;
  size_t uv_plane_sz = y_plane_sz / 4;
  y_plane = av_malloc(y_plane_sz);
  u_plane = av_malloc(uv_plane_sz);
  v_plane = av_malloc(uv_plane_sz);
  if (!y_plane || !u_plane || !v_plane) {
    errx(1, "Failed to allocate plane memory");
  }
}

static void process_frame(AVFrame *frame, void *ctx) {
  int ret;

  int uv_pitch = video_width / 2;
  uint8_t *data[] = {
      y_plane,
      u_plane,
      v_plane,
  };
  const int linesize[] = {
      video_width,
      uv_pitch,
      uv_pitch,
  };
  sws_scale(sws_ctx, (const uint8_t **)&frame->data, frame->linesize, 0,
            video_height, data, linesize);

  ret = SDL_UpdateYUVTexture(texture, NULL, y_plane, video_width, u_plane,
                             uv_pitch, v_plane, uv_pitch);
  if (ret) {
    errx(1, "Failed to update texture");
  }

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);

  usleep(1000000UL / 30);
}

static void exit_sdl(void) {
  av_free(y_plane);
  av_free(u_plane);
  av_free(v_plane);

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

int main(int argc, const char *argv[]) {
  /*
   * Pretend to use the secrets for something.
   * TODO: do actual decryption later.
   */
  (void)KEY;
  (void)IV;

  if (argc < 2) {
    errx(1, "Usage: video_player <file>");
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

  decoder = video_decoder_init(video_data, video_size);
  if (!decoder) {
    errx(1, "Failed to initialize decoder");
  }
  video_decoder_get_info(decoder, &video_width, &video_height, &video_pix_fmt);
  printf("Video dimensions: %dx%d\n", video_width, video_height);

  init_sdl();

  if (!video_decoder_decode(decoder, NULL, process_frame)) {
    errx(1, "Failed to decode frame");
  }

  exit_sdl();
  munmap(video_data, video_size);
  close(fd);
  return 0;
}
