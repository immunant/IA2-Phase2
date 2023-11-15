
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Build libvideo_decoder.so
add_ia2_compartment(video_decoder LIBRARY
  SOURCES video_decoder.c
  PKEY 2
  INCLUDE_DIRECTORIES include
  LIBRARIES ${FFMPEG_LIBRARIES}
)

# Build video_player
add_ia2_compartment(video_player EXECUTABLE
  SOURCES video_player.c
  LIBRARIES video_decoder ${SDL2_LIBRARIES} ${FFMPEG_LIBRARIES}
  PKEY 1
  INCLUDE_DIRECTORIES include ${SDL2_INCLUDE_DIRS}
)

