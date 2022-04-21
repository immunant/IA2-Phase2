
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    /* the round robin data must be first */
    ngx_http_upstream_rr_peer_data_t   rrp;

    ngx_uint_t                         hash;

    u_char                             addrlen;
    u_char                            *addr;

    u_char                             tries;

    ngx_event_get_peer_pt              get_rr_peer;
} ngx_http_upstream_ip_hash_peer_data_t;


static ngx_int_t ngx_http_upstream_init_ip_hash_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static ngx_int_t ngx_http_upstream_get_ip_hash_peer(ngx_peer_connection_t *pc,
    void *data);
static char *ngx_http_upstream_ip_hash(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

IA2_DEFINE_WRAPPER(ngx_http_upstream_ip_hash, _ZTSPFPcP10ngx_conf_sP13ngx_command_sPvE, 1);

static ngx_command_t  ngx_http_upstream_ip_hash_commands[] = {

    { ngx_string("ip_hash"),
      NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS,
      IA2_WRAPPER(ngx_http_upstream_ip_hash, 1),
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_ip_hash_module_ctx = {
    IA2_NULL_FNPTR,                                  /* preconfiguration */
    IA2_NULL_FNPTR,                                  /* postconfiguration */

    IA2_NULL_FNPTR,                                  /* create main configuration */
    IA2_NULL_FNPTR,                                  /* init main configuration */

    IA2_NULL_FNPTR,                                  /* create server configuration */
    IA2_NULL_FNPTR,                                  /* merge server configuration */

    IA2_NULL_FNPTR,                                  /* create location configuration */
    IA2_NULL_FNPTR                                   /* merge location configuration */
};


ngx_module_t  ngx_http_upstream_ip_hash_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_ip_hash_module_ctx, /* module context */
    ngx_http_upstream_ip_hash_commands,    /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    IA2_NULL_FNPTR,                                  /* init master */
    IA2_NULL_FNPTR,                                  /* init module */
    IA2_NULL_FNPTR,                                  /* init process */
    IA2_NULL_FNPTR,                                  /* init thread */
    IA2_NULL_FNPTR,                                  /* exit thread */
    IA2_NULL_FNPTR,                                  /* exit process */
    IA2_NULL_FNPTR,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static u_char ngx_http_upstream_ip_hash_pseudo_addr[3];


static ngx_int_t
ngx_http_upstream_init_ip_hash(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
    if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    us->peer.init = IA2_DEFINE_WRAPPER_FN_SCOPE(ngx_http_upstream_init_ip_hash_peer, _ZTSPFlP18ngx_http_request_sP28ngx_http_upstream_srv_conf_sE, 1);

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_init_ip_hash_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    struct sockaddr_in                     *sin;
#if (NGX_HAVE_INET6)
    struct sockaddr_in6                    *sin6;
#endif
    ngx_http_upstream_ip_hash_peer_data_t  *iphp;

    iphp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_ip_hash_peer_data_t));
    if (iphp == NULL) {
        return NGX_ERROR;
    }

    r->upstream->peer.data = &iphp->rrp;

    if (ngx_http_upstream_init_round_robin_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    r->upstream->peer.get = IA2_DEFINE_WRAPPER_FN_SCOPE(ngx_http_upstream_get_ip_hash_peer, _ZTSPFlP21ngx_peer_connection_sPvE, 1);

    switch (r->connection->sockaddr->sa_family) {

    case AF_INET:
        sin = (struct sockaddr_in *) r->connection->sockaddr;
        iphp->addr = (u_char *) &sin->sin_addr.s_addr;
        iphp->addrlen = 3;
        break;

#if (NGX_HAVE_INET6)
    case AF_INET6:
        sin6 = (struct sockaddr_in6 *) r->connection->sockaddr;
        iphp->addr = (u_char *) &sin6->sin6_addr.s6_addr;
        iphp->addrlen = 16;
        break;
#endif

    default:
        iphp->addr = ngx_http_upstream_ip_hash_pseudo_addr;
        iphp->addrlen = 3;
    }

    iphp->hash = 89;
    iphp->tries = 0;
    iphp->get_rr_peer = IA2_DECLARE_WRAPPER_FN_SCOPE(ngx_http_upstream_get_round_robin_peer, _ZTSPFlP21ngx_peer_connection_sPvE, 1);

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_get_ip_hash_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_ip_hash_peer_data_t  *iphp = data;

    time_t                        now;
    ngx_int_t                     w;
    uintptr_t                     m;
    ngx_uint_t                    i, n, p, hash;
    ngx_http_upstream_rr_peer_t  *peer;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get ip hash peer, try: %ui", pc->tries);

    /* TODO: cached */

    ngx_http_upstream_rr_peers_rlock(iphp->rrp.peers);

    if (iphp->tries > 20 || iphp->rrp.peers->single) {
        ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);
        return IA2_CALL(iphp->get_rr_peer, _ZTSPFlP21ngx_peer_connection_sPvE, 1)(pc, &iphp->rrp);
    }

    now = ngx_time();

    pc->cached = 0;
    pc->connection = NULL;

    hash = iphp->hash;

    for ( ;; ) {

        for (i = 0; i < (ngx_uint_t) iphp->addrlen; i++) {
            hash = (hash * 113 + iphp->addr[i]) % 6271;
        }

        w = hash % iphp->rrp.peers->total_weight;
        peer = iphp->rrp.peers->peer;
        p = 0;

        while (w >= peer->weight) {
            w -= peer->weight;
            peer = peer->next;
            p++;
        }

        n = p / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

        if (iphp->rrp.tried[n] & m) {
            goto next;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "get ip hash peer, hash: %ui %04XL", p, (uint64_t) m);

        ngx_http_upstream_rr_peer_lock(iphp->rrp.peers, peer);

        if (peer->down) {
            ngx_http_upstream_rr_peer_unlock(iphp->rrp.peers, peer);
            goto next;
        }

        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)
        {
            ngx_http_upstream_rr_peer_unlock(iphp->rrp.peers, peer);
            goto next;
        }

        if (peer->max_conns && peer->conns >= peer->max_conns) {
            ngx_http_upstream_rr_peer_unlock(iphp->rrp.peers, peer);
            goto next;
        }

        break;

    next:

        if (++iphp->tries > 20) {
            ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);
            return IA2_CALL(iphp->get_rr_peer, _ZTSPFlP21ngx_peer_connection_sPvE, 1)(pc, &iphp->rrp);
        }
    }

    iphp->rrp.current = peer;

    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

    peer->conns++;

    if (now - peer->checked > peer->fail_timeout) {
        peer->checked = now;
    }

    ngx_http_upstream_rr_peer_unlock(iphp->rrp.peers, peer);
    ngx_http_upstream_rr_peers_unlock(iphp->rrp.peers);

    iphp->rrp.tried[n] |= m;
    iphp->hash = hash;

    return NGX_OK;
}


static char *
ngx_http_upstream_ip_hash(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t  *uscf;

    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    if (!IA2_FNPTR_IS_NULL(uscf->peer.init_upstream)) {
        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "load balancing method redefined");
    }

    uscf->peer.init_upstream = IA2_DEFINE_WRAPPER_FN_SCOPE(ngx_http_upstream_init_ip_hash, _ZTSPFlP10ngx_conf_sP28ngx_http_upstream_srv_conf_sE, 1);

    uscf->flags = NGX_HTTP_UPSTREAM_CREATE
                  |NGX_HTTP_UPSTREAM_WEIGHT
                  |NGX_HTTP_UPSTREAM_MAX_CONNS
                  |NGX_HTTP_UPSTREAM_MAX_FAILS
                  |NGX_HTTP_UPSTREAM_FAIL_TIMEOUT
                  |NGX_HTTP_UPSTREAM_DOWN;

    return NGX_CONF_OK;
}
