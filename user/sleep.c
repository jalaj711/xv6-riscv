#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc==1) {
        char * message = "error: sleep count is required\n";
        write(2, message, strlen(message));
        exit(1);
    }

    int t = 0;
    char * time = argv[1];
    for (int i=0;time[i] != '\0';i++) {
        t = t*10 + (time[i]-'0');
    }

    sleep(t);

    exit(0);
}