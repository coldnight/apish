#include "command.h"
#include "request.h"
#include "apish.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <curl/curl.h>

static int LOOP_FINISHED = 0;
static char *prompt = CMDPROMPT;
static RequestContainer *current_rc = NULL;
static CommandContainer *container;
static CommandContainer *current;
static CommandContainer *maincc;
static CommandContainer *containercc;
static CommandContainer *requestcc;
static Request *current_req = NULL;

static CommandContainer *find_command_container(const char *);
static CommandContainer * create_command_container(const char *);
static Command* create_command(CommandContainer *, const char *, const char *, int (*handler)(const char*));
static Command* find_command(CommandContainer *, const char *);
static char *get_container_prompt(void);


static CommandContainer *
create_command_container(const char *key){
    CommandContainer *tmp = find_command_container(key);
    char *k;
    if (tmp == NULL){
        if ((tmp = (CommandContainer *)malloc(sizeof(CommandContainer))) == NULL){
            perror("memory empty");
            return NULL;
        }
        k = dupstr(key);
        tmp->key = k;
        tmp->next = NULL;
        tmp->commands = NULL;
    }
    if (container == NULL){
        tmp->prev = NULL;
        container = tmp;
    }else{
        CommandContainer *t, *o;
        for (o=t=container; t !=NULL; o=t, t=t->next)
            ;
        tmp->prev = o;
        o->next = tmp;
    }
    return tmp;
}

static CommandContainer *
find_command_container(const char *key)
{
    CommandContainer *tmp;
    for (tmp = container; tmp != NULL; tmp = tmp->next)
        if (strcmp(tmp->key, key) == 0)
            return tmp;
    return NULL;
}

static Command*
create_command(CommandContainer *cc, const char *cmd, const char *info,
        int (*handler)(const char*))
{
    Command *tmp = find_command(cc, cmd);
    char  *i;
    if ((i = dupstr(info)) == NULL){
        return NULL;
    }
    if (tmp == NULL){
        if ((tmp = (Command *)malloc(sizeof(Command))) == NULL){
            perror("memory is empty");
            return NULL;
        }
        char *c;
        if ((c = dupstr(cmd)) == NULL){
            return NULL;
        }
        tmp->cmd = c;
        tmp->next = NULL;
    }
    tmp->info = i;
    tmp->handler = handler;
    if (cc->commands == NULL)
        cc->commands = tmp;
    else{
        Command *t, *o;
        for (o=t=cc->commands; t != NULL; o=t, t=t->next)
            ;
        o->next = tmp;
    }
    return tmp;
}

static Command *
find_command(CommandContainer *cc, const char *cmd)
{
    Command *tmp;
    for (tmp = cc->commands; tmp != NULL; tmp=tmp->next)
        if (strcmp(tmp->cmd, cmd) == 0)
            return tmp;
    return NULL;
}

void command_cleanup(void)
{
    CommandContainer *tmp;
    for (tmp = container; tmp != NULL; tmp = tmp->next){
        Command *cmd;
        for (cmd = tmp->commands; cmd != NULL; cmd=cmd->next){
            free(cmd->cmd);
            free(cmd->info);
            free(cmd);
        }
        free(tmp->key);
        free(tmp);
    }
}

static void dispatch_command(CommandContainer *cc, const char *command)
{
    Command *cmd;
    for (cmd=cc->commands; cmd != NULL; cmd=cmd->next){
        if(cmd->handler(command)){
            return;
        }
    }
    printf("Unknow command %s\n", command);
}

static void raw_input(char *msg, char *buf, size_t size)
{
    int i = 0, c;
    printf("%s ", msg);
    while((c = getc(stdin)) != '\n'){
        if (i < (int) (size  - 1)){
            buf[i++] = c;
        }else{
            break;
        }
    }
    buf[i]='\0';

}

char *
command_generator(const char *text, int state)
{
    static int len;
    static Command *cc;
    if (!state){
        len = strlen(text);
        cc = current->commands;
    }
    while (cc != NULL){
        if (strncmp(cc->cmd, text, len) == 0){
            char *r = dupstr(cc->cmd);
            cc = cc->next;
            return r;
        }else{
            cc = cc->next;
        }
    }
    return NULL;
}

char **
command_completion(const char *text, int start, int end)
{
    char **matches;
    matches = (char **) NULL;
    if (start == 0)
        matches = rl_completion_matches(text, command_generator);
    return matches;
}


jmp_buf ctrlc_buf;

void handle_sigs(int sig)
{
    if (sig == SIGINT){
        printf("\n");
        longjmp(ctrlc_buf, 1);
    }else if (sig == SIGQUIT){
        printf("exit..");
        LOOP_FINISHED = TRUE;
    }
}

void command_loop(void)
{
    if (current == NULL){
        fprintf(stderr, "Please init command first\n");
        return;
    }
    char *buf;
    using_history();
    rl_readline_name = "apish";
    rl_attempted_completion_function = command_completion;
    while(!LOOP_FINISHED){
        /* raw_input(prompt, buf, BUFSIZ); */
        setjmp(ctrlc_buf);
        buf = readline(prompt);
        if (buf == NULL){
            break;
        }
        if (strlen(stripspace(buf)) == 0)
            continue;
        dispatch_command(current, buf);
        add_history(buf);
    }
}

