#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
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
    printf(" - [%5d] World !\n", getpid());
    sethostname("In Namespace", 12);
    mount("proc", "/proc", "proc", 0, NULL);
    read(checkpoint[0], &c, 1);
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);
    printf(" - [%5d] Hello ?\n", getpid());
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    close(checkpoint[1]);
    waitpid(child_pid, NULL, 0);
    mount("proc", "/proc", "proc", 0, NULL);
    return 0;
}
