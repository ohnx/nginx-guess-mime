/* libmagic */
#include <magic.h>

/* nginx */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* config options */
typedef struct {
    ngx_flag_t      enable;
} ngx_http_guess_mime_conf_t;

/* function definitions for later on */
static void *ngx_http_guess_mime_create_conf(ngx_conf_t *conf);
static char *ngx_http_guess_mime_merge_conf(ngx_conf_t *conf, void *parent, void *child);
static ngx_int_t ngx_http_guess_mime_init(ngx_conf_t *conf);

/* provided directives */
static ngx_command_t ngx_http_guess_mime_commands[] = {
    {
        /* directive string */
        ngx_string("guess_mime"), /* directive string */
        /* can be used anywhere, and has a single flag of enable/disable */
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
        /* simple set on/off */
        ngx_conf_set_flag_slot,
        /* flags apply to each location */
        NGX_HTTP_LOC_CONF_OFFSET,
        /* memory address offset to read/write from/to */
        offsetof(ngx_http_guess_mime_conf_t, enable),
        NULL
    },
    /* no more directives */
    ngx_null_command
};

/* set up contexts and callbacks */
static ngx_http_module_t ngx_http_guess_mime_module_ctx = {
    /* preconfiguration */
    NULL,
    /* postconfiguration */
    ngx_http_guess_mime_init,
    /* create main configuration */
    NULL,
    /* init main configuration */
    NULL,
    /* create server configuration */
    NULL,
    /* merge server configuration */
    NULL,
    /* create location configuration */
    ngx_http_guess_mime_create_conf,
    /* merge location configuration */
    ngx_http_guess_mime_merge_conf
};

/* main module exported to nginx */
ngx_module_t ngx_http_guess_mime_module = {
    /* mandatory padding */
    NGX_MODULE_V1,
    /* context */
    &ngx_http_guess_mime_module_ctx,
    /* directives used */
    ngx_http_guess_mime_commands,
    /* module type */
    NGX_HTTP_MODULE,
    /* init master callback */
    NULL,
    /* init module callback */
    NULL,
    /* init process callback */
    NULL,
    /* init thread callback */
    NULL,
    /* exit thread callback */
    NULL,
    /* exit process callback */
    NULL,
    /* exit master callback */
    NULL,
    /* mandatory padding */
    NGX_MODULE_V1_PADDING
};

/* pointer to next handler in the chain */
static ngx_http_request_body_filter_pt ngx_http_next_request_body_filter;

/* hook for requests */
static ngx_int_t ngx_http_guess_mime_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    u_char *p;
    ngx_chain_t *cl;
    ngx_http_guess_mime_conf_t *mod_conf;

    /* get this module's configuration (scoped to location) */
    mod_conf = ngx_http_get_module_loc_conf(r, ngx_http_guess_mime_module);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
               "catch request body filter not enabled");
    exit(-1);
    return NGX_HTTP_FORBIDDEN;

    /* check if enabled */
    if (!mod_conf->enable) {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
               "catch request body filter not enabled");
        /* skip */
        goto done;
    }

#if 0
r->headers_out.status = NGX_HTTP_OK;
r->headers_out.content_length_n = 100;
r->headers_out.content_type.len = sizeof("image/gif") - 1;
r->headers_out.content_type.data = (u_char *) "image/gif";
ngx_http_send_header(r);

#endif

    /* enabled. guessing mime type... */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
               "catch request body filter");
    for (cl = in; cl; cl = cl->next) {
        p = cl->buf->pos;
        for (p = cl->buf->pos; p < cl->buf->last; p++) {
            ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "catch body in:%02Xd:%c", *p, *p);
            if (*p == 'X') {
                ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                               "catch body: found");
                /*
                 + As we return NGX_HTTP_FORBIDDEN, the r->keepalive flag
                 + won't be reset by ngx_http_special_response_handler().
                 + Make sure to reset it to prevent processing of unread
                 + parts of the request body.
                 */
                r->keepalive = 0;
                return NGX_HTTP_FORBIDDEN;
            }
        }
    }

done:
    return ngx_http_next_request_body_filter(r, in);
}

/* config initalization function */
static void *ngx_http_guess_mime_create_conf(ngx_conf_t *conf) {
    ngx_http_guess_mime_conf_t *mod_conf;

    /* allocate memory for config struct */
    mod_conf = ngx_pcalloc(conf->pool, sizeof(ngx_http_guess_mime_conf_t));
    if (mod_conf == NULL) {
        /* OOM */
        return NULL;
    }

    /* unknown value right now */
    mod_conf->enable = NGX_CONF_UNSET;

    /* return pointer */
    return mod_conf;
}

/* let child blocks set guess_mime even if the parent has it set */
static char *ngx_http_guess_mime_merge_conf(ngx_conf_t *conf, void *parent, void *child) {
    ngx_http_guess_mime_conf_t *prev = (ngx_http_guess_mime_conf_t *)parent;
    ngx_http_guess_mime_conf_t *curr = (ngx_http_guess_mime_conf_t *)child;

    /* child takes precendence over parent */
    ngx_conf_merge_value(curr->enable, prev->enable, 0);

    return NGX_CONF_OK;
}

/* initialization callback */
static ngx_int_t ngx_http_guess_mime_init(ngx_conf_t *conf) {
    /* save the original next module */
    ngx_http_next_request_body_filter = ngx_http_top_request_body_filter;

    /* insert ourselves into the list */
    ngx_http_top_request_body_filter = ngx_http_guess_mime_filter;

    return NGX_OK;
}
