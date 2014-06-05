#include "request.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include <json/json.h>
#include <json/json_util.h>

static void free_request(RequestContainer *);
static void free_one_container(RequestContainer *);
static void free_one_request(Request *);
static char *parse_request_query(const RequestContainer *, const Request *);
static void request_requests_load(RequestContainer *, struct json_object *);

RequestContainer * request_container = NULL;
static CURL *easy_handler = NULL;


void request_init(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
    easy_handler = curl_easy_init();

}

void request_cleanup(void)
{
    free_container();
    curl_easy_cleanup(easy_handler);
    curl_global_cleanup();
}

RequestContainer *
create_request_container(const char *scheme, const char *host, int port)
{

    RequestContainer *tmp;
    char *s, *h;
    tmp = find_request_container(scheme, host, port);
    if (tmp != NULL)
        return tmp;

    if ((s = dupstr(scheme)) == NULL){
        return NULL;
    }
    if ((h = dupstr(host)) == NULL){
        return NULL;
    }

    if ((tmp = (RequestContainer *)malloc(sizeof(RequestContainer))) == NULL){
        perror("memory is empty");
        return NULL;
    }
    tmp->requests = NULL;
    tmp->scheme = s;
    tmp->host = h;
    tmp->port = port;
    tmp->verbose = False;
    tmp->next = NULL;
    if (request_container == NULL){
        request_container = tmp;
    }else{
        RequestContainer *t, *o;
        for (o=t=request_container; t != NULL; o=t, t=t->next)
            ;
        if (o != NULL)
            o->next = tmp;
    }
    return tmp;
}


/*
 * 根据索引删除请求容器
 * return 0 success -1 超出索引
 */
int delete_request_container(int index)
{
    int i = 0;
    RequestContainer *tmp, *prev=NULL;
    for (tmp=request_container; tmp != NULL && i < index; prev=tmp,tmp=tmp->next, i++)
        ;
    if (tmp == NULL || i != index){
        return -1;
    }
    if (prev==NULL)
        request_container = tmp->next;
    else
        prev->next = tmp->next;
    free_one_container(tmp);
    return 0;
}

/*
 * 根据索引删除请求
 * return 0 success -1 超出索引
 */
int delete_request(RequestContainer *rc, int index)
{
    int i = 0;
    Request *req, *prev = NULL;
    for (req = rc->requests; req != NULL && i < index; prev=req, req=req->next, i++)
        ;

    if (req == NULL || i != index){
        return -1;
    }
    if (prev == NULL){
        rc->requests = req->next;
    }else{
        prev->next = req->next;
    }
    free_one_request(req);
    return 0;
}


RequestContainer *
find_request_container(const char *scheme,const  char *host, int port)
{
    if (request_container == NULL)
        return NULL;
    RequestContainer *tmp;

    for (tmp=request_container; tmp != NULL; tmp=tmp->next){
        if (strcmp(scheme, tmp->scheme) == 0 &&
                strcmp(host, tmp->host) == 0 && port == tmp->port)
            return tmp;
    }
    return NULL;
}


Request *
add_request(RequestContainer *reqcont, const char *path, method_t method)
{
    Request *tmp;
    char *p;

    tmp = find_request(reqcont, path, method);
    if (tmp != NULL)
        return NULL;

    if ((p = dupstr(path)) == NULL){
        return NULL;
    }
    if ((tmp = (Request *)malloc(sizeof(Request))) == NULL){
        perror("memory is empty");
        return NULL;
    }
    tmp->path = p;
    tmp->method = method;
    tmp->query = NULL;
    tmp->header = NULL;
    tmp->next = NULL;
    tmp->write_data = NULL;
    if (reqcont->requests == NULL){
        reqcont->requests = tmp;
    }else{
        Request *t, *o;
        for (o=t=reqcont->requests; t != NULL; o=t,t=t->next)
            ;
        o->next = tmp;
    }
    return tmp;
}


Request *
find_request(RequestContainer *reqcont, const char *path, method_t method)
{
    Request *tmp;
    for (tmp=reqcont->requests; tmp != NULL; tmp=tmp->next)
        if (strcmp(path, tmp->path) == 0 &&
                method == tmp->method)
            return tmp;
    return NULL;
}

