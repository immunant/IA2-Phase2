include(CheckIncludeFile)

function(gather_ffmpeg_headers)
  set(outputVar ${ARGV0})
  set(libName ${ARGV1})

  find_path(headersPath ${libName})
  file(GLOB headerList ${headersPath}/${libName}/*)

  # Make sure that we can actually compile with each header, and remove the ones
  # that we can't use in this configuration
  foreach(header ${headerList})
    CHECK_INCLUDE_FILE(${header} ${header}-valid)
    if(NOT ${header}-valid)
      list(REMOVE_ITEM headerList ${header})
    endif()
  endforeach()

  set(${outputVar} ${headerList} PARENT_SCOPE)
endfunction()

# ffmpeg system library wrappers
gather_ffmpeg_headers(AVFORMAT_HEADERS libavformat)
define_ia2_wrapper(
    WRAPPER avformat_wrapper
    WRAPPED_LIB avformat
    HEADERS ${AVFORMAT_HEADERS}
    OUTPUT_DIR libavformat
    USE_SYSTEM_HEADERS
    CALLER_PKEY 1
)
target_include_directories(avformat_wrapper
  BEFORE PUBLIC ${IA2_INCLUDE_DIR}
  INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
)

gather_ffmpeg_headers(AVUTIL_HEADERS libavutil)
define_ia2_wrapper(
    WRAPPER avutil_wrapper
    WRAPPED_LIB avutil
    HEADERS ${AVUTIL_HEADERS}
    OUTPUT_DIR libavutil
    USE_SYSTEM_HEADERS
    CALLER_PKEY 1
)
target_include_directories(avutil_wrapper
  BEFORE PUBLIC ${IA2_INCLUDE_DIR}
  INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
)

gather_ffmpeg_headers(AVCODEC_HEADERS libavcodec)
define_ia2_wrapper(
    WRAPPER avcodec_wrapper
    WRAPPED_LIB avcodec
    HEADERS ${AVCODEC_HEADERS}
    OUTPUT_DIR libavcodec
    USE_SYSTEM_HEADERS
    CALLER_PKEY 1
)
target_include_directories(avcodec_wrapper
  BEFORE PUBLIC ${IA2_INCLUDE_DIR}
  INTERFACE ${CMAKE_CURRENT_BINARY_DIR}
)
