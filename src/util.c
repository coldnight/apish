#include "util.h"
#include "apish.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

int file_exists(const char *path)
{
    struct stat buf;
    if (stat(path, &buf) == 0){
        return TRUE;
    }else{
        return FALSE;
    }
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
        return FALSE;
    }
    int ret = file_exists(lpth);
    free(lpth);
    return ret;
}

int lock_file(const char *path)
{
    char *pth = get_lock_path(path);
    if (pth == NULL){
        return FALSE;
    }

    if (file_exists(pth)){
        free(pth);
        return TRUE;
    }
    FILE *fp = fopen(pth, "w");
    if (fp == NULL){
        free(pth);
        return FALSE;
    }
    fclose(fp);
    free(pth);
    return TRUE;
}

int unlock_file(const char *path)
{
    char *lpth = get_lock_path(path);
    if (lpth == NULL){
        return FALSE;
    }

    if (!file_exists(lpth))
        return TRUE;
    if (remove(lpth) == 0){
        free(lpth);
        return TRUE;
    }
    free(lpth);
    return FALSE;
}

static void print_level_string(int level){

    int i;
    for (i=0; i < level; i++){
        printf("%s", LEVEL_STR);
    }
}

void print_pretty_json(const char *json_str)
{
    char *buf;
    if ((buf = (char *)malloc(sizeof(char) * (strlen(json_str) + 1))) == NULL){
        perror("Memory empty");
        return;
    }
    strcpy(buf, json_str);
    char *o = buf;
    int level = 0, ch, wrap=FALSE, wrapped = FALSE;
    int i, n = 0;

    for (ch = *buf; ch != '\0'; ch = *(++buf), n++){

        if (ch == '{' || ch == '['){
            level++;
            putchar(ch);
            putchar('\n');
            wrapped = TRUE;
            print_level_string(level);
        }else if (ch == '}' || ch == ']'){
            putchar('\n');
            wrapped = TRUE;
            level--;
            print_level_string(level);
            putchar(ch);
            wrap=TRUE;
        }else if (ch == ','){
            putchar(ch);
            putchar('\n');
            wrapped = TRUE;
            print_level_string(level);
            if (wrap)
                wrap = FALSE;
        }else if ( wrapped && ch == ' ')
            continue;
        else{
            putchar(ch);
            wrap = FALSE;
            wrapped = FALSE;
        }
    }
    /* printf("Done\n\n"); */
    putchar('\n');
    free(o);
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