static void append_table(Table **table, const char *key, const char *value)
{
    Table *tmp = find_table(*table, key);
    char *k, *v;
    if ((v = dupstr(value)) == NULL){
        return;
    }
    if (tmp == NULL){
        if ((tmp = (Table *) malloc(sizeof(Table))) == NULL){
            perror("memory is empty");
            return;
        }
        if ((k = dupstr(key)) == NULL)
            return;
        tmp->key = k;
        tmp->val = v;
        tmp->next = NULL;
        Table *h, *o;
        for (o = h = *table; h != NULL; o = h,h=h->next)
            ;
        if (o != NULL){
            o->next = tmp;
        }
    }else{
        tmp->val = v;
    }

    if (*table == NULL){
        *table = tmp;
    }
}

void add_query(Request *tmp, const char *key, const char *value)
{
    append_table(&tmp->query, key, value);
}

char *find_query(Request *req, const char *key)
{
    Table *tmp;
    for (tmp = req->query; tmp != NULL; tmp=tmp->next)
        if (strcmp(key, tmp->key) == 0)
            return tmp->val;
    return NULL;
}

void add_header(Request *tmp, const char *key, const char *value)
{
    append_table(&tmp->header, key, value);
}

char *find_header(Request *req, const char *key)
{
    Table *t;
    t = find_table(req->header, key);
    if (t == NULL)
        return NULL;
    else
        return t->val;
}

Table *find_table(Table *table, const char *key)
{
    Table *tmp;
    for (tmp=table; tmp != NULL; tmp=tmp->next)
        if (strcmp(tmp->key, key) == 0)
            return tmp;
    return NULL;
}

static void
free_one_request(Request *req)
{
    Table *table;
    for (table=req->query; table != NULL; table=table->next){
        free(table->key);
        free(table->val);
        free(table);
    }

    for (table=req->header; table != NULL; table=table->next){
        free(table->key);
        free(table->val);
        free(table);
    }
    free(req->path);
    json_object_put(req->write_data);
    free(req);
}

static void
free_request(RequestContainer *rc)
{
    Request *req;
    for (req = rc->requests; req != NULL; req=req->next){
        free_one_request(req);
    }
}


static void free_one_container(RequestContainer *rc)
{
    free_request(rc);
    free(rc->host);
    free(rc->scheme);
    free(rc);
}
void
free_container()
{
    RequestContainer *rc, *next;
    for (rc=request_container; rc != NULL; rc=next){
        next = rc->next;
        free_one_container(rc);
    }
}


static char *table_dump(Table *h)
{
    Table *tmp;
    char *ret = NULL;

#ifdef DEBUG
    printf("start\n");
#endif
    for (tmp = h; tmp != NULL; tmp=tmp->next){
        char *k, *v, *key, *val;
        if ((key = dupstr(tmp->key)) == NULL){
            break;
        }
        if ((val = dupstr(tmp->val)) == NULL){
            break;
        }
    v = curl_easy_escape(easy_handler, val, 0);
        k = curl_easy_escape(easy_handler, key, 0);
        int size = strlen(k) + strlen(v) + 3;
        if (ret == NULL){
#ifdef DEBUG
            printf("msize: %d\n", size);
#endif
            ret = (char *)malloc(sizeof(char) * size);
            strcpy(ret, "");
        }else{
#ifdef DEBUG
            printf("rsize: %ld\n", size + strlen(ret));
#endif
            ret = (char *) realloc(ret, sizeof(char) * (size + strlen(ret)));

        }
        if (ret == NULL){
            perror("memoty empty");
            curl_free(k);
            curl_free(v);
            free(key);
            free(val);
            break;
        }
        strcat(ret, k);
        strcat(ret, "=");
        strcat(ret, v);
        strcat(ret, "&");
        curl_free(k);
        curl_free(v);
        free(key);
        free(val);
    }
#ifdef DEBUG
    printf("table dump: %s\n", ret);
#endif
    return ret;
}

/*
 * 删除table节点
 * return 0 删除成功, -1 没有该值
 */
int delete_table(Table **root, const char *key)
{
    Table *t, *prev = NULL;
    for (t = *root; t != NULL; prev=t, t = t->next)
        if (strcmp(key, t->key) == 0)
            break;
    if (t == NULL)
        return -1;
    if (prev == NULL)
        *root = t->next;
    else{
        prev->next = t->next;
    }
    free(t->key);
    free(t->val);
    free(t);
    return 0;
}

