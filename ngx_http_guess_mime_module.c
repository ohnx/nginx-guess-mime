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

static u_char ngx_hello_world[] = "Hello, world!";

/* hook for requests */
static ngx_int_t ngx_http_guess_mime_handler(ngx_http_request_t *r, ngx_chain_t *in) {
    ngx_http_guess_mime_conf_t *mod_conf;

    ngx_buf_t *b;
    ngx_chain_t out;

    /* get this module's configuration (scoped to location) */
    mod_conf = ngx_http_get_module_loc_conf(r, ngx_http_guess_mime_module);

    fprintf(stderr, "HELLO_WORLD\n");

    /* check if enabled */
    if (!mod_conf->enable) {
        fprintf(stderr, "IGNORE_WORLD\n");
        /* skip */
        return NGX_DECLINED;
    }

    /* Set the Content-Type header. */
    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char *) "text/plain";

    /* Allocate a new buffer for sending out the reply. */
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));

    /* Insertion in the buffer chain. */
    out.buf = b;
    out.next = NULL; /* just one buffer */

    b->pos = ngx_hello_world; /* first position in memory of the data */
    b->last = ngx_hello_world + sizeof(ngx_hello_world); /* last position in memory of the data */
    b->memory = 1; /* content is in read-only memory */
    b->last_buf = 1; /* there will be no more buffers in the request */

    /* Sending the headers for the reply. */
    r->headers_out.status = NGX_HTTP_OK; /* 200 status code */
    /* Get the content length of the body. */
    r->headers_out.content_length_n = sizeof(ngx_hello_world);
    ngx_http_send_header(r); /* Send the headers */

    /* Send the body, and return the status code of the output filter chain. */
    return ngx_http_output_filter(r, &out);
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

    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(conf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = (ngx_http_handler_pt)ngx_http_guess_mime_handler;

    return NGX_OK;
}
