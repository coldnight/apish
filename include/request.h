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


typedef struct TABLE {
    char *key;
    char *val;
    struct TABLE *next;
} Table;

typedef struct REQ {
    char *path;
    method_t method;
    Table *query;
    Table *header;
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
extern Request *add_request(RequestContainer *, const char *, method_t);
extern int delete_request(RequestContainer *, int);
extern Request *find_request(RequestContainer *, const char *, method_t);
extern Table *find_table(Table *, const char *key);
extern int delete_table(Table **, const char *);
extern void add_query(Request*, const char *, const char *);
extern char *find_query(Request *, const char *);
extern void add_header(Request *, const char *, const char *);
extern char *find_header(Request *, const char *);
extern void free_container();
extern void request_load(const char *path);
extern void request_dump(const char *path);
extern void request_init(void);
extern void request_cleanup(void);
extern struct json_object *request_run(RequestContainer*, Request *);
extern void request_all_run(RequestContainer *);
extern void list_request_container(void);

extern RequestContainer *request_container;

#ifdef __cplusplus
}
#endif

#endif
