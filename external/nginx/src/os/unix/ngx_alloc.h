
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ALLOC_H_INCLUDED_
#define _NGX_ALLOC_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

#include <ia2.h>

IA2_BEGIN_NO_WRAP

void *ngx_alloc_ext(size_t size, ngx_log_t *log, unsigned is_shared);
void *ngx_alloc(size_t size, ngx_log_t *log);
void *ngx_shared_alloc(size_t size, ngx_log_t *log);
void *ngx_calloc_ext(size_t size, ngx_log_t *log, unsigned is_shared);
void *ngx_calloc(size_t size, ngx_log_t *log);
void *ngx_shared_calloc(size_t size, ngx_log_t *log);

#define ngx_free          free


/*
 * Linux has memalign() or posix_memalign()
 * Solaris has memalign()
 * FreeBSD 7.0 has posix_memalign(), besides, early version's malloc()
 * aligns allocations bigger than page size at the page boundary
 */

#if (NGX_HAVE_POSIX_MEMALIGN || NGX_HAVE_MEMALIGN)

void *ngx_memalign_ext(size_t alignment, size_t size, ngx_log_t *log, unsigned is_shared);
void *ngx_memalign(size_t alignment, size_t size, ngx_log_t *log);
void *ngx_shared_memalign(size_t alignment, size_t size, ngx_log_t *log);

#else

#define ngx_memalign_ext(alignment, size, log, is_shared)  ngx_alloc_ext(size, log, is_shared)
#define ngx_memalign(alignment, size, log)  ngx_alloc(size, log)
#define ngx_shared_memalign(alignment, size, log)  ngx_shared_alloc(size, log)

#endif


extern ngx_uint_t  ngx_pagesize;
extern ngx_uint_t  ngx_pagesize_shift;
extern ngx_uint_t  ngx_cacheline_size;

IA2_END_NO_WRAP

#endif /* _NGX_ALLOC_H_INCLUDED_ */
