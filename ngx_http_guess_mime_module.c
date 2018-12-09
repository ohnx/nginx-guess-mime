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
static void *ngx_http_guess_mime_create_conf(ngx_conf_t *cf);
static char *ngx_http_guess_mime_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_guess_mime_init(ngx_conf_t *cf);

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
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static ngx_int_t ngx_http_guess_mime_header_filter(ngx_http_request_t *r) {
    ngx_http_guess_mime_conf_t *gmcf;

    /* get this module's configuration (scoped to location) */
    gmcf = ngx_http_get_module_loc_conf(r, ngx_http_guess_mime_module);

    /* check if enabled */
    if (!gmcf->enable) {
        /* skip */
        return ngx_http_next_header_filter(r);
    }

    /* check to see if we've been called before */
    if (ngx_http_get_module_ctx(r, ngx_http_guess_mime_module) != NULL) {
        /* we've been called before! */
        fprintf(stderr, "ctx != NULL in header\n");
        return ngx_http_next_header_filter(r);
    }

    fprintf(stderr, "HELLO_WORLD HEADER\n");

    /* don't send headers for now */
    return NGX_OK;
}

/* hook for requests */
static ngx_int_t ngx_http_guess_mime_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    u_char *p;
    ngx_chain_t *cl;
    ngx_table_elt_t *h;
    ngx_http_guess_mime_conf_t *gmcf;

    fprintf(stderr, "HELLO_WORLD BODY\n");

    /* get this module's configuration (scoped to location) */
    gmcf = ngx_http_get_module_loc_conf(r, ngx_http_guess_mime_module);

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
               "catch request body filter not enabled");

    /* check if enabled */
    if (!gmcf->enable) {
        /* skip */
        goto done;
    }

    /* check to see if we've been called before */
    if (ngx_http_get_module_ctx(r, ngx_http_guess_mime_module) != NULL) {
        /* we've been called before! */
        fprintf(stderr, "ctx != NULL in body\n");
        goto done;
    }

    /* set the context so that we know for future requests */
    ngx_http_set_ctx(r, (void *)0x1, ngx_http_guess_mime_module);

    /* enabled. guessing mime type... */
    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
               "catch request body filter");
    for (cl = in; cl; cl = cl->next) {
        p = cl->buf->pos;
        fprintf(stderr, "buffer data:```\n%s```", p);
        for (p = cl->buf->pos; p < cl->buf->last; p++) {
            if (*p == 'X') {
                fprintf(stderr, "catch body: found\n");
                /*
                 + As we return NGX_HTTP_FORBIDDEN, the r->keepalive flag
                 + won't be reset by ngx_http_special_response_handler().
                 + Make sure to reset it to prevent processing of unread
                 + parts of the request body.
                 */
                r->keepalive = 0;
                h = ngx_list_push(&r->headers_out.headers);
                if (h == NULL) {
                    return ngx_http_filter_finalize_request(r, &ngx_http_guess_mime_module, NGX_HTTP_INTERNAL_SERVER_ERROR);
                }

                h->hash = 1;
                ngx_str_set(&h->key, "Potato-Enabled");
                ngx_str_set(&h->value, "true");
                h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
                if (h->lowcase_key == NULL) {
                    return NGX_ERROR;
                }
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);


/*                r->headers_out.status = NGX_HTTP_FORBIDDEN;*/
                ngx_http_send_header(r);
                return ngx_http_next_body_filter(r, in);/*
                r->header_sent = 0;*/
                //return ngx_http_filter_finalize_request(r, &ngx_http_guess_mime_module, NGX_HTTP_NOT_FOUND);
            }
        }
    }

    /* send headers now */
    ngx_http_send_header(r);

done:
    return ngx_http_next_body_filter(r, in);
}

/* config initalization function */
static void *ngx_http_guess_mime_create_conf(ngx_conf_t *cf) {
    ngx_http_guess_mime_conf_t *mod_conf;

    /* allocate memory for config struct */
    mod_conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_guess_mime_conf_t));
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
static char *ngx_http_guess_mime_merge_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_guess_mime_conf_t *prev = (ngx_http_guess_mime_conf_t *)parent;
    ngx_http_guess_mime_conf_t *curr = (ngx_http_guess_mime_conf_t *)child;

    /* child takes precendence over parent */
    ngx_conf_merge_value(curr->enable, prev->enable, 0);

    return NGX_CONF_OK;
}

/* initialization callback */
static ngx_int_t ngx_http_guess_mime_init(ngx_conf_t *cf) {
    /* save the original next modules */
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_next_body_filter = ngx_http_top_body_filter;

    /* insert ourselves into the list */
    ngx_http_top_header_filter = ngx_http_guess_mime_header_filter;
    ngx_http_top_body_filter = ngx_http_guess_mime_body_filter;
    fprintf(stderr, "HELLO_WORLD INITIALIZED\n. Next header: %p;Next body:%p\n", ngx_http_next_header_filter, ngx_http_next_body_filter);

    return NGX_OK;
}
