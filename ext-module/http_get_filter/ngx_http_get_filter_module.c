#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_str_t subrequest_uri;
} ngx_http_get_filter_loc_conf_t;

typedef struct
{
    ngx_str_t stock[6];
    ngx_str_t response_str;
} ngx_http_http_get_filter_ctx_t;


static char *
ngx_http_http_get_filter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_http_http_get_filter_handler(ngx_http_request_t *r);
static ngx_int_t http_get_filter_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc);
static void http_get_filter_post_handler(ngx_http_request_t * r);

static void *ngx_http_dns_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_dns_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);



static ngx_command_t  ngx_http_http_get_filter_commands[] =
{

    {
        ngx_string("http_get_filter"),
            NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_TAKE1,
            ngx_http_http_get_filter,
            NGX_HTTP_LOC_CONF_OFFSET,
            offsetof(ngx_http_get_filter_loc_conf_t, subrequest_uri),
            NULL
    },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_get_filter_module_ctx =
{
    NULL,                              /* preconfiguration */
    NULL,                  		/* postconfiguration */

    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */

    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */

    ngx_http_dns_create_loc_conf,
    ngx_http_dns_merge_loc_conf,
};

ngx_module_t  ngx_http_get_filter_module =
{
    NGX_MODULE_V1,
    &ngx_http_get_filter_module_ctx,           /* module context */
    ngx_http_http_get_filter_commands,              /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_http_dns_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_get_filter_loc_conf_t *conf;
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_get_filter_loc_conf_t));
    if (conf == NULL)
    {
        return NULL;
    }
    
    return conf;
}
static char *
ngx_http_dns_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_get_filter_loc_conf_t *prev = parent;
    ngx_http_get_filter_loc_conf_t *conf = child;
    ngx_conf_merge_str_value(conf->subrequest_uri, prev->subrequest_uri,
        "http_get_filter");
    return NGX_CONF_OK;
}

static ngx_int_t http_get_filter_subrequest_post_handler(ngx_http_request_t *r,
                                                         void *data, ngx_int_t rc)
{
    //��ǰ����r������������parent��Ա��ָ������
    ngx_http_request_t          *pr;

    //ע�⣬�������Ǳ����ڸ������еģ��μ�5.6.5�ڣ�������Ҫ��pr��ȡ�����ġ�
    //��ʵ�и��򵥵ķ�����������data���������ģ���ʼ��subrequestʱ
    //���ǾͶ�����������˵ģ������Ϊ��˵����λ�ȡ���������������
    ngx_http_http_get_filter_ctx_t* myctx;

    pr = r->parent;

    myctx = ngx_http_get_module_ctx(pr, ngx_http_get_filter_module);

    pr->headers_out.status = r->headers_out.status;

    printf("%s:%d body=[%s]\n", __FILE__, __LINE__, (const char*)r->upstream->buffer.pos);

    size_t bodylen = r->upstream->buffer.last - r->upstream->buffer.pos;
    myctx->response_str.data = ngx_palloc(r->pool, bodylen);
    myctx->response_str.len  = bodylen;
    memcpy(myctx->response_str.data, r->upstream->buffer.pos, bodylen);
//
//    //�������NGX_HTTP_OK��Ҳ����200����ζ�ŷ������˷������ɹ������Ž�
//    //��ʼ����http����
//    if (r->headers_out.status == NGX_HTTP_OK)
//    {
//        flag = 0;
//
//        //�ڲ�ת����Ӧʱ��buffer�лᱣ�������η���������Ӧ���ر�����ʹ��
//        //�������ģ��������η�����ʱ�������ʹ��upstream����ʱû���ض���
//        //input_filter������upstream����Ĭ�ϵ�input_filter��������ͼ
//        //�����е�������Ӧȫ�����浽buffer��������
//        pRecvBuf = &r->upstream->buffer;
//
//        //���¿�ʼ�������η���������Ӧ��������������ֵ���������Ľṹ��
//        //myctx->stock������
//        for (; pRecvBuf->pos != pRecvBuf->last; pRecvBuf->pos++)
//        {
//            if (*pRecvBuf->pos == ',' || *pRecvBuf->pos == '\"')
//            {
//                if (flag > 0)
//                {
//                    myctx->stock[flag - 1].len = pRecvBuf->pos - myctx->stock[flag - 1].data;
//                }
//                flag++;
//                myctx->stock[flag - 1].data = pRecvBuf->pos + 1;
//            }
//
//            if (flag > 6)
//                break;
//        }
//    }


    //��һ������Ҫ�����ý�����������Ļص�����
    pr->write_event_handler = http_get_filter_post_handler;

    return NGX_OK;
}


