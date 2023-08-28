
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Build libvideo_decoder.so
define_shared_lib(
  LIBNAME video_decoder
  SRCS video_decoder.c
  PKEY 2
  UNWRAPPED_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS}
  UNWRAPPED_LIBRARY_DIRS ${FFMPEG_LIBRARY_DIRS}
  UNWRAPPED_LIBS ${FFMPEG_LIBRARIES}
)

# Build video_player
define_test(
    SRCS video_player.c
    LIBS video_decoder
    PKEY 1
    NEEDS_LD_WRAP
    NOT_IN_CHECK_IA2
    UNWRAPPED_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS}
    UNWRAPPED_LIBRARY_DIRS ${SDL2_LIBRARY_DIRS}
    UNWRAPPED_LIBS ${SDL2_LIBRARIES} ${FFMPEG_LIBRARIES}
)

define_ia2_wrapper(
  ORIGINAL_TARGETS video_player_main video_decoder)