static Table *table_load(const char *str)
{
    Table *table = NULL;
    char buf[BUFSIZ], *key=NULL, *val=NULL;
    enum STATE {NONE, KEY, VALUE, DONE};
    enum STATE state = KEY;
    int i = 0, t, n = 0;
    for (t = str[n]; t != '\0'; t = str[++n]){
        if (state == KEY){
            if (t != '='){
                buf[i++] = t;
            }else{
                buf[i] = '\0';
                state = VALUE;
                char *ek = curl_easy_unescape(easy_handler, buf, 0, 0);
                if ((key = dupstr(ek)) == NULL){
                    return NULL;
                }
                curl_free(ek);
                i = 0;
            }
        }else if (state == VALUE){
            if (t != '&'){
                buf[i++] = t;
            }else{
                buf[i] = '\0';
                state = DONE;
                char *ev = curl_easy_unescape(easy_handler, buf, 0, 0);
                if ((val = dupstr(ev)) == NULL){
                    return NULL;
                }
                curl_free(ev);
                i = 0;

                append_table(&table, key, val);
                free(key);
                free(val);
                key = NULL;
                val = NULL;
                state = KEY;
            }
        }
    }
    if (key != NULL && val != NULL){
        append_table(&table, key, val);
    }
    return table;
}

void request_load(const char *path)
{
    struct json_object *obj, *tmp = NULL;
    int i = 0;
    if (!file_exists(path)){
        perrormsg("Warnning", 2, path, " is not exists");
        return;
    }
    obj = json_object_from_file(path);
    if (obj == NULL){
        perrormsg("Warnning", 2, path, "is empty or not a file with JSON format data");
        return;
    }
    if (!json_object_object_get_ex(obj, "containers", &obj)){
        perrormsg("Data Format Error", 1, "need containers key");
        return;
    }
    if (!json_object_is_type(obj, json_type_array)){
        perrormsg("Data Format Error", 1, "containers must be a array");
        goto error;
    }
    for (i = 0; i < json_object_array_length(obj); i++){
        char *host, *scheme;
        int port=80, verbose = False;
        struct json_object *hobj, *sobj, *pobj, *vobj, *requests;
        tmp = json_object_array_get_idx(obj, i);
        if (!json_object_is_type(tmp, json_type_object)){
            json_object_put(tmp);
            perrormsg("Type Error", 1, "containers item must be an object");
            goto error;
        }

        /* 获取主机名 */
        if (!json_object_object_get_ex(tmp, "host", &hobj)){
            perrormsg("Key Required", 1, "you must specify host");
            goto error;
        }
        if (json_object_is_type(hobj, json_type_string)){
            host = dupstr(json_object_get_string(hobj));
        }else{
            json_object_put(hobj);
            perrormsg("Type Error", 1, "host must be a string value");
            goto error;
        }

        // 端口
        if (json_object_object_get_ex(tmp, "port", &pobj)){
            if (json_object_is_type(pobj, json_type_int)){
                port = json_object_get_int(pobj);
            }else{
                json_object_put(pobj);
                perrormsg("Type Error", 1, "port must be an int value");
                goto error;
            }
        }

        // 协议
        if (json_object_object_get_ex(tmp, "scheme", &sobj)){
            if (json_object_is_type(sobj, json_type_string)){
                scheme = dupstr(json_object_get_string(sobj));
            }else{
                json_object_put(sobj);
                perrormsg("Type Error", 1, "scheme must be a string value");
                goto error;
            }
        }else{
            scheme = dupstr("http");
        }
        if (!((strlen(scheme) == 4 && (strcmp(scheme, "http") == 0)) ||
                (strlen(scheme) == 5 && (strcmp(scheme, "https") == 0)))){
            perrormsg("Value Error", 1, "scheme must be one of http and https");
        }

        // 详细
        if (json_object_object_get_ex(tmp, "verbose", &vobj)){
            if (json_object_is_type(vobj, json_type_boolean)){
                verbose = json_object_get_boolean(vobj);
            }else{
                json_object_put(vobj);
                perrormsg("Type Error", 1, "verbose must be a boolean value");
                goto error;
            }
        }

        json_object_put(vobj);
        json_object_put(hobj);
        json_object_put(pobj);
        json_object_put(sobj);
        RequestContainer *rc = create_request_container(scheme, host, port);
        rc->verbose = verbose;
        free(host);
        free(scheme);

        // requests
        if (json_object_object_get_ex(tmp, "requests", &requests)){
            if (json_object_is_type(requests, json_type_array)){
                request_requests_load(rc, requests);
            }else{
                json_object_put(requests);
                perrormsg("Type Error", 1, "requests must be a array");
                goto error;
            }
        }
    }
    return;
error:
    json_object_put(obj);
    if (tmp != NULL){
        json_object_put(tmp);
    }
    exit(1);
    return;
}

