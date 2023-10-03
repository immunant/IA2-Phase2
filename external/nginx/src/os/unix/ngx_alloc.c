
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ia2.h>
#include <ia2_allocator.h>


ngx_uint_t  ngx_pagesize IA2_SHARED_DATA;
ngx_uint_t  ngx_pagesize_shift IA2_SHARED_DATA;
ngx_uint_t  ngx_cacheline_size IA2_SHARED_DATA;


void *
ngx_alloc_ext(size_t size, ngx_log_t *log, unsigned is_shared)
{
    void  *p;

    if (is_shared) {
        p = shared_malloc(size);
    } else {
        p = malloc(size);
    }
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "malloc(%uz) failed", size);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, log, 0, "malloc: %p:%uz", p, size);

    return p;
}

void *
ngx_alloc(size_t size, ngx_log_t *log)
{
    return ngx_alloc_ext(size, log, 0);
}

void *
ngx_shared_alloc(size_t size, ngx_log_t *log)
{
    return ngx_alloc_ext(size, log, 1);
}

void *
ngx_calloc_ext(size_t size, ngx_log_t *log, unsigned is_shared)
{
    void  *p;

    p = ngx_alloc_ext(size, log, is_shared);

    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}

void *
ngx_calloc(size_t size, ngx_log_t *log)
{
    return ngx_calloc_ext(size, log, 0);
}

void *
ngx_shared_calloc(size_t size, ngx_log_t *log)
{
    return ngx_calloc_ext(size, log, 1);
}


#if (NGX_HAVE_POSIX_MEMALIGN)

void *
ngx_memalign_ext(size_t alignment, size_t size, ngx_log_t *log, unsigned is_shared)
{
    void  *p;
    int    err;

    if (is_shared) {
        err = shared_posix_memalign(&p, alignment, size);
    } else {
        err = posix_memalign(&p, alignment, size);
    }

    if (err) {
        ngx_log_error(NGX_LOG_EMERG, log, err,
                      "posix_memalign(%uz, %uz) failed", alignment, size);
        p = NULL;
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0,
                   "posix_memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#elif (NGX_HAVE_MEMALIGN)

void *
ngx_memalign_ext(size_t alignment, size_t size, ngx_log_t *log, unsigned is_shared)
{
    void  *p;

    if (is_shared) {
        p = shared_memalign(alignment, size);
    } else {
        p = memalign(alignment, size);
    }
    if (p == NULL) {
        ngx_log_error(NGX_LOG_EMERG, log, ngx_errno,
                      "memalign(%uz, %uz) failed", alignment, size);
    }

    ngx_log_debug3(NGX_LOG_DEBUG_ALLOC, log, 0,
                   "memalign: %p:%uz @%uz", p, size, alignment);

    return p;
}

#endif

void *
ngx_memalign(size_t alignment, size_t size, ngx_log_t *log)
{
    return ngx_memalign_ext(alignment, size, log, 0);
}

void *
ngx_shared_memalign(size_t alignment, size_t size, ngx_log_t *log)
{
    return ngx_memalign_ext(alignment, size, log, 1);
}
