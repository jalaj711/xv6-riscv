#include "kernel/types.h"
#include "user/user.h"

void primes() __attribute__((noreturn));

void primes() {

    int p[2];
    pipe(p);

    uint32 i;
    if (read(0, &i, sizeof i)<=0) {
        close(0);
        close(1);
        exit(0);
    }

    printf("%d prime\n", i);

    if (fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        primes();
    }
    else {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);

        while (1) {
            uint32 j;
            if (read(0, &j, sizeof j)<=0) {
                close(1);
                wait(0);
                exit(0);
            }
            if (j%i != 0) {
                write(1, &j, sizeof j);
            }
        }
    }
}

int main() {

    int p[2];
    pipe(p);
    if (fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        primes();
    }
    else {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        for (int i=2;i<100;i++) {
            write(1, &i, sizeof i);
        }
        close(1);
        wait(0);
        exit(0);
    }
}