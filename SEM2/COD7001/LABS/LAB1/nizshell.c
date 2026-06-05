#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_ARGS 64
#define MAX_TOKENS 128

//History Feature
#define MAX_HISTORY 100

// Multi-Stage Extension
#define MAX_CMDS 16   // max commands in a single pipeline


static char *history[MAX_HISTORY];
static int history_size = 0;   // how many stored (<= MAX_HISTORY)
static int history_start = 0;  // index of oldest entry (circular buffer)


typedef struct {
    char *argv[MAX_ARGS];
    char *infile;
    char *outfile;
    int argc;
} Command;

static volatile sig_atomic_t child_exited = 0;


/* ------------ SIGNAL HANDLERS ------------ */

void sigchld_handler(int signo) {
    (void)signo;
    int saved = errno;
    int status;

    // Reap all finished children
    while (waitpid(-1, &status, WNOHANG) > 0) {}
    errno = saved;
}

void install_handlers() {
    struct sigaction sa;

    // For Ignoring Ctrl-C in shell
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Reap background processes
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

/* ---------------- Advanced Features ----------------- */

/* ------------ HISTORY SUPPORT ------------ */

void add_history(const char *line) {
    if (!line || !*line) return;   // ignore empty

    char *copy = strdup(line);     // make our own copy
    if (!copy) return;             // out of memory -> silently ignore

    if (history_size < MAX_HISTORY) {
        history[history_size++] = copy;
    } else {
        // buffer full: overwrite oldest
        free(history[history_start]);
        history[history_start] = copy;
        history_start = (history_start + 1) % MAX_HISTORY;
    }
}

void print_history_builtin(void) {
    int idx = history_start;

    for (int i = 0; i < history_size; i++) {
        int num = i + 1; // item numbering from 1
        printf("%4d  %s\n", num, history[idx]);
        idx = (idx + 1) % MAX_HISTORY;
    }
}

/* ------------ BASIC UTILITIES ------------ */

void init_command(Command *cmd) {
    cmd->argc = 0;
    cmd->infile = NULL;
    cmd->outfile = NULL;
    for (int i = 0; i < MAX_ARGS; i++)
        cmd->argv[i] = NULL;
}

void print_prompt() {
    printf("nizshell> "); // name of the shell
    fflush(stdout);
}


/* ------------ TOKENIZER WITH QUOTES ------------ */

int tokenize(char *line, char *tokens[]) {
    int nt = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p) break;

        if (nt >= MAX_TOKENS) {
            fprintf(stderr, "Too many tokens\n");
            break;
        }

        // Single-char tokens
        if (*p == '<' || *p == '>' || *p == '|' || *p == '&') {
            tokens[nt++] = p;
            char *next = p + 1;
            *next = '\0';
            p = next + 1;
        }

        // Quoted string
        else if (*p == '"' || *p == '\'') {
            char quote = *p++;
            char *start = p;
            while (*p && *p != quote) p++;
            if (*p) *p = '\0';
            tokens[nt++] = start;
            p++;
        }

        // Normal word
        else {
            char *start = p;
            while (*p && *p != ' ' && *p != '\t' &&
                   *p != '<' && *p != '>' && *p != '|' && *p != '&')
                p++;
            if (*p) {
                *p = '\0';
                p++;
            }
            tokens[nt++] = start;
        }
    }

    return nt;
}


/* ------------ PARSER - Single Stage 

int parse(char *tokens[], int nt, Command *c1, Command *c2, int *has_pipe, int *background) {
    init_command(c1);
    init_command(c2);

    *has_pipe = 0;
    *background = 0;

    Command *cur = c1;

    for (int i = 0; i < nt; i++) {
        char *t = tokens[i];

        if (strcmp(t, "|") == 0) {
            if (*has_pipe) {
                fprintf(stderr, "Error: Only one pipe allowed\n");
                return -1;
            }
            *has_pipe = 1;
            cur = c2;
        }
        else if (strcmp(t, "&") == 0) {
            *background = 1;
        }
        else if (strcmp(t, "<") == 0) {
            if (i + 1 >= nt) {
                fprintf(stderr, "Missing input file\n");
                return -1;
            }
            cur->infile = tokens[++i];
        }
        else if (strcmp(t, ">") == 0) {
            if (i + 1 >= nt) {
                fprintf(stderr, "Missing output file\n");
                return -1;
            }
            cur->outfile = tokens[++i];
        }
        else {
            if (cur->argc >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many args\n");
                return -1;
            }
            cur->argv[cur->argc++] = t;
            cur->argv[cur->argc] = NULL;
        }
    }

    return 0;
}
 
------------ */


