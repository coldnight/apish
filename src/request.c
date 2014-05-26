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
static char *parse_request_query(const RequestContainer *, const Request *);

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
    if (tmp == NULL){
        return -1;
    }
    if (prev==NULL)
        request_container = NULL;
    else
        prev->next = tmp->next;
    free_one_container(tmp);
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


void
add_request(RequestContainer *reqcont, const char *path, method_t method)
{
    Request *tmp;
    char *p;

    tmp = find_request(reqcont, path, method);
    if (tmp != NULL)
        return;

    if ((p = dupstr(path)) == NULL){
        return;
    }
    if ((tmp = (Request *)malloc(sizeof(Request))) == NULL){
        perror("memory is empty");
        return;
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

static void append_hash(StrHash **hash, const char *key, const char *value)
{
    StrHash *tmp = find_hash(*hash, key);
    char *k, *v;
    if ((v = dupstr(value)) == NULL){
        return;
    }
    if (tmp == NULL){
        if ((tmp = (StrHash *) malloc(sizeof(StrHash))) == NULL){
            perror("memory is empty");
            return;
        }
        if ((k = dupstr(key)) == NULL)
            return;
        tmp->key = k;
        tmp->val = v;
        tmp->next = NULL;
        StrHash *h, *o;
        for (o = h = *hash; h != NULL; o = h,h=h->next)
            ;
        if (o != NULL){
            o->next = tmp;
        }
    }else{
        tmp->val = v;
    }

    if (*hash == NULL){
        *hash = tmp;
    }
}

void add_query(Request *tmp, const char *key, const char *value)
{
    append_hash(&tmp->query, key, value);
}

char *find_query(Request *req, const char *key)
{
    StrHash *tmp;
    for (tmp = req->query; tmp != NULL; tmp=tmp->next)
        if (strcmp(key, tmp->key) == 0)
            return tmp->val;
    return NULL;
}

void add_header(Request *tmp, const char *key, const char *value)
{
    append_hash(&tmp->header, key, value);
}

char *find_header(Request *req, const char *key)
{
    StrHash *t;
    t = find_hash(req->header, key);
    if (t == NULL)
        return NULL;
    else
        return t->val;
}

StrHash *find_hash(StrHash *hash, const char *key)
{
    StrHash *tmp;
    for (tmp=hash; tmp != NULL; tmp=tmp->next)
        if (strcmp(tmp->key, key) == 0)
            return tmp;
    return NULL;
}

static void
free_request(RequestContainer *rc)
{
    Request *req, *next;
    for (req = rc->requests; req != NULL; req=next){
        next = req->next;
        StrHash *hash, *next;
        for (hash=req->query; hash != NULL; hash=next){
            next = hash->next;
            free(hash->key);
            free(hash->val);
            free(hash);
        }

        for (hash=req->header; hash != NULL; hash=next){
            next = hash->next;
            free(hash->key);
            free(hash->val);
            free(hash);
        }
        free(req->path);
        json_object_put(req->write_data);
        free(req);
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


static char *get_dat_path(void)
{
    char *hp = getenv("HOME");
    if (hp == NULL)
        return DATA_FILE;
    char *r = (char *) malloc(sizeof(char) * (strlen(hp) + strlen(DATA_FILE) + 2));
    if (r == NULL){
        perror("memory empty");
        return DATA_FILE;
    }
    strcpy(r, hp);
    strcat(r, "/");
    strcat(r, DATA_FILE);
    return r;
}

static char *hash_dump(StrHash *h)
{
    StrHash *tmp;
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
    printf("hash dump: %s\n", ret);
#endif
    return ret;
}

static StrHash *hash_load(const char *str)
{
    StrHash *hash = NULL;
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

                append_hash(&hash, key, val);
                free(key);
                free(val);
                key = NULL;
                val = NULL;
                state = KEY;
            }
        }
    }
    if (key != NULL && val != NULL){
        append_hash(&hash, key, val);
    }
    return hash;
}

static char *fscanfstr(FILE *fp, const char *format)
{
    char buf[BUFSIZ];
    fscanf(fp, format, buf);
    return dupstr(buf);
}

static int count_n(FILE *fp){
    int i = fgetc(fp), c=0;
    for(; i == '\n'; c++, i=fgetc(fp))
        ;
    ungetc(i, fp);
    return c;
}

void request_load(void)
{
    char *path = get_dat_path();
    char *host, *scheme, *pth;
    Request *req;
    int port, count;
    enum STATE {NONE, RC, REQ, QUERY, HEADER};
    int state = RC;
    char *headerstr;
    method_t method;
    RequestContainer *rc = NULL;
    FILE *fp;
    if ((fp = fopen(path, "r")) == NULL)
        return perror(path);

    while (!feof(fp)){
        switch(state){
            case RC:
                scheme = fscanfstr(fp, "%s\n");
                if (strlen(scheme) == 0){
                    continue;
                }
                host = fscanfstr(fp, "%s\n");
                fscanf(fp, "%d", &port);
                rc = create_request_container(scheme, host, port);
#ifdef DEBUG
                printf("%s\n%s\n%d\n", scheme, host, port);
#endif
                free(scheme);
                free(host);
                count = count_n(fp);
                if (count == 0)
                    state = NONE;
                else if (count == 2)
                    state = REQ;
                else if (count == 4)
                    state = RC;
                continue;
            case REQ:
                if (rc == NULL)
                    continue;
                pth = fscanfstr(fp, "%s\n");
                fscanf(fp, "%d", &method);
#ifdef DEBUG
                printf("%s\n%d\n", pth, method);
#endif
                add_request(rc, pth, method);
                count = count_n(fp);
                if (count == 0){
                    free(pth);
                    state = NONE;
                }else if (count == 4){
                    state = RC;
                    free(pth);
                }else if (count == 2){
                    free(pth);
                    state = REQ;
                }else if (count == 1)
                    state = QUERY;
                continue;

            case QUERY:
                if (rc == NULL)
                    continue;
                char *querystr;
                querystr = fscanfstr(fp, "%s");
#ifdef DEBUG
                printf("%s\n", querystr);
#endif
                StrHash *query;
                query = hash_load(querystr);
                req = find_request(rc, pth, method);
                req->query = query;
                count = count_n(fp);
                free(querystr);
                if (count == 1)
                    state = HEADER;
                else if(count == 3)
                    state = REQ;
                else if(count == 7)
                    state = RC;
                else
                    state = NONE;
                continue;

            case HEADER:
                headerstr = fscanfstr(fp, "%s");
#ifdef DEBUG
                printf("%s\n", headerstr);
#endif
                StrHash *header;
                header = hash_load(headerstr);
                req = find_request(rc, pth, method);
                req->header = header;
                free(headerstr);
                count = count_n(fp);
                if (count == 6){
                    state = RC;
                } else if (count == 2){
                    state = REQ;
                }
                else{
                    state = NONE;
                }
                continue;
            case NONE:
                goto BRK_WHILE;
        }
BRK_WHILE:
        break;
    }

    fclose(fp);
}

void request_dump(void)
{
    char *path = get_dat_path();
    char *lpath = get_lock_path(path);
    if (is_file_locked(path))
        return;
    FILE *fp;
    RequestContainer *rc;

    fp = fopen(lpath, "w");
    if (fp == NULL){
        perror(path);
        return;
    }

    for (rc = request_container; rc != NULL; rc=rc->next){
        fwrite(rc->scheme, strlen(rc->scheme), sizeof(char), fp);
        fputc('\n', fp);
        fwrite(rc->host, strlen(rc->host), sizeof(char), fp);
        fputc('\n', fp);
        fprintf(fp, "%d\n", rc->port);
        fwrite("\n", 1, sizeof(char), fp);

        Request *req;
        for (req = rc->requests; req != NULL; req=req->next){
            fwrite(req->path, strlen(req->path), sizeof(char), fp);
            fputc('\n', fp);
            fprintf(fp, "%d\n", req->method);
            if(req->query){
                char *query;
                query = hash_dump(req->query);
                if (query != NULL){
                    fwrite(query, strlen(query), sizeof(char), fp);
                    free(query);
                }
            }else{
                fputc('\0', fp);
            }
            fputc('\n', fp);
            if (req->header){
                char *header = hash_dump(req->header);
                if (header != NULL){
                    fwrite(header, strlen(header), sizeof(char), fp);
                    free(header);
                }
            }
            fputc('\n', fp);
            fputc('\n', fp);
        }
        fwrite("\n\n\n\n", 4, sizeof(char), fp);
    }
    fclose(fp);
    rename(lpath, path);
    free(lpath);
    unlock_file(path);

    if (strcmp(path, DATA_FILE) != 0){
        free(path);
    }
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
    StrHash *tmp;

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
        fprintf(stderr, "Request Index Error: %s\n", buf);
        return NULL;
    }
    Request *tmp = rc->requests;
    if (index != 0){
        for (i = 0; tmp != NULL && i != index; tmp = tmp->next, i++)
            ;

        if (i != index || tmp == rc->requests){
            fprintf(stderr, "Request Index Error: %s\n", buf);
            return NULL;
        }
    }
    if (tmp == req){
        fprintf(stderr, "Request Reference Error: %s Reference Its Own\n", buf);
        return NULL;
    }
     struct json_object *obj, *tmpobj;
    if (tmp->write_data == NULL){
        tmpobj = request_run((RequestContainer *)rc, tmp);
        if (tmpobj == NULL){
            fprintf(stderr, "Request Denpendence Error: %s Denpendence NOT Return JSON Object\n", buf);
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
                fprintf(stderr, "Empty Brackets Error: %s\n", buf);
                goto error;
            }
        }else if (state == DOTKEY){
            if (ch != '.' && ch != '['){
                t[ti++] = ch;
            }else{
                t[ti] = '\0';
                ti = 0;
                if (!json_object_object_get_ex(obj, t, &obj)){
                    fprintf(stderr, "Key Eerror: %s in %s\n", t, buf);
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
                    fprintf(stderr, "Key Eerror: %s in %s\n", t, buf);
                    goto error;
                }
                need_redge = 1;
            }
        } else if (state == STRKEY && need_redge == 1){
            if (ch == ']'){
                need_redge = 0;
                state = NONE;
            }else if (ch != ' '){
                fprintf(stderr, "Brackets Right Edge Error: %s\n", buf);
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
        fprintf(stderr, "Quote Error: %c is not closed: %s\n", q, buf);
        goto error;
    }
    if (state == DOTKEY){
        t[ti] = '\0';
        if (!json_object_object_get_ex(obj, t, &obj)){
            fprintf(stderr, "Key Eerror: %s in %s\n", t, buf);
            goto error;
        }
    }
    if (need_redge){
        fprintf(stderr, "Brackets Right Edge Error: %s\n", buf);
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
    StrHash *query = req->query;
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
                    char *r = parse_query_value(rc, req, buf);
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
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
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
    if (obj == NULL){
        printf("\nText Response >>>\n %s\n", str);
    }else{
        req->write_data = obj;
        printf("\nJSON Response >>> \n");
        print_pretty_json(json_object_to_json_string(obj));
    }
    free(str);
    fclose(fp);
    request_number--;
    if (request_number == 0)
        current_request = NULL;
    curl_easy_cleanup(curl);
    return obj;
}
