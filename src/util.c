#include "util.h"
#include "apish.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdarg.h>

static void print_color_start(const char *color);
static void print_color_end(void);
static void fprint_level_string(FILE *stream);
static void fprint_json_object_key(FILE *, const char *, int, int);
static void fprint_json_object_item(FILE *, struct json_object *, int);
static void fprint_json_object_object(FILE *, struct json_object *, int);
static void fprint_json_object_array(FILE *, struct json_object *, int);

enum pjson_state_t {NONE, KEY, VALUE, ARRAY, OBJECT};
static int pjson_level = 0;
static enum pjson_state_t prev_type = NONE;
static enum pjson_state_t current_type = NONE;

int file_exists(const char *path)
{
    struct stat buf;
    if (stat(path, &buf) == 0){
        return True;
    }else{
        return False;
    }
}

char *get_dat_path(void)
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

char *get_lock_path(const char *path)
{
    char *tmp;
    if ((tmp = (char *)malloc(sizeof(char) * (strlen(path) + strlen(LOCK_SUFFIX) + 1))) == NULL){
        return NULL;
    }
    strcpy(tmp, path);
    strcat(tmp, LOCK_SUFFIX);
    return tmp;
}

int is_file_locked(const char *path)
{
    char *lpth = get_lock_path(path);
    if (lpth == NULL){
        return False;
    }
    int ret = file_exists(lpth);
    free(lpth);
    return ret;
}

int lock_file(const char *path)
{
    char *pth = get_lock_path(path);
    if (pth == NULL){
        return False;
    }

    if (file_exists(pth)){
        free(pth);
        return True;
    }
    FILE *fp = fopen(pth, "w");
    if (fp == NULL){
        free(pth);
        return False;
    }
    fclose(fp);
    free(pth);
    return True;
}

int unlock_file(const char *path)
{
    char *lpth = get_lock_path(path);
    if (lpth == NULL){
        return False;
    }

    if (!file_exists(lpth))
        return True;
    if (remove(lpth) == 0){
        free(lpth);
        return True;
    }
    free(lpth);
    return False;
}

static void fprint_level_string(FILE *stream){

    int i;
    for (i=0; i < pjson_level; i++){
        fprintf(stream, "%s", LEVEL_STR);
    }
}

static void print_color_start(const char *color)
{
    if (!global_colored)
        return;
    const char *out;
    if (strcmp(color, "red") == 0)
        out = "\033[0;31;5m";
    else if (strcmp(color, "green") == 0)
        out = "\033[0;32;5m";
    else if (strcmp(color, "blue") == 0)
        out = "\033[0;34;5m";
    else
        out = "\033[0;37;0m";
    printf("%s", out);
}

static void print_color_end(void)
{
    if (!global_colored)
        return;
    printf("\033[0m");
}

static void fprint_json_object_key(FILE *stream, const char *key, int colored, int first)
{
    if (prev_type != NONE && current_type != ARRAY && !first)
        fprintf(stream, ",\n");
    fprint_level_string(stream);
    prev_type = KEY;
    if (colored)
        fprintf(stream, "%s: ", key);
    else
        fprintf(stream, "\"%s\": ", key);

}

static void fprint_json_object_item(FILE *stream, struct json_object *object, int colored)
{
    json_type type;
    type = json_object_get_type(object);
    switch (type){
        case json_type_object:
            fprint_json_object_object(stream, object, colored);
            break;
        case json_type_array:
            fprint_json_object_array(stream, object, colored);
            break;

        default:
            if (colored){
                print_color_start(type == json_type_string ? "green" : "red");
            }
            prev_type = VALUE;
            if (current_type == ARRAY)
                fprint_level_string(stream);
            fprintf(stream, "%s", json_object_to_json_string(object));
            if (colored){
                print_color_end();
            }
            break;
    }
}
static void fprint_json_object_object(FILE *stream, struct json_object *object, int colored)
{
    enum pjson_state_t old = current_type;
    int i = 0;
    current_type = OBJECT;
    if (old == ARRAY || prev_type != KEY)
        fprint_level_string(stream);
    fprintf(stream, "{\n");
    pjson_level++;
    json_object_object_foreach(object, key, val)
    {
        fprint_json_object_key(stream, key, colored, i++ == 0);
        fprint_json_object_item(stream, val, colored);
    }
    fprintf(stream, "\n");
    pjson_level--;
    fprint_level_string(stream);
    fprintf(stream, "}");
    prev_type = OBJECT;
    current_type = old;
}


static void fprint_json_object_array(FILE *stream, struct json_object *object, int colored)
{
    int i;
    enum pjson_state_t old = current_type;
    current_type = ARRAY;
    if (old == ARRAY || prev_type != KEY)
        fprint_level_string(stream);
    fprintf(stream, "[\n");
    pjson_level++;
    for (i = 0; i < json_object_array_length(object); i++){
        if (i > 0)
            fprintf(stream, ",\n");
        fprint_json_object_item(stream, json_object_array_get_idx(object, i), colored);
    }
    fprintf(stream, "\n");
    pjson_level--;
    fprint_level_string(stream);
    fprintf(stream, "]");
    prev_type = ARRAY;
    current_type = old;
}


void fprint_pretty_json(FILE *stream, struct json_object *object, int colored)
{
    if (!global_colored)
        colored = 0;
    fprint_json_object_item(stream, object, colored);
    fputc('\n', stream);
}


void print_pretty_json(struct json_object *object)
{
    fprint_pretty_json(stdout, object, 1);
}

char *dupstr(const char *s)
{
    char *r;
    if ((r = (char *)malloc(sizeof(char) * (strlen(s) + 1))) == NULL){
        perror("memoty empty");
        return NULL;
    }
    strcpy(r, s);
    return r;
}

char *stripspace(char *string)
{
    char *s, *t;
    for (s = string; isspace(*s); s++)
        ;
    t = s + strlen(s) - 1;
    while (t > s && isspace(*t))
        t--;
    *(++t) = '\0';
    return s;
}

void perrormsg(const char *tip, int n, ...)
{
    va_list ap;
    int i;

    fprintf(stderr, "\033[;31;1m%s\033[0m: ", tip);
    va_start(ap, n);
    for (i = 0; i < n; i++)
        fprintf(stderr, "%s", va_arg(ap, char *));
    va_end(ap);
    putchar('\n');
}
