#ifndef _COMMAND_H
#define _COMMAND_H

#define CMDPROMPT ">> "
typedef struct COMMAND {
    char *cmd;    // 命令
    char *info;   // 帮助信息
    int (*handler)(const char *);   // 处理了返回 1, 没处理返回 0
    struct COMMAND *next;
} Command;


typedef struct COMMANDCONTAINER{
    char *key;
    Command *commands;
    struct COMMANDCONTAINER *next;
    struct COMMANDCONTAINER *prev;
} CommandContainer;


extern void command_init(void);
extern void command_loop(void);
extern void command_cleanup(void);
#endif