static int do_main_help_handler(const char *cmd)
{
    if (strcmp(cmd, "help") != 0)
        return FALSE;
    Command *tmp;
    for(tmp = current->commands;  tmp != NULL; tmp=tmp->next){
        printf("%-20s\t%s\n", tmp->cmd, tmp->info);
    }
    return TRUE;
}

static int do_main_ls_handler(const char *cmd)
{
    if (strcmp(cmd, "ls") != 0){
        return FALSE;
    }

    RequestContainer *rc;
    int i = 0;
    for (rc = request_container; rc != NULL; rc = rc->next){
        int c = 0;
        Request *req;
        for (req = rc->requests; req != NULL; c++, req = req->next)
            ;
        printf("[%2d] <%s://%s:%d [%2d]>\n", i++, rc->scheme, rc->host, rc->port, c);
    }
    return TRUE;
}

static int do_main_exit_handler(const char *cmd)
{
    if (strcmp(cmd, "exit") != 0)
        return FALSE;
    LOOP_FINISHED = 1;
    return TRUE;
}

static int do_main_cd_handler(const char *cmd){
    if (strncmp(cmd, "cd", 2) != 0)
        return FALSE;
    char *last = NULL;
    int index = 0, i = 0;
    if (strlen(cmd) > 2){
        last = dupstr(strchr(cmd, ' '));
        index = atoi(last);
        free(last);
    }
    RequestContainer *rc;

    for (rc = request_container; rc != NULL && i != index; rc = rc->next, i++)
        ;
    if (rc == NULL){
        printf("Index Error\n");
        return TRUE;
    }else{
        current_rc = rc;
        current = containercc;
    }
    if (strcmp(prompt, CMDPROMPT) != 0)
        free(prompt);
    prompt = get_container_prompt();
    return TRUE;
}

static char * get_container_prompt(void)
{
    char *tmp;
    int size, c=0;
    Request *req;

    for (req = current_rc->requests; req != NULL; c++, req = req->next)
        ;
    size = strlen(current_rc->host) + strlen(current_rc->host) + 54;
    if ((tmp = (char *) malloc(sizeof(char) * size)) == NULL){
        perror("Memory is empty");
        return NULL;
    }
    sprintf(tmp, "[<%s://%s:%d: [%d]>] ", current_rc->scheme, current_rc->host, current_rc->port, c);
    return tmp;
}

static int do_main_new_handler(const char *str)
{
    if (strcmp(str, "new") != 0)
        return FALSE;
    char scheme[20], host[BUFSIZ], port[20];
    int p;
    while (1){
        raw_input("Scheme:", scheme, 20);
        if (strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0)
            break;
    }

    while(1){
        raw_input("Host:", host, BUFSIZ);
        if (strlen(host) > 0)
            break;
    }

    while (1){
        raw_input("Port:", port, 20);
        if ((p = atoi(port)) > 0)
            break;
    }
    create_request_container(scheme, host, p);

    return TRUE;
}

static int do_main_delete_handler(const char *str)
{
    if (strncmp(str, "drop", 4) != 0){
        return FALSE;
    }
    int index = 0;

    if (strncmp(str, "drop ", 5) == 0 && strlen(str) > 5){
        index = atoi(strchr(str, ' '));
    }
    delete_request_container(index);
    return 1;
}

static int do_container_ls_hander(const char *str)
{
    if (strcmp(str, "ls") != 0)
        return 0;
    int i = 0;
    Request *req;
    for (req = current_rc->requests; req != NULL; req=req->next, i++){
        printf("%2d: %-5s %s\n", i, req->method == POST ? "POST" : "GET", req->path);
    }
    return 1;
}


static char *strupper(char *str)
{
    char *p = str;
    int i;
    for (i = 0; *p != '\0'; i++, p++)
        str[i] = toupper(*p);
    return str;
}

static int do_container_new(const char *str)
{
    if (strcmp(str, "new") != 0)
        return FALSE;
    char path[BUFSIZ], method[6];
    method_t m;

    while (TRUE){
        raw_input("Path:", path, BUFSIZ);
        if (strlen(path) > 0)
            break;

    }
    while (TRUE){
        raw_input("Method ([GET]/POST):", method, 6);
        if (strlen(method) == 0){
            m = GET;
            break;
        }else{
            if (strcmp(strupper(method), "POST") == 0){
                m = POST;
                break;
            }else if(strcmp(strupper(method), "GET") == 0){
                m = GET;
                break;
            }
        }
    }
    add_request(current_rc, path, m);
    return TRUE;
}

static int do_container_exit(const char *str)
{
    if (strcmp(str, "exit") != 0)
        return 0;
    free(prompt);
    prompt = CMDPROMPT;
    current = maincc;
    current_rc = NULL;
    return 1;
}