static void request_requests_load(RequestContainer *rc,
        struct json_object *requests)
{
    int i;
    struct json_object *obj = NULL, *tmp=NULL;
    for (i = 0; i < json_object_array_length(requests); i++){
        char *path;
        method_t method = GET;
        obj = json_object_array_get_idx(requests, i);
        if (!json_object_is_type(obj, json_type_object)){
            perrormsg("Type Error", 1, "requests item must be an object value");
            goto error;
        }

        // path
        if (json_object_object_get_ex(obj, "path", &tmp)){
            if (json_object_is_type(tmp, json_type_string)){
                path = dupstr(json_object_get_string(tmp));
#ifdef DEBUG
                printf("Path: %s\n", path);
#endif
            }else{
                perrormsg("Type Error", 1, "path must be a string value");
                goto error;
            }
        }else{
            perrormsg("Data Error", 1, "request item must have a path item");
            goto error;
        }
        json_object_put(tmp);

        // method
        if (json_object_object_get_ex(obj, "post", &tmp)){
            if (json_object_is_type(tmp, json_type_boolean)){
                if (json_object_get_boolean(tmp)){
                    method = POST;
                }
            }else{
                perrormsg("Type Error", 1, "post must be a boolean value");
                goto error;
            }
        }
        Request *req;
        req = add_request(rc, path, method);
        free(path);
        if (req == NULL){
            perrormsg("Error", 1, "add request error");
            goto error;
        }
        json_object_put(tmp);
        // params
        if (json_object_object_get_ex(obj, "params", &tmp)){
            if (json_object_is_type(tmp, json_type_object)){
                json_object_object_foreach(tmp, key, val)
                {
                    if (json_object_is_type(val, json_type_string)){
#ifdef DEBUG
                        printf("Add Query: %s = %s\n", key, json_object_get_string(val));
#endif
                        add_query(req, key, json_object_get_string(val));
                    }else{
                        perrormsg("Type Error", 3, "params ", key, " value not a string");
                    }
                }
            }else{
                perrormsg("Type Error", 1, "params must be an object value");
                goto error;
            }
        }

        json_object_put(tmp);
        // header
        if (json_object_object_get_ex(obj, "headers", &tmp)){
            if (json_object_is_type(tmp, json_type_object)){
                json_object_object_foreach(tmp, key, val)
                {
                    if (json_object_is_type(val, json_type_string)){
#ifdef DEBUG
                        printf("Add header: %s = %s\n", key, json_object_get_string(val));
#endif
                        add_header(req, key, json_object_get_string(val));
                    }else{
                        perrormsg("Type Error", 3, "Header", key, " value not a string");
                    }
                }
            }else{
                perrormsg("Type Error", 1, "header must be an object value");
                goto error;
            }
        }
    }

error:
    if (obj != NULL)
        json_object_put(obj);
    if (tmp != NULL)
        json_object_put(tmp);
}


