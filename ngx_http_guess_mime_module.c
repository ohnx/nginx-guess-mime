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
    ngx_http_guess_mime_initp,
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

/* hook for requests */
static ngx_int_t ngx_http_guess_mime_handler(ngx_http_request_t *r, ngx_chain_t *in) {
    u_char                          *p;
    size_t                          root;
    ngx_str_t                       path;
    ngx_int_t                       rc;
    ngx_uint_t                      level;
    ngx_log_t                       *log;
    ngx_buf_t                       *b;
    ngx_chain_t                     out;
    ngx_table_elt_t                 *h;
    ngx_open_file_info_t            of;
    ngx_http_guess_mime_conf_t      *gmcf;
    ngx_http_core_loc_conf_t        *clcf;

    /* get this module's configuration (scoped to location) */
    gmcf = ngx_http_get_module_loc_conf(r, ngx_http_guess_mime_module);
    log = r->connection->log;

    fprintf(stderr, "r equal? %s\tin = %p\n", r == r->main ? "yes" : "no", in);
    fprintf(stderr, "HELLO_WORLD\n");

    /* check if enabled */
    if (!gmcf->enable) {
        /* skip */
        return NGX_DECLINED;
    }

    /* check if serving directory */
    if (r->uri.data[r->uri.len - 1] == '/') {
        return NGX_DECLINED;
    }

    /* check if not GET/HEAD */
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    fprintf(stderr, "USE_WORLD\n");
    if (in) {
        return NGX_AGAIN;
    }

    /* ok, we have confirmed that we want magic to happen */

    /* get the correct corresponding file path */
    p = ngx_http_map_uri_to_path(r, &path, &root, 0);
    if (p == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* fetch location conf */
    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    /* clear open file handle */
    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    /* options for file handle */
    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    /* try reading file */
    if (ngx_open_cached_file(clcf->open_file_cache, &path, &of, r->pool) != NGX_OK) {
        /* there was an error, map the correct error codes */
        switch (of.err) {
        case 0:
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        case NGX_ENOENT:
        case NGX_ENOTDIR:
        case NGX_ENAMETOOLONG:
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;
            break;
        case NGX_EACCES:
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;
            break;
        default:
            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
            break;
        }

        if (rc != NGX_HTTP_NOT_FOUND || clcf->log_not_found) {
            ngx_log_error(level, log, of.err, "%s \"%s\" failed", of.failed, path.data);
        }

        return rc;
    }

    /* another issue could be that it's not a file */
    if (!of.is_file) {
        if (ngx_close_file(of.fd) == NGX_FILE_ERROR) {
            ngx_log_error(NGX_LOG_ALERT, log, ngx_errno,
                          ngx_close_file_n " \"%s\" failed", path.data);
        }

        return NGX_DECLINED;
    }

    /* do something ?? */
    r->root_tested = !r->error_page;

    /* discard request body */
    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        return rc;
    }

    /* update log action */
    log->action = "sending response to client";

    /* set corresponding headers (don't send until we have secured the buffers) */
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = of.size;
    r->headers_out.last_modified_time = of.mtime;
    r->headers_out.content_type_len = clcf->default_type.len;
    r->headers_out.content_type = clcf->default_type;

    /* push a custom header for testing */
    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    h->hash = 1;
    ngx_str_set(&h->key, "Potato-Enabled");
    ngx_str_set(&h->value, "true");
    h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }
    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    /* set ETag header */
    if (ngx_http_set_etag(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* allocate space for buffer */
    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* allocate space to store the file info */
    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* now we can send the headers */
    rc = ngx_http_send_header(r);

    /* return error if failed to send errors, or OK if only headers sent */
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    /* range of file to read */
    b->file_pos = 0;
    b->file_last = of.size;

    /* more properties */
    b->in_file = b->file_last ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    /* more properties */
    b->file->fd = of.fd;
    b->file->name = path;
    b->file->log = log;
    b->file->directio = of.is_directio;

    /* output only this buffer */
    out.buf = b;
    out.next = NULL;

    /* output this buffer */
    return ngx_http_output_filter(r, &out);
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
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;

    /* get main module config file */
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    /* make room for a pointer to our handler */
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    /* install our handler */
    *h = (ngx_http_handler_pt)ngx_http_guess_mime_handler;

    return NGX_OK;
}

/* process initialization callback */
static ngx_int_t ngx_http_guess_mime_initp(ngx_cycle_t *cycle) {

    return NGX_OK;
}
