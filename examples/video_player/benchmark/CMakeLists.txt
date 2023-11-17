pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Build video decoder library. We have to build a second copy of this because we
# can't shared shared libraries across multiple compartmentalized executables
# yet.
add_ia2_compartment(video_decoder_benchmark_lib LIBRARY
  SOURCES video_decoder.c
  PKEY 2
  INCLUDE_DIRECTORIES include
  LIBRARIES ${FFMPEG_LIBRARIES}
)

add_ia2_compartment(video_decoder_benchmark EXECUTABLE
  SOURCES benchmark.c
  LIBRARIES video_decoder_benchmark_lib ${SDL2_LIBRARIES} ${FFMPEG_LIBRARIES}
  PKEY 1
  INCLUDE_DIRECTORIES include ${SDL2_INCLUDE_DIRS}
)


# Non-compartmentalized baseline
add_library(libvideo_decoder_benchmark_baseline SHARED video_decoder.c)
target_include_directories(libvideo_decoder_benchmark_baseline PUBLIC include $<TARGET_PROPERTY:libia2,INCLUDE_DIRECTORIES>)
target_link_libraries(libvideo_decoder_benchmark_baseline ${FFMPEG_LIBRARIES})

add_executable(video_decoder_benchmark_baseline benchmark.c)
target_link_libraries(video_decoder_benchmark_baseline ${SDL2_LIBRARIES} ${FFMPEG_LIBRARIES})
target_link_libraries(video_decoder_benchmark_baseline libvideo_decoder_benchmark_baseline)
