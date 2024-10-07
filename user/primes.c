#include "kernel/types.h"
#include "kernel/syscall.h"
#include "user/user.h"

void primes() __attribute__((noreturn));

// recursive function to calculate the primes
void primes() {
    uint32 i;

    // if read returns 0 that means parent closed stream
    if (read(0, &i, sizeof i) <= 0) {
        close(0);
        close(1);
        exit(0);
    }
    printf("%d prime\n", i);

    // create new pipe for child processes
    int p[2];
    pipe(p);

    if (fork() == 0) {
        // inside child
        // replace stdin with read side of pipe then close the
        // pipe read and write FDs.
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        primes();
    } else {
        // replace stdout with write side of the pipe
        // and then close the pipe
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);

        while (1) {
            uint32 j;
            if (read(0, &j, sizeof j) <= 0) {
                // parent closed stream
                close(1);
                // wait for children to exit
                wait(0);
                exit(0);
            }

            if (j % i != 0) {
                // send this number to right pipe
                write(1, &j, sizeof j);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int limit;
    if (argc == 1) limit = 100;
    else {
        limit = 0;
        char *arg = argv[1];
        for (int i = 0; arg[i] != '\0'; i++)
            limit = limit * 10 + arg[i] - '0';

        if (limit < 2) {
            char *message = "error: invalid limit for primes\n";
            write(2, message, strlen(message));
            exit(1);
        }
    }

//    trace(1<<SYS_fork|1<<SYS_close|1<<SYS_pipe);
    kpgtbl();

    int p[2];
    pipe(p);
    if (fork() == 0) {
        // create first child to receive all the numbers
        // replace its stdin with this pipe
        // and then close this pipe
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        primes();
    } else {
        // replace our stdout with the pipe
        // and then close the pipe
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        for (int i = 2; i < limit; i++) {
            write(1, &i, sizeof i);
        }
        // close write fd and then wait for children to
        // finish processing
        close(1);
        wait(0);
        exit(0);
    }
}