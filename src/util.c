#include "util.h"
#include "apish.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdarg.h>

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

static void print_level_string(FILE *stream, int level){

    int i;
    for (i=0; i < level; i++){
        fprintf(stream, "%s", LEVEL_STR);
    }
}

static void print_color_start(const char *color)
{
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
    printf("\033[0m");
}

void fprint_pretty_json(FILE *stream, const char *json_str, int colored)
{
    if (global_colored != -1 && colored){
        colored = global_colored;
    }
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
    enum ROLE {KEY, VALUE};
    enum ROLE role = KEY;
    int color_started = False;
    int n = 0, in_list=False;

    for (ch = *buf; ch != '\0'; pch = ch, ch = *(++buf), n++){

        if ((ch == '{' || ch == '[') && single_q == NONE && double_q == NONE){
            level++;
            fputc(ch, stream);
            fputc('\n', stream);
            wrapped = True;
            print_level_string(stream, level);
            if (ch == '{')
                role = KEY;
            else{
                role = VALUE;
                in_list = True;
            }
        }else if ((ch == '}' || ch == ']') && single_q == NONE && double_q == NONE ){
            if (color_started){
                print_color_end();
                color_started = False;
            }
            fputc('\n', stream);
            wrapped = True;
            level--;
            print_level_string(stream, level);
            fputc(ch, stream);
            wrap=True;
            if (ch == ']')
                in_list = False;
        }else if (ch == ',' && single_q == NONE && double_q == NONE){
            if (color_started){
                print_color_end();
                color_started = False;
            }
            fputc(ch, stream);
            fputc('\n', stream);
            wrapped = True;
            print_level_string(stream, level);
            if (wrap)
                wrap = False;
            if (!in_list)
                role = KEY;
        }else if ( wrapped && ch == ' ')
            continue;
        else{
            if (single_q == NONE && double_q == NONE && ch == ':'){
                role = VALUE;
            }
            if (ch == '\'' && pch != '\\' && role == VALUE){
                single_q = single_q == SQ ? NONE : SQ;
                if (single_q == SQ){
                    if (colored)
                        print_color_start("green");
                    fputc(ch, stream);
                }else{
                    fputc(ch, stream);
                    if (colored)
                        print_color_end();
                }
            }
            else if (ch == '"' && pch != '\\' && role == VALUE){
                double_q = double_q == SQ ? NONE : SQ;
                if (double_q == SQ){
                    if (colored)
                        print_color_start("green");
                    fputc(ch, stream);
                }else{
                    fputc(ch, stream);
                    if (colored)
                        print_color_end();
                }
            }else{
                if (role == KEY &&
                        ((ch == '\'' && pch != '\\') ||
                         (ch == '"' && pch != '\\')) && colored)
                    ;
                else if (role == VALUE && ch != ' ' && ch != '\'' &&
                        ch != '"' && ch != ':' &&
                        single_q == NONE && double_q == NONE && colored){
                    print_color_start("red");
                    color_started = True;
                    fputc(ch, stream);
                }else{
                    fputc(ch, stream);
                }
            }
            wrap = False;
            wrapped = False;
        }
    }
    fputc('\n', stream);
    free(o);
}


void print_pretty_json(const char *json_str)
{
    fprint_pretty_json(stdout, json_str, 1);
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
