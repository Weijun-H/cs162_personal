#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "show the current directory"},
    {cmd_cd, "cd", "change the current directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Print the current working directory to standard output */
int cmd_pwd(unused struct tokens* tokens) {
    char buf[80];
    getcwd(buf, sizeof(buf));
    printf("current working directory : %s\n", buf);
    return 1;
}

int cmd_cd(unused struct tokens* tokens) {
  if (tokens_get_length(tokens) != 2) {
    printf("Usage: cd path\n");
    exit(1);
  }
  
  int error = chdir(tokens_get_token(tokens, 1));
  if (error == -1) printf("No such file or directory\n");
  return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */      
      if (tokens_get_length(tokens) == 2) {
        char* program = tokens_get_token(tokens, 0);
        char* arg = tokens_get_token(tokens, 1);
        int pid = fork();
        int error = 0;
        if (pid == 0) {
          error = execl(program, arg);
          if (error == -1) {
            // fprintf(stdout, "error: %d\n", error);
            char* _paths = getenv("PATH");
            char* paths = (char*) malloc(strlen(_paths) + 1);
            paths = _paths;
            char* path;
            char* curr_path;
            path = strsep(&paths, ":");
            while (path != NULL&& error == -1) {
              // fprintf(stdout, "%s\n", path);
              // fprintf(stdout, "%s\n", paths);
              curr_path = (char*) malloc(strlen(path) + 1 +strlen(program) + 10);
              strcpy(curr_path, path);
              strcat(curr_path, "/");
              strcat(curr_path, program);
              path = strsep(&paths, ":");
              error = execl(curr_path, curr_path, arg, NULL);
            }
            if (error == -1) fprintf(stdout, "This shell cannot run this command\n");
            return;
          }
        } else {
          int returnStatus;
          waitpid(pid, &returnStatus, 0);
          
        }

      }else {
        fprintf(stdout, "This shell doesn't know how to run programs.\n");
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
