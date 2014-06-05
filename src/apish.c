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
    int ch, run=False, dump = True, list=False, index=-1;
    char *path = NULL;
    while ((ch = getopt(argc, argv, "r::lhnc")) != -1){
        switch(ch){
            case 'r':
                run = True;
                if (optarg)
                    index = atoi(optarg);
                break;
            case 'n':
                dump = False;
                break;
            case 'l':
                list = True;
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
    if (list){
        list_request_container();
        return 0;
    }
    if (run && index < 0){
        request_all_run();
    }else if (run && index >= 0){
        int i = 0;
        RequestContainer *rc = NULL;
        for (rc=request_container; rc != NULL && i != index; i++, rc=rc->next)
            ;
        if (rc == NULL){
            fprintf(stderr, "Error: %d is out of index\n", index);
            return 1;
        }
        request_run_container(rc);
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
    printf(" data to it, and cover the old data\n\n");
    printf("Options:\n");
    printf("\t-c\t\tcolorize the output\n");
    printf("\t-l\t\tlist request containers\n");
    printf("\t-n\t\tDO NOT write modified data after exit the shell\n");
    printf("\t-r[INDEX]\tjust run the requests and exit, \n");
    printf("\t\t\t  INDEX is the index of containers, default is run all containers.\n");
    printf("\t-h\t\tshow this information\n");
}