static int do_container_cd(const char *str)
{
    if (strncmp(str, "cd", 2) != 0){
        return FALSE;
    }

    int i, index = 0;
    if (strlen(str) > 3 && strncmp(str, "cd ", 3) == 0){
        char *l;
        l = dupstr(strchr(str, ' '));
        index = atoi(l);
        free(l);
    }
    Request *req = NULL;
    for (i=0, req = current_rc->requests; req != NULL && i != index; i++, req=req->next)
        ;

    if (req == NULL){
        printf("Index Error\n");
        return TRUE;
    }
    int mlen = req->method == POST ? strlen("POST") : strlen("GET");
    prompt = (char *)realloc(prompt, sizeof(char) * (strlen(prompt) + strlen(req->path) + mlen + 7));
    strcat(prompt, " ");
    strcat(prompt, req->path);
    strcat(prompt, " ");
    strcat(prompt, req->method == POST? "POST" : "GET");
    strcat(prompt, " >> ");
    current = requestcc;
    current_req = req;
    return TRUE;
}

static int do_request_exit(const char *str)
{
    if (strcmp(str, "exit") != 0)
        return FALSE;
    if (strcmp(prompt, CMDPROMPT) != 0)
        free(prompt);
    prompt = get_container_prompt();
    current = containercc;
    return TRUE;
}

static char
*strip(char *str, int *n)
{
    int ch;
    for (ch=*str, *n = 0; ch == ' '; (*n)++, ++str, ch = *str)
        ;
    return str;
}

static int
add_key_val(int type, const char *str)
{
    int n;
    char *t;
    t = dupstr(strchr(str, ' '));
    char *last = strip(t, &n);
    char *val = strchr(last, ' ');
    if (val != NULL){
        val = strip(val, &n);
        char *v;
        if ((v = dupstr(val)) == NULL) {
            perror("Memory empty");
            free(t);
            return FALSE;
        }

        last[strlen(last) - strlen(v) - n] = '\0';
        printf("Add %s: %s = %s\n", type == 1 ? "Parameter" : "Header", last, v);
        if (type == 1)
            add_query(current_req, last, v);
        else
            add_header(current_req, last, v);
        free(v);
    }else{
        char *val = type == 1 ? find_query(current_req, last) : find_header(current_req, last);
        if (val == NULL){
            printf("Key Error: %s\n", last);
        }else{
            printf("\t%-20s:%s\n", last, val);
        }
    }
    free(t);
    return TRUE;
}

static int do_request_p(const char *str)
{
    if (strncmp(str, "p", 1) != 0)
        return FALSE;
    if (strlen(str) == 1){
        StrHash *query;
        printf("Parameters:\n");
        for (query=current_req->query; query != NULL; query=query->next){
            printf("\t%-10s:\t%s\n", query->key, query->val);
        }
    }else if(strncmp(str, "p ", 2) == 0){
        add_key_val(1, str);
    }else{
        return FALSE;
    }
    return TRUE;
}

static int do_request_h(const char *str)
{
    if (strncmp(str, "h", 1) != 0)
        return FALSE;
    if (strlen(str) == 1){
        StrHash *header;
        printf("Headers:\n");
        for (header=current_req->header; header != NULL;header =header->next){
            printf("\t%-10s:\t%s\n", header->key, header->val);
        }
    }else if(strncmp(str, "h ", 2) == 0){
        add_key_val(2, str);
    }else{
        return FALSE;
    }
    return TRUE;
}

static int do_request_run(const char *str)
{
    if (strcmp(str, "run") != 0)
        return FALSE;

    request_run(current_rc, current_req);
    return TRUE;
}

void command_init(void)
{
    signal(SIGINT, handle_sigs);
    maincc = create_command_container("main");
    current = maincc;
    create_command(maincc, "cd", "Enter to container", do_main_cd_handler);
    create_command(maincc, "ls", "List Request Containers", do_main_ls_handler);
    create_command(maincc, "new", "Create a Request Container", do_main_new_handler);
    create_command(maincc, "drop", "Drop a Request Container", do_main_delete_handler);
    create_command(maincc, "help", "Show Help Info", do_main_help_handler);
    create_command(maincc, "exit", "Exit", do_main_exit_handler);

    containercc = create_command_container("container");
    create_command(containercc, "cd", "Enter to Request", do_container_cd);
    create_command(containercc, "ls", "List Requests", do_container_ls_hander);
    create_command(containercc, "new", "Create a Request", do_container_new);
    create_command(containercc, "help", "Show Help Info", do_main_help_handler);
    create_command(containercc, "exit", "Back Container List", do_container_exit);

    requestcc = create_command_container("request");
    create_command(requestcc, "p", "Show/Change/Add Parameter", do_request_p);
    create_command(requestcc, "h", "Show/Change/Add Header", do_request_h);
    create_command(requestcc, "run", "Run Request", do_request_run);
    create_command(requestcc, "help", "Show Help Info", do_main_help_handler);
    create_command(requestcc, "exit", "Back Request List", do_request_exit);
}
