# ffmpeg system library wrappers
set(AVFORMAT_HEADERS libavformat/avformat.h libavformat/avio.h)
set(AVUTIL_HEADERS libavutil/dict.h libavutil/version.h libavutil/macros.h
    libavutil/common.h libavutil/mem.h libavutil/attributes.h libavutil/avutil.h
    libavutil/rational.h libavutil/mathematics.h libavutil/intfloat.h
    libavutil/log.h libavutil/pixfmt.h libavutil/error.h)

define_ia2_wrapper(
    WRAPPER avformat_wrapper
    WRAPPED_LIB avformat
    HEADERS ${AVFORMAT_HEADERS}
    USE_SYSTEM_HEADERS
    CALLER_PKEY 1
)

define_ia2_wrapper(
    WRAPPER avutil_wrapper
    WRAPPED_LIB avutil
    HEADERS ${AVUTIL_HEADERS}
    USE_SYSTEM_HEADERS
    CALLER_PKEY 1
)