void request_dump(const char *path)
{
    if (path == NULL)
        path = get_dat_path();
    char *lpath = get_lock_path(path);

    if (is_file_locked(path)){
        perrormsg("Lock Error", 2, path, " is locked\n");
        return;
    }
    FILE *fp;
    RequestContainer *rc;
    Request *req;
    struct json_object *root_object, *containers, *container, *requests;
    struct json_object *request, *pobj, *hobj;
    Table *table;

    root_object = json_object_new_object();
    containers = json_object_new_array();
    json_object_object_add(root_object, "containers", containers);
    json_object_get(containers);

    for (rc = request_container; rc != NULL; rc = rc->next){
        container = json_object_new_object();
        json_object_array_add(containers, container);
        json_object_get(container);

        json_object_object_add(container, "scheme",
                json_object_new_string(rc->scheme));
        json_object_object_add(container, "host",
                json_object_new_string(rc->host));
        json_object_object_add(container, "verbose",
                json_object_new_boolean(rc->verbose));
        json_object_object_add(container, "port",
                json_object_new_int(rc->port));

        requests = json_object_new_array();
        json_object_object_add(container, "requests", requests);
        json_object_get(requests);

        for (req = rc->requests; req != NULL; req = req->next){
            request = json_object_new_object();
            json_object_array_add(requests, request);
            json_object_get(request);

            json_object_object_add(request, "path",
                    json_object_new_string(req->path));
            json_object_object_add(request, "post",
                    json_object_new_boolean(req->method == POST));

            pobj = json_object_new_object();
            json_object_object_add(request, "params", pobj);
            json_object_get(pobj);

            for (table = req->query; table != NULL; table = table->next){
                json_object_object_add(pobj, table->key,
                        json_object_new_string(table->val));
            }
            json_object_put(pobj);

            hobj = json_object_new_object();
            json_object_object_add(request, "header", hobj);
            json_object_get(hobj);

            for (table=req->header; table != NULL; table = table->next){
                json_object_object_add(hobj, table->key,
                        json_object_new_string(table->val));
            }
            json_object_put(hobj);
            json_object_put(request);
        }
        json_object_put(requests);
        json_object_put(container);
    }
    json_object_put(containers);

    fp = fopen(lpath, "w");
    if (fp == NULL){
        perror(lpath);
        return;
    }
    fprint_pretty_json(fp, json_object_to_json_string(root_object), 0);
    fclose(fp);
    rename(lpath, path);
    json_object_put(root_object);
}

static char *get_request_url(const RequestContainer *rc, const Request *req)
{
    char *ret;
    char p[20];
    sprintf(p, "%d", rc->port);
    int size = sizeof(char) * (strlen(rc->scheme) + 3 + strlen(rc->host) +
            1 + strlen(p) + strlen(req->path) + 1);
    char *querystr = NULL;
    if (req->method == GET){
        querystr = parse_request_query(rc, req);
        if (querystr != NULL)
            size += sizeof(char) * (strlen(querystr) + 1);
        else{
            return NULL;
        }
    }
    if ((ret = (char *)malloc(size)) == NULL){
        perror("Memory empty");
        return NULL;
    }
    sprintf(ret, "%s://%s:%s%s", rc->scheme, rc->host, p, req->path);
    if (req->method == GET && querystr != NULL){
        strcat(ret, "?");
        strcat(ret, querystr);
        free(querystr);
    }
    return ret;
}

struct curl_slist *parse_request_header(Request *req)
{
    struct curl_slist *chunk = NULL;
    Table *tmp;

    for (tmp = req->header; tmp != NULL; tmp=tmp->next){
        char *ret = (char *)malloc(sizeof(char) + (strlen(tmp->key) + strlen(tmp->val) + 3));
        if (ret == NULL){
            perror("Memory empty");
            return chunk;
        }
        strcpy(ret, tmp->key);
        strcat(ret, ": ");
        strcat(ret, tmp->val);
        curl_slist_append(chunk, ret);
        free(ret);
    }
    return chunk;
}

static Request *current_request;
static unsigned int request_number = 0;

/*
 * {{ 0.status }}       -- 第一个请求是一个对象, 取它的 status 字段
 * {{ 0['status'] }}    -- 同上
 * {{ 0[0] }}           -- 第一个请求返回一个数组, 取它的第一个元素
 */
