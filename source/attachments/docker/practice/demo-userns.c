#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/capability.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];
int child_main(void* arg) {
  cap_t caps;
    for(;;) {
        printf("eUID = %ld, eGID = %ld\n", (long)geteuid(), (long)getegid());
        caps = cap_get_proc();
        printf("capabilities: %s\n", cap_to_text(caps, NULL));
        if(arg == NULL)
            break;
        sleep(5);
    }
    return 0;
}
int main(int argc, char* argv[]) {
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWUSER | SIGCHLD, argv[1]);
    waitpid(child_pid, NULL, 0);
    return 0;
}
