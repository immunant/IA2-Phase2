set(AVFORMAT_HEADERS libavformat/avformat.h libavformat/avio.h)
set(AVUTIL_HEADERS libavutil/dict.h libavutil/version.h libavutil/macros.h
    libavutil/common.h libavutil/mem.h libavutil/attributes.h libavutil/avutil.h
    libavutil/rational.h libavutil/mathematics.h libavutil/intfloat.h
    libavutil/log.h libavutil/pixfmt.h libavutil/error.h)

# Build the test
define_test(
    SRCS main.c
    WRAPPERS avformat_wrapper avutil_wrapper
)
