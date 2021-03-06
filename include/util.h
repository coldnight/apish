#ifndef APISH_UTIL_H
#define APISH_UTIL_H

#define LOCK_SUFFIX ".lock"
#define LEVEL_STR "    "
#include <stdio.h>
#include <json/json.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int file_exists(const char *);
extern int lock_file(const char *);
extern int is_file_locked(const char *);
extern int unlock_file(const char *);
extern char *get_lock_path(const char*);
extern char *dupstr(const char *);
extern char *stripspace(char *);
extern void perrormsg(const char *, int, ...);
extern char *get_dat_path(void);
extern void print_pretty_json(struct json_object *);
extern void fprint_pretty_json(FILE *, struct json_object *, int);

#ifdef __cplusplus
}
#endif
#endif
