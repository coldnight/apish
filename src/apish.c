#include "request.h"
#include "command.h"
#include "util.h"
#include <stdio.h>
#include <getopt.h>


static void help_info(const char *exe_name);
    int
main(int argc, char **argv)
{
    int ch, run=False;
    char *path = NULL, *out=NULL;
    while ((ch = getopt(argc, argv, "f:o::rh")) != -1){
        switch(ch){
            case 'f':
                path = dupstr(optarg);
                break;
            case 'o':
                out = dupstr(optarg);
                break;
            case 'r':
                run = True;
                break;
            case 'h':
                help_info(*argv);
                return 0;
        }
    }
    if (path == NULL){
        path = get_dat_path();
    }
    request_load(path);
    request_init();
    if (run){
        request_all_run();
    }else{
        command_init();
        command_loop();
        request_dump(path);
        command_cleanup();
    }
    request_cleanup();
    return 0;
}

static void help_info(const char *exe_name)
{
    printf("Usage: %s [OPTION]\n", exe_name);
    printf("Options:\n");
    printf("\t-f filename\tThe JSON format file with requests content\n");
    printf("\t-o filename\tSpecify the out file, default is stdout\n");
    printf("\t-r\t\tJust run the requests and exit\n");
    printf("\t-h\t\tShow this information\n");
}