static void
http_get_filter_post_handler(ngx_http_request_t * r)
{
    ngx_chain_t out;
    int bodylen;
    ngx_buf_t* b;
    ngx_int_t ret;
    ngx_str_t output_format;
    ngx_http_http_get_filter_ctx_t* myctx;
    static ngx_str_t type = ngx_string("text/plain; charset=GBK");

    myctx = ngx_http_get_module_ctx(r, ngx_http_get_filter_module);

    ngx_str_set(&output_format, "stock[%V],Today current price: %V, volumn: %V");

    //���û�з���200��ֱ�ӰѴ����뷢���û�
    if (r->headers_out.status != NGX_HTTP_OK)
    {
        ngx_http_finalize_request(r, r->headers_out.status);
        return;
    }

    //��ǰ�����Ǹ�����ֱ��ȡ��������

    //���巢���û���http�������ݣ���ʽΪ��
    //stock[��],Today current price: ��, volumn: ��

    //��������Ͱ���ĳ���
//    bodylen = output_format.len + myctx->stock[0].len
//        + myctx->stock[1].len + myctx->stock[4].len - 6;
//    r->headers_out.content_length_n = bodylen;

    //���ڴ���Ϸ����ڴ汣�潫Ҫ���͵İ���
    bodylen = myctx->response_str.len;
    b = ngx_create_temp_buf(r->pool, bodylen);
    memcpy(b->pos, myctx->response_str.data, bodylen);
    b->last = b->pos + bodylen;
    b->last_buf = 1;

    out.buf = b;
    printf("%s:%d body=[%s]\n", __FILE__, __LINE__, (const char*)myctx->response_str.data);
    out.next = NULL;
    //����Content-Type��ע�⺺�ֱ������˷�����ʹ����GBK
    r->headers_out.content_type = type;
    r->headers_out.status = NGX_HTTP_OK;

    r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED;
    ret = ngx_http_send_header(r);
    ret = ngx_http_output_filter(r, &out);

    //ע�⣬���﷢������Ӧ������ֶ�����ngx_http_finalize_request
    //����������Ϊ��ʱhttp��ܲ����ٰ�æ������
    ngx_http_finalize_request(r, ret);
}



static char *
ngx_http_http_get_filter(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_http_core_loc_conf_t  *clcf;
    ngx_http_get_filter_loc_conf_t * lconf; 

    //�����ҵ�http_get_filter���������������ÿ飬clcfò����location���ڵ�����
    //�ṹ����ʵ��Ȼ����������main��srv����loc���������Ҳ����˵��ÿ��
    //http{}��server{}��Ҳ����һ��ngx_http_core_loc_conf_t�ṹ��
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    //http����ڴ����û�������е�NGX_HTTP_CONTENT_PHASE�׶�ʱ�����
    //���������������URI��http_get_filter���������ڵ����ÿ���ƥ�䣬�ͽ���������
    //ʵ�ֵ�ngx_http_http_get_filter_handler���������������
    clcf->handler = ngx_http_http_get_filter_handler;


    lconf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_get_filter_module);
    ngx_str_t* value = (ngx_str_t*)cf->args->elts;
    lconf->subrequest_uri = value[1];

    return NGX_CONF_OK;
}

//nginx���յ�����������������
static ngx_int_t
ngx_http_http_get_filter_handler(ngx_http_request_t * r)
{
    ngx_http_post_subrequest_t *psr;
    ngx_http_request_t *sr;
    ngx_int_t rc;
    ngx_http_get_filter_loc_conf_t* lconf;

    lconf = ngx_http_get_module_loc_conf(r, ngx_http_get_filter_module);

    //����http������
    ngx_http_http_get_filter_ctx_t* myctx = ngx_http_get_module_ctx(r, ngx_http_get_filter_module);
    if (myctx == NULL)
    {
        myctx = ngx_palloc(r->pool, sizeof(ngx_http_http_get_filter_ctx_t));
        if (myctx == NULL)
        {
            return NGX_ERROR;
        }

        //�����������õ�ԭʼ����r��
        ngx_http_set_ctx(r, myctx, ngx_http_get_filter_module);
    }

    // ngx_http_post_subrequest_t�ṹ������������Ļص��������μ�5.4.1��
    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    //����������ص�����Ϊhttp_get_filter_subrequest_post_handler
    psr->handler = http_get_filter_subrequest_post_handler;

    //data��Ϊmyctx�����ģ������ص�http_get_filter_subrequest_post_handler
    //ʱ�����data��������myctx
    psr->data = myctx;

    printf("subrequest_uri=%s\n", (const char*)lconf->subrequest_uri.data);

    //sr����������
    //����ngx_http_subrequest������������ֻ�᷵��NGX_OK
    //����NGX_ERROR������NGX_OKʱ��sr���Ѿ��ǺϷ���������ע�⣬����
    //��NGX_HTTP_SUBREQUEST_IN_MEMORY����������upstreamģ�����
    //�η���������Ӧȫ���������������sr->upstream->buffer�ڴ滺������
    rc = ngx_http_subrequest(r, &lconf->subrequest_uri, NULL, &sr, psr, NGX_HTTP_SUBREQUEST_IN_MEMORY);
    if (rc != NGX_OK)
    {
        return NGX_ERROR;
    }

    //���뷵��NGX_DONE������ͬupstream
    return NGX_DONE;
}

