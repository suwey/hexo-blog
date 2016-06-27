#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];
char* const child_args[] = {
    "/bin/bash",
    NULL
};
int checkpoint[2];
int child_main(void* arg) {
    char c;
    close(checkpoint[1]);
    read(checkpoint[0], &c, 1);
    printf(" - World !\n");
    sethostname("In Namespace", 12);
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);
    printf(" - Hello ?\n");
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    sleep(4);
    close(checkpoint[1]);
    waitpid(child_pid, NULL, 0);
    return 0;
}
