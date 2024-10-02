/**
 * Unix like find utility
 *
 * Usage: find <path> <search>
 *
 * Recursively searches for a file or directory of the name
 * <search>. Each search result is written to stdout in a new
 * line.
 */

#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

/*
 * Function that takes a path string and a search string
 * and tells whether this is the path we were looking for.
 * It compares the part of the string after the last '/' with
 * the search string.
 *
 * Returns 0 if not equal and 1 if equal.
 *
 * Example:
 *  cmpfile("/a/b/c", "c") -> 1
 *  cmpfile("/a/b/c/d", "c") -> 0
 */
int cmpfile(char *path, char*search)
{
    char *p = path+strlen(path);
    char *s = search+strlen(search);

    while(p >= path && *p != '/' && s >= search) {
        if (*s != *p) return 0;
        p--;
        s--;
    }
    return 1;
}

void find(char * path, char * search) {
    // buf stores the complete path to the children
    // p is the pointer in buffer
    char buf[512], *p;

    // file descriptor of current path
    int fd;

    // struct to get each entry of directory
    struct dirent de;

    // stores the stats of this directory
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        char * m = "find: can not open file\n";
        write(2, m, strlen(m));
        exit(1);
    }


    if ((fstat(fd, &st) < 0)) {
        char * m = "find: can not stat file\n";
        write(2, m, strlen(m));
        exit(1);
    }

    // check if this is already the correct file
    if (cmpfile(path, search)) {
        printf("%s\n", path);
    }

    // only operate if this is a directory not a file
    if(st.type == T_DIR){

        // combined path size will become too long
        // we can only store upto 512 bytes
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("find: path too long\n");
            return;
        }

        strcpy(buf, path);
        p = buf+strlen(buf);
        *p++ = '/';

        // keep reading from fd till there are entries
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0 || de.name[0] == '.')
                continue;
            // DIRSZ stores the size of name of each directory
            // defined as 14 bytes in kernel/fs.h
            //
            // copy name into buf to create new path
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            // recursively call find
            find(buf, search);
        }
    }
    close(fd);
}

int main (int argc, char * argv[]) {
    if (argc < 3) {
        char * m = "find: error: provide filename to search\n";
        write(2, m, strlen(m));
        exit(1);
    }

    char * path = argv[1];
    char * search = argv[2];

    find(path, search);
}