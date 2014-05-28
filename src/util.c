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
        return True;
    }else{
        return False;
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
    int level = 0, ch, wrap=False, wrapped = False, pch;
    enum QUOTE {NONE, SQ};
    enum QUOTE single_q = NONE;
    enum QUOTE double_q = NONE;
    int n = 0;

    for (ch = *buf; ch != '\0'; pch = ch, ch = *(++buf), n++){

        if ((ch == '{' || ch == '[') && single_q == NONE && double_q == NONE){
            level++;
            putchar(ch);
            putchar('\n');
            wrapped = True;
            print_level_string(level);
        }else if ((ch == '}' || ch == ']') && single_q == NONE && double_q == NONE ){
            putchar('\n');
            wrapped = True;
            level--;
            print_level_string(level);
            putchar(ch);
            wrap=True;
        }else if (ch == ',' && (single_q == NONE && double_q == NONE)){
            putchar(ch);
            putchar('\n');
            wrapped = True;
            print_level_string(level);
            if (wrap)
                wrap = False;
        }else if ( wrapped && ch == ' ')
            continue;
        else{
            if (ch == '\'' && pch != '\\')
                single_q = single_q == SQ ? NONE : SQ;
            if (ch == '"' && pch != '\\')
                double_q = double_q == SQ ? NONE : SQ;
            putchar(ch);
            wrap = False;
            wrapped = False;
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