static char *
parse_query_value(const RequestContainer *rc, const Request *req, const char *buf)
{
#ifdef DEBUG
    printf("D: Parse %s\n", buf);
#endif
    int index = -1, i;
    sscanf(buf, "%d", &index);
    if (index == -1){
        perrormsg("Request Index Error", 1, buf);
        return NULL;
    }
    Request *tmp = rc->requests;
    if (index != 0){
        for (i = 0; tmp != NULL && i != index; tmp = tmp->next, i++)
            ;

        if (i != index || tmp == rc->requests){
            perrormsg("Request Index Error", 1, buf);
            return NULL;
        }
    }
    if (tmp == req){
        perrormsg("Request Reference Error", 2, buf, " reference its own\n");
        return NULL;
    }
     struct json_object *obj, *tmpobj;
    if (tmp->write_data == NULL){
        tmpobj = request_run((RequestContainer *)rc, tmp);
        if (tmpobj == NULL){
            perrormsg("Request Denpendence Error", 2,  buf, " denpendence NOT return JSON object\n");
            return NULL;
        }
        obj = json_tokener_parse(json_object_to_json_string(tmpobj));
    }else{
        obj = json_tokener_parse(json_object_to_json_string(tmp->write_data));
    }

    int ch, ti=0, q=0, need_redge = 0, need_quote = 0;
    char t[BUFSIZ], *b;
    enum STATE {NONE, LEDGE, REDGE, DOTKEY, STRKEY, INDEX};
    enum STATE state = NONE;


    if ((b = dupstr(buf)) == NULL){
        json_object_put(obj);
        return NULL;
    }
    char *o = b;

    for (ch = *b; ch != '\0'; ch = *(++b)){
        if (state == NONE){
            if (ch == '.')
                state = DOTKEY;
            else if(ch == '[')
                state = LEDGE;
        } else if (state == LEDGE){
            if (ch == '\'' || ch == '"'){
                state = STRKEY;
                q = ch;
                need_quote = 1;
            }
            else if (isdigit(ch)){
                state = INDEX;
                t[ti++] = ch;
                need_redge = 1;
            }else if (ch == ']'){
                perrormsg("Empty Brackets Error", 1, buf);
                goto error;
            }
        }else if (state == DOTKEY){
            if (ch != '.' && ch != '['){
                t[ti++] = ch;
            }else{
                t[ti] = '\0';
                ti = 0;
                if (!json_object_object_get_ex(obj, t, &obj)){
                    perrormsg("Key Eerror", 3,  t, " in ", buf);
                    goto error;
                }
                if (ch == '.'){
                    state = DOTKEY;
                }else{
                    state = INDEX;
                }
            }
        }else if (state == STRKEY && need_redge == 0){
            if (ch != q){
                t[ti++] = ch;
            }else{
                need_quote = 0;
                t[ti] = '\0';
                ti = 0;
                if (!json_object_object_get_ex(obj, t, &obj)){
                    perrormsg("Key Eerror", 2, t, buf);
                    goto error;
                }
                need_redge = 1;
            }
        } else if (state == STRKEY && need_redge == 1){
            if (ch == ']'){
                need_redge = 0;
                state = NONE;
            }else if (ch != ' '){
                perrormsg("Brackets Right Edge Error", 1, buf);
                goto error;
            }
        }else if (state == INDEX){
            if (ch != ']')
                t[ti++] = ch;
            else{
                t[ti] = '\0';
                obj = json_object_array_get_idx(obj, atoi(t));
                ti = 0;
                need_redge = 0;
                state = NONE;
            }
        }
    }
    if (state == STRKEY && need_quote){
        char b[2] = {q, '\0'};
        perrormsg("Quote Error", 3, b, " is not closed: ", buf);
        goto error;
    }
    if (state == DOTKEY){
        t[ti] = '\0';
        if (!json_object_object_get_ex(obj, t, &obj)){
            perrormsg("Key Error", 3, t, " in ", buf);
            goto error;
        }
    }
    if (need_redge){
        perrormsg("Brackets Right Edge Error", 1, buf);
        goto error;
    }
    char *r;
    if ((r = dupstr(json_object_get_string(obj))) == NULL){
        perror("Memory empty");
        goto error;
    }
    free(o);
    json_object_put(obj);
    return r;

error:
    free(o);
    json_object_put(obj);
    return NULL;
}


static char *parse_request_query(const RequestContainer *rc, const Request *req)
{
    if (req == current_request && request_number != 1){
        fprintf(stderr, "%s %s <==> %s %s Interdependence\n",
                current_request->method == POST ? "POST" : "GET",
                current_request->path,
                req->method == POST ? "POST": "GET",
                req->path);
        return NULL;
    }
    enum STATE {NONE, M1, START, END};
    enum STATE state = NONE;
    Table *query = req->query;
    char buf[BUFSIZ], *ret;
    if ((ret = (char *) malloc(sizeof(char) * (BUFSIZ + 1))) == NULL){
        perror("Memory empty");
        return NULL;
    }
    strcpy(ret, "");
    for (; query != NULL; query=query->next){
        char *v = query->val;
        char *rv;
        int size = BUFSIZ;
        if ((rv = (char *) malloc(sizeof(char) * (size + 1))) == NULL){
            perror("Memory empty");
            free(ret);
            return NULL;
        }
        strcpy(rv, "");
        int i = 0, ch;
        for (ch = *v; ch != '\0'; ch = *(++v)){
            if (ch == '{' && state == NONE)
                state = M1;
            else if (ch == '{' && state == M1)
                state = START;
            else if (state == START && ch == '}')
                state = END;
            else if (state == END){
                if (ch == '}'){
                    buf[i] = '\0';
                    state = NONE;
                    char *r = parse_query_value(rc, req, stripspace(buf));
                    if (r == NULL){
                        free(ret);
                        free(rv);
                        return NULL;
                    }
                    strcat(rv, r);
                }else{
                    // 如果没有结束说明 } 是内容的一部分
                    buf[i++] = '}';
                    buf[i++] = ch;
                    state = START;
                }
            }
            else if (state == START)
                buf[i++] = ch;
            else{
                char b[2] = {ch, '\0'};
                strcat(rv, b);
            }
        }
        strcat(ret, query->key);
        strcat(ret, "=");
        char *ev = curl_easy_escape(easy_handler, rv, 0);
        strcat(ret, ev);
        strcat(ret, "&");
        free(rv);
        curl_free(ev);
    }
#ifdef DEBUG
    printf("Query String: %s\n", ret);
#endif
    return ret;
}


