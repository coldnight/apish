#include "request.h"
#include "command.h"
#include "util.h"
#include <stdio.h>
#include <getopt.h>


static void help_info(const char *exe_name);
int global_colored = 0;

    int
main(int argc, char **argv)
{
    int ch, run=False, dump = True;
    char *path = NULL, *out=NULL;
    while ((ch = getopt(argc, argv, "::rhnc")) != -1){
        switch(ch){
            case 'o':
                out = dupstr(optarg);
                break;
            case 'r':
                run = True;
                break;
            case 'n':
                dump = False;
                break;
            case 'c':
                global_colored = 1;
                break;
            case 'h':
                help_info(*argv);
                return 0;
            default:
                fprintf(stderr, "%s: invalid option '%s'\n", *argv, argv[optind-1]);
                fprintf(stderr, "Try '%s -h' for more infomation\n", *argv);
                return 1;

        }
    }
    if (optind < argc){
        path = argv[optind];
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
        if (dump)
            request_dump(path);
        command_cleanup();
    }
    request_cleanup();
    return 0;
}

static void help_info(const char *exe_name)
{
    printf("Usage: %s [OPTION] [FILENAME]\n\n", exe_name);
    printf("FILENAME is the input file specified, it can be a non-existent file,\n");
    printf("empty file or exists file with 'JSON format data'.\n\n");
    printf("If you DO NOT specify it, the application will use ~/.apish.json as default.\n");
    printf("If you DO NOT specify '-n' option, and the application will write the modified");
    printf(" data to it, and the application will erasure the old data\n\n");
    printf("Options:\n");
    printf("\t-r\t\tJust run the requests and exit\n");
    printf("\t-n\t\tDO NOT write modified data after exit the shell\n");
    printf("\t-c\t\tcolorize the output\n");
    printf("\t-h\t\tShow this information\n");
}
