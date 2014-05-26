#ifndef APISH_REQUEST_H
#define APISH_REQUEST_H

#include <stdio.h>
#include <json/json.h>
#include "apish.h"

#define HTTP_SCHEME "http"
#define HTTPS_SCHEME "https"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _method_t {GET, POST} method_t;


typedef struct STRHASH {
    char *key;
    char *val;
    struct STRHASH *next;
} StrHash;

typedef struct REQ {
    char *path;
    method_t method;
    StrHash *query;
    StrHash *header;
    struct json_object *write_data;
    struct REQ *next;
} Request;

typedef struct REQCONTAINER{
    char *scheme;
    char *host;
    int port;
    int verbose;
    struct REQCONTAINER *next;
    Request *requests;
}RequestContainer;

extern RequestContainer *create_request_container(const char *, const char *, int);
extern int delete_request_container(int);
extern RequestContainer *find_request_container(const char *, const char *, int);
extern void add_request(RequestContainer *, const char *, method_t);
extern Request *find_request(RequestContainer *, const char *, method_t);
extern StrHash *find_hash(StrHash *, const char *key);
extern void add_query(Request*, const char *, const char *);
extern char *find_query(Request *, const char *);
extern void add_header(Request *, const char *, const char *);
extern char *find_header(Request *, const char *);
extern void free_container();
extern void request_load(void);
extern void request_dump(void);
extern void request_init(void);
extern void request_cleanup(void);
extern struct json_object *request_run(RequestContainer*, Request *);

extern RequestContainer *request_container;

#ifdef __cplusplus
}
#endif

#endif