/*----------- Parser Multi-Stage ------------ */

int parse(char *tokens[], int nt, Command cmds[], int *num_cmds, int *background) {
    *background = 0;
    *num_cmds = 0;

    if (nt == 0) return 0;

    // init all possible commands
    for (int i = 0; i < MAX_CMDS; i++) {
        init_command(&cmds[i]);
    }

    int cur = 0;
    Command *curcmd = &cmds[cur];
    *num_cmds = 1;

    for (int i = 0; i < nt; i++) {
        char *t = tokens[i];

        if (strcmp(t, "|") == 0) {
            // prevent empty command before |
            if (curcmd->argc == 0 && !curcmd->infile && !curcmd->outfile) {
                fprintf(stderr, "Error: empty command in pipeline\n");
                return -1;
            }

            if (*num_cmds >= MAX_CMDS) {
                fprintf(stderr, "Error: too many commands in pipeline (max %d)\n", MAX_CMDS);
                return -1;
            }

            cur++;
            curcmd = &cmds[cur];
            (*num_cmds)++;
        }
        else if (strcmp(t, "&") == 0) {
            *background = 1;
        }
        else if (strcmp(t, "<") == 0) {
            if (i + 1 >= nt) {
                fprintf(stderr, "Missing input file\n");
                return -1;
            }
            curcmd->infile = tokens[++i];
        }
        else if (strcmp(t, ">") == 0) {
            if (i + 1 >= nt) {
                fprintf(stderr, "Missing output file\n");
                return -1;
            }
            curcmd->outfile = tokens[++i];
        }
        else {
            if (curcmd->argc >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many args\n");
                return -1;
            }
            curcmd->argv[curcmd->argc++] = t;
            curcmd->argv[curcmd->argc] = NULL;
        }
    }

    // final command must not be empty
    Command *last = &cmds[*num_cmds - 1];
    if (last->argc == 0 && !last->infile && !last->outfile) {
        fprintf(stderr, "Error: empty command at end of pipeline\n");
        return -1;
    }

    return 0;
}

/* ------------ BUILTINS ------------ */

int is_builtin(Command *cmd) {
    if (cmd->argc == 0) return 0;
    return strcmp(cmd->argv[0], "cd") == 0 ||
           strcmp(cmd->argv[0], "exit") == 0 ||
           strcmp(cmd->argv[0], "history") == 0;
}

void run_builtin(Command *cmd) {
    if (strcmp(cmd->argv[0], "cd") == 0) {
        char *dir = cmd->argc > 1 ? cmd->argv[1] : getenv("HOME");
        if (chdir(dir) < 0)
            perror("cd");
    }
    else if (strcmp(cmd->argv[0], "exit") == 0) {
        // free history before exiting (optional but nice)
        for (int i = 0; i < history_size; i++) {
            int idx = (history_start + i) % MAX_HISTORY;
            free(history[idx]);
        }
        exit(0);
    }
    else if (strcmp(cmd->argv[0], "history") == 0) {
        print_history_builtin();
    }
}


/* ------------ REDIRECTION ------------ */

