#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "amos_perf.h" /* used by both kernel module and user program */


void do_syscall(char *call_string);  // does the call emulation

// variables shared between main() and the do_syscall() function
int fp;
char the_file[256] = "/sys/kernel/debug/";
char call_buf[MAX_CALL];  /* no call string can be longer */
char resp_buf[MAX_RESP];  /* no response strig can be longer */

void main (int argc, char* argv[])
{
    int i;
    int rc = 0;
    char input[MAX_CALL] = "\0";
    pid_t my_pid;  

    my_pid = getpid();
    fprintf(stdout, "System call getpid() returns %d\n", my_pid);

    strcat(the_file, dir_name);
    strcat(the_file, "/");
    strcat(the_file, file_name);

    if ((fp = open (the_file, O_RDWR)) == -1) {
        fprintf (stderr, "error opening %s\n", the_file);
        exit (-1);
    }

    i = 1;
    while (i < argc) {
        strcat(input, argv[i]);
        if (i < argc - 1) strcat(input, " ");
        i++;
    }

    do_syscall(input);

    fprintf(stdout, "%s", resp_buf); 
    close(fp);
}

void do_syscall(char *call_string)
{
    int rc;

    strcpy(call_buf, call_string);

    rc = write(fp, call_buf, strlen(call_buf) + 1);
    if (rc == -1) {
        fprintf (stderr, "error writing %s\n", the_file);
        fflush(stderr);
        exit (-1);
    }

    rc = read(fp, resp_buf, sizeof(resp_buf));
    if (rc == -1) {
        fprintf (stderr, "error reading %s\n", the_file);
        fflush(stderr);
        exit (-1);
    }
}

