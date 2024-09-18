#include "kernel/types.h"
#include "user/user.h"

int main() {
    int pid = getpid();

    // array to store pipe fd
    int p[2];
    pipe(p);

    if (fork() == 0) {
        // child code
        int child = getpid();
        char * buf[4];
        read(p[0], buf, 4);
        printf("%d: child recieved ", child);
        write(1, buf, 4);
        write(1, "\n", 1);
        write(p[1], "ping", 4);
        exit(0);
    }

    // parent code
    write(p[1], "pong", 4);
    wait(0);

    char * buf[4];
    read(p[0], buf, 4);
    printf("%d: parent recieved ", pid);
    write(1, buf, 4);
    write(1, "\n", 1);
    exit(0);
}