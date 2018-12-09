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
static ngx_int_t ngx_http_guess_mime_initp(ngx_cycle_t *cycle);
static void ngx_http_guess_mime_exitp(ngx_cycle_t *cycle);

/* libmagic stuff */
static magic_t ngx_http_guess_mime_magic_cookie;

/* directives provided by this module */
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
    ngx_http_guess_mime_initp,
    /* init thread callback */
    NULL,
    /* exit thread callback */
    NULL,
    /* exit process callback */
    ngx_http_guess_mime_exitp,
    /* exit master callback */
    NULL,
    /* mandatory padding */
    NGX_MODULE_V1_PADDING
};

/* pointer to next handler in the chain */
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

/* hook for requests - header (this only delays the header sending) */
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
    if (ngx_http_get_module_ctx(r, ngx_http_guess_mime_module)) {
        /* we've been called before! */
        return ngx_http_next_header_filter(r);
    }

    /* don't send headers for now since we will change them in the body filter */
    return NGX_OK;
}

/* hook for requests - body (this reads the body) */
static ngx_int_t ngx_http_guess_mime_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
    const char *lmr;
    ngx_http_guess_mime_conf_t *gmcf;
    ngx_http_core_loc_conf_t *clcf;

    /* get this module's configuration (scoped to location) */
    gmcf = ngx_http_get_module_loc_conf(r, ngx_http_guess_mime_module);

    /* check if enabled */
    if (!gmcf->enable) {
        /* skip */
        return ngx_http_next_body_filter(r, in);
    }

    /* check to see if we've been called before */
    if (ngx_http_get_module_ctx(r, ngx_http_guess_mime_module)) {
        /* we've been called before, skip doing anything interesting */
        return ngx_http_next_body_filter(r, in);
    }

    /* set the context so that we know we have been called already */
    ngx_http_set_ctx(r, (void *)0x1, ngx_http_guess_mime_module);

    /* get core configuration so we can read the default type later */
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /* try to use the default guessing */
    /* todo: configurable overwrite of content type */
    if (ngx_http_set_content_type(r) != NGX_OK) return NGX_ERROR;

    /* check if an actual guess was made instead of fallback */
    if (r->headers_out.content_type.data != clcf->default_type.data) {
        /* guess made, use it */
        goto done;
    }

    /* bad guess, run libmagic */
    /*
     * notice here that we only run libmagic on the first buffer.
     * this is an intentional choice, since i don't want to use up all of the memory
     * for larger files (libmagic needs a single contiguous buffer)
     */
    /* TODO: configurable whether or not to read the entire buffer */
    lmr = magic_buffer(ngx_http_guess_mime_magic_cookie, in->buf->pos, in->buf->last - in->buf->pos);

    if (!lmr) {
        /* error running libmagic */
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0, "Failed to determine MIME type: %s\n", magic_error(ngx_http_guess_mime_magic_cookie));
        goto done;
    }

    /* overwrite the old, bad, content-type */
    r->headers_out.content_type_len = strlen(lmr);
    r->headers_out.content_type.data = (u_char *)lmr;
    r->headers_out.content_type.len = r->headers_out.content_type_len;

    /* TODO: mime encoding with libmagic *//*
    h->hash = 1;
    ngx_str_set(&h->key, "Content-Encoding");
    ngx_str_set(&h->value, "whatever");
    r->headers_out.content_encoding = h;*/

done:
    /* send headers now */
    ngx_http_send_header(r);

    /* move to next body filter */
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

    return NGX_OK;
}

/* process initialization callback */
static ngx_int_t ngx_http_guess_mime_initp(ngx_cycle_t *cycle) {
    /* initialize libmagic */
    /* note that we do this per-process because libmagic is not threadsafe. */
    ngx_http_guess_mime_magic_cookie = magic_open(MAGIC_MIME_TYPE);

    /* check libmagic initialization status */
    if (!ngx_http_guess_mime_magic_cookie) {
        ngx_log_stderr(0, "unable to initialize libmagic\n");
        return NGX_ERROR;
    }

    /* try to load the magic */
    if (magic_load(ngx_http_guess_mime_magic_cookie, NULL)) {
        ngx_log_stderr(0, "cannot load libmagic database: %s\n", magic_error(ngx_http_guess_mime_magic_cookie));
        magic_close(ngx_http_guess_mime_magic_cookie);
        return NGX_ERROR;
    }

    return NGX_OK;
}

static void ngx_http_guess_mime_exitp(ngx_cycle_t *cycle) {
    /* close libmagic handle */
    magic_close(ngx_http_guess_mime_magic_cookie);
}