void apply_redirection(Command *cmd) {
    int fd;

    if (cmd->infile) {
        fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) { perror("open infile"); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (cmd->outfile) {
        fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) { perror("open outfile"); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}


/* ------------ EXECUTION ------------ */

void exec_single(Command *cmd, int background) {
    if (cmd->argc == 0) return;

    if (is_builtin(cmd)) {
        run_builtin(cmd);
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {
        // Child: restore Ctrl-C behavior
        signal(SIGINT, SIG_DFL);

        apply_redirection(cmd);
        execvp(cmd->argv[0], cmd->argv);
        perror("execvp");
        exit(1);
    }

    if (!background) {
        waitpid(pid, NULL, 0);
    } else {
        printf("[bg pid %d]\n", pid);
    }
}


/*---------- Old Pipeline for Single Stage
void exec_pipe(Command *c1, Command *c2, int background) {
    int pipefd[2];
    pipe(pipefd);

    pid_t p1 = fork();
    if (p1 == 0) {
        signal(SIGINT, SIG_DFL);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        apply_redirection(c1);
        execvp(c1->argv[0], c1->argv);
        perror("exec1");
        exit(1);
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        signal(SIGINT, SIG_DFL);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);
        apply_redirection(c2);
        execvp(c2->argv[0], c2->argv);
        perror("exec2");
        exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    if (!background) {
        waitpid(p1, NULL, 0);
        waitpid(p2, NULL, 0);
    } else {
        printf("[bg pids %d %d]\n", p1, p2);
    }
} --------*/

/*------------ Pipeline Multi-Stage ------------*/

void exec_pipeline(Command cmds[], int num_cmds, int background) {
    if (num_cmds <= 0) return;
    if (num_cmds == 1) {
        exec_single(&cmds[0], background);
        return;
    }

    int pipefd[MAX_CMDS - 1][2];
    pid_t pids[MAX_CMDS];

    // create pipes
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefd[i]) < 0) {
            perror("pipe");
            return;
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            // parent should ideally clean up & wait; here we just report error
            return;
        }

        if (pid == 0) {
            // child
            signal(SIGINT, SIG_DFL);

            // connect stdin
            if (i > 0) {
                if (dup2(pipefd[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2 stdin");
                    exit(1);
                }
            }

            // connect stdout
            if (i < num_cmds - 1) {
                if (dup2(pipefd[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2 stdout");
                    exit(1);
                }
            }

            // close all pipe fds in child
            for (int j = 0; j < num_cmds - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            // apply any redirection on this stage
            apply_redirection(&cmds[i]);

            // For simplicity, treat everything in a pipeline as external command
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("execvp");
            exit(1);
        } else {
            pids[i] = pid;
        }
    }

    // parent: close all pipe fds
    for (int i = 0; i < num_cmds - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    if (!background) {
        // wait for all children
        for (int i = 0; i < num_cmds; i++) {
            waitpid(pids[i], NULL, 0);
        }
    } else {
        // print group of pids
        printf("[bg pipeline");
        for (int i = 0; i < num_cmds; i++) {
            printf(" %d", pids[i]);
        }
        printf("]\n");
    }
}

/* ------------ MAIN LOOP ------------ */

int main(void) {
    install_handlers();

    char *line = NULL;
    size_t cap = 0;

    while (1) {
        print_prompt();

        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) break;

        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';

        // skip empty / whitespace-only lines ---
        char *tmp = line;
        while (*tmp == ' ' || *tmp == '\t')
            tmp++;

        if (*tmp == '\0') {
            // line was empty or only spaces -> don't store in history
            continue;
        }

        // add to history before tokenizing (use trimmed start) ---
        add_history(tmp);

        // --- tokenize & execute ---
                char *tokens[MAX_TOKENS];
        int nt = tokenize(line, tokens);
        if (nt == 0) continue;

        Command cmds[MAX_CMDS];
        int num_cmds = 0;
        int bg = 0;

        if (parse(tokens, nt, cmds, &num_cmds, &bg) < 0)
            continue;

        if (num_cmds == 1)
            exec_single(&cmds[0], bg);
        else
            exec_pipeline(cmds, num_cmds, bg);

        /*---- Old Code
        char *tokens[MAX_TOKENS];
        int nt = tokenize(line, tokens);
        if (nt == 0) continue;

        Command c1, c2;
        int has_pipe = 0, bg = 0;

        if (parse(tokens, nt, &c1, &c2, &has_pipe, &bg) < 0)
            continue;

        if (!has_pipe)
            exec_single(&c1, bg);
        else
            exec_pipe(&c1, &c2, bg);
        
        ----*/
    }

    free(line);
    return 0;
}