struct json_object *
request_run(RequestContainer *rc, Request *req)
{
    if (current_request == NULL)
        current_request = req;

    request_number++;

    CURLcode res;
    CURL *curl = curl_easy_init();
    char *url = get_request_url(rc, req);
    if (url == NULL){
        request_number--;
        return NULL;
    }
    char *body = NULL;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    if (current_request == req){
        curl_easy_setopt(curl, CURLOPT_VERBOSE, rc->verbose);
    }else{
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
    }
    if (req->method == POST){
        body = parse_request_query(rc, req);
        if (body == NULL){
            free(url);
            request_number--;
            return NULL;
        }
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }
    struct curl_slist *chunk = NULL;

    FILE *fp = tmpfile();
    if (fp == NULL){
        perror("tmp file error");
        request_number--;
        if (request_number == 0)
            current_request = NULL;
        return NULL;
    }
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    if (req->header != NULL){
        chunk = parse_request_header(req);
    }
    if (chunk != NULL){
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
    res = curl_easy_perform(curl);
    if (res != CURLE_OK){
        printf("\nRequest: %s failed: %s\n", url,
                curl_easy_strerror(res));
    }
    free(url);
    if (body != NULL)
        free(body);
    curl_slist_free_all(chunk);

    fseek(fp, 0, SEEK_SET);

    int size = BUFSIZ;
    char *str = (char *)malloc(sizeof(char) * (size + 1));
    if (str == NULL){
        perror("Memory empty");
        request_number--;
        if (request_number == 0)
            current_request = NULL;
        return NULL;
    }
    strcpy(str, "");
    while(!feof(fp)){
        size_t s = fread(str, BUFSIZ, sizeof(char), fp);
        if (s == BUFSIZ){
            str = (char *)realloc(str, sizeof(char) * (size + BUFSIZ + 1));
            if (str == NULL){
                request_number--;
                if (request_number == 0)
                    current_request = NULL;
                perror("Memory empty");
                return NULL;
            }
        }
    }
    struct json_object *obj;
    obj = json_tokener_parse(str);
    if (current_request == req){
        if (obj == NULL){
            printf("\nText Response >>>\n %s\n", str);
        }else{
            if (req->write_data != NULL)
                json_object_put(req->write_data);

            req->write_data = obj;
            printf("\nJSON Response >>> \n");
            print_pretty_json(json_object_to_json_string(obj));
        }
    }
    free(str);
    fclose(fp);
    request_number--;
    if (request_number == 0)
        current_request = NULL;
    curl_easy_cleanup(curl);
    return obj;
}

void request_all_run(void)
{
    RequestContainer *rc;
    Request *req;
    for (rc = request_container; rc != NULL; rc = rc->next){
        printf(">>>>>>>> Begain %s://%s:%d <<<<<<<<\n", rc->scheme,
                rc->host, rc->port);
        for (req = rc->requests; req != NULL; req=req->next){
            printf("========== Request %s %s ==========\n",
                    req->method == POST ? "POST" : "GET", req->path);
            request_run(rc, req);
            printf("========== End Request %s %s ==========\n",
                    req->method == POST ? "POST" : "GET", req->path);
        }
        printf(">>>>>>>> End %s://%s:%d <<<<<<<<\n", rc->scheme,
                rc->host, rc->port);
    }
}
