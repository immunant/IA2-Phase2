
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static char *ngx_http_empty_gif(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

IA2_DEFINE_WRAPPER(ngx_http_empty_gif, _ZTSPFPcP10ngx_conf_sP13ngx_command_sPvE, 1);

static ngx_command_t  ngx_http_empty_gif_commands[] = {

    { ngx_string("empty_gif"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      IA2_WRAPPER(ngx_http_empty_gif, 1),
      0,
      0,
      NULL },

      ngx_null_command
};


/* the minimal single pixel transparent GIF, 43 bytes */

static u_char  ngx_empty_gif[] = {

    'G', 'I', 'F', '8', '9', 'a',  /* header                                 */

                                   /* logical screen descriptor              */
    0x01, 0x00,                    /* logical screen width                   */
    0x01, 0x00,                    /* logical screen height                  */
    0x80,                          /* global 1-bit color table               */
    0x01,                          /* background color #1                    */
    0x00,                          /* no aspect ratio                        */

                                   /* global color table                     */
    0x00, 0x00, 0x00,              /* #0: black                              */
    0xff, 0xff, 0xff,              /* #1: white                              */

                                   /* graphic control extension              */
    0x21,                          /* extension introducer                   */
    0xf9,                          /* graphic control label                  */
    0x04,                          /* block size                             */
    0x01,                          /* transparent color is given,            */
                                   /*     no disposal specified,             */
                                   /*     user input is not expected         */
    0x00, 0x00,                    /* delay time                             */
    0x01,                          /* transparent color #1                   */
    0x00,                          /* block terminator                       */

                                   /* image descriptor                       */
    0x2c,                          /* image separator                        */
    0x00, 0x00,                    /* image left position                    */
    0x00, 0x00,                    /* image top position                     */
    0x01, 0x00,                    /* image width                            */
    0x01, 0x00,                    /* image height                           */
    0x00,                          /* no local color table, no interlaced    */

                                   /* table based image data                 */
    0x02,                          /* LZW minimum code size,                 */
                                   /*     must be at least 2-bit             */
    0x02,                          /* block size                             */
    0x4c, 0x01,                    /* compressed bytes 01_001_100, 0000000_1 */
                                   /* 100: clear code                        */
                                   /* 001: 1                                 */
                                   /* 101: end of information code           */
    0x00,                          /* block terminator                       */

    0x3B                           /* trailer                                */
};


static ngx_http_module_t  ngx_http_empty_gif_module_ctx = {
    IA2_NULL_FNPTR,                          /* preconfiguration */
    IA2_NULL_FNPTR,                          /* postconfiguration */

    IA2_NULL_FNPTR,                          /* create main configuration */
    IA2_NULL_FNPTR,                          /* init main configuration */

    IA2_NULL_FNPTR,                          /* create server configuration */
    IA2_NULL_FNPTR,                          /* merge server configuration */

    IA2_NULL_FNPTR,                          /* create location configuration */
    IA2_NULL_FNPTR                           /* merge location configuration */
};


ngx_module_t  ngx_http_empty_gif_module = {
    NGX_MODULE_V1,
    &ngx_http_empty_gif_module_ctx, /* module context */
    ngx_http_empty_gif_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    IA2_NULL_FNPTR,                          /* init master */
    IA2_NULL_FNPTR,                          /* init module */
    IA2_NULL_FNPTR,                          /* init process */
    IA2_NULL_FNPTR,                          /* init thread */
    IA2_NULL_FNPTR,                          /* exit thread */
    IA2_NULL_FNPTR,                          /* exit process */
    IA2_NULL_FNPTR,                          /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_str_t  ngx_http_gif_type = ngx_string("image/gif");


static ngx_int_t
ngx_http_empty_gif_handler(ngx_http_request_t *r)
{
    ngx_http_complex_value_t  cv;

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));

    cv.value.len = sizeof(ngx_empty_gif);
    cv.value.data = ngx_empty_gif;
    r->headers_out.last_modified_time = 23349600;

    return ngx_http_send_response(r, NGX_HTTP_OK, &ngx_http_gif_type, &cv);
}


static char *
ngx_http_empty_gif(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = IA2_DEFINE_WRAPPER_FN_SCOPE(ngx_http_empty_gif_handler, _ZTSPFlP18ngx_http_request_sE, 1);

    return NGX_CONF_OK;
}
