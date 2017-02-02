#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "uwrr.h" /* used by both kernel module and user program */
#include "../Assignment2/mysync.h"

#define MAX_TIME  10  //run time in seconds
void mysync_do_syscall(char *call_string);  // does the call emulation
void uwrr_do_syscall(char *call_string);  // does the call emulation

// variables shared between main() and the do_syscall() function
int mysync_fp;
int uwrr_fp;
char uwrr_the_file[256] = "/sys/kernel/debug/";
char mysync_the_file[256] = "/sys/kernel/debug/";
char call_buf[MAX_CALL];  /* no call string can be longer */
char mysync_resp_buf[MAX_RESP];  /* no response strig can be longer */
char uwrr_resp_buf[MAX_RESP];  /* no response strig can be longer */

void main (int argc, char* argv[])
{
  int i;
  int rc = 0;
  char input[MAX_CALL] = "\0";

  unsigned long count = 0;
  pid_t my_pid;
  time_t start, elapsed;
  /* Build the complete file path name and open the file */

  strcat(mysync_the_file, dir_name);
  strcat(mysync_the_file, "/");
  strcat(mysync_the_file, file_name);

  if ((mysync_fp = open (mysync_the_file, O_RDWR)) == -1) {
      fprintf (stderr, "mysync error opening %s\n", mysync_the_file);
      exit (-1);
  }

  mysync_do_syscall("event_create 99\n");
  //fprintf(stdout, "Module mysync returns %s", mysync_resp_buf);

  strcat(uwrr_the_file, uwrr_dir_name);
  strcat(uwrr_the_file, "/");
  strcat(uwrr_the_file, uwrr_file_name);

  if ((uwrr_fp = open (uwrr_the_file, O_RDWR)) == -1) {
      fprintf (stderr, "uwrr error opening %s\n", uwrr_the_file);
      exit (-1);
  }

  i = 1;
  while (i < argc) {
      strcat(input, argv[i]);
      if (i < argc - 1) strcat(input, " ");
      i++;
  }
  //printf("input: %s\n", input);

  // use the system call to get the pid
  my_pid = getpid();
  fprintf(stdout, "System call uwrr() returns %d\n", my_pid);

  // use the kernel module to get the pid
  uwrr_do_syscall(input);

  mysync_do_syscall("event_wait 99 0\n");
  //fprintf(stdout, "Module mysync returns %s", mysync_resp_buf);

  start = time(NULL);
  elapsed = 0;
  while (elapsed < MAX_TIME) {
    crypt("This is my lazy password", "A1");
    count++;
    elapsed = time(NULL) - start;
  }
  fprintf(stdout, "PID %d w(%s) Elapsed time %ld iterations %ld\n", my_pid, argv[2], elapsed, count);
  fprintf(stdout, "Module uwrr returns %s", uwrr_resp_buf);

  close (mysync_fp);
  close (uwrr_fp);
} /* end main() */

/* 
 * A function to actually emulate making a system call by
 * writing the request to the debugfs file and then reading
 * the response.  It encapsulates the semantics of a regular
 * system call in that the calling process is blocked until
 * both the request (write) and response (read) have been
 * completed by the kernel module.
 *  
 * The input string should be properly formatted for the
 * call string expected by the kernel module using the
 * specified debugfs path (this function does no error
 * checking of input).
 */ 
void mysync_do_syscall(char *call_string)
{
  int rc;

  strcpy(call_buf, call_string);

  rc = write(mysync_fp, call_buf, strlen(call_buf) + 1);
  if (rc == -1) {
     fprintf (stderr, "mysync error writing %s\n", mysync_the_file);
     fflush(stderr);
     exit (-1);
  }

  rc = read(mysync_fp, mysync_resp_buf, sizeof(mysync_resp_buf));
  if (rc == -1) {
     fprintf (stderr, "mysync error reading %s\n", mysync_the_file);
     fflush(stderr);
     exit (-1);
  }
}

void uwrr_do_syscall(char *call_string)
{
  int rc;

  strcpy(call_buf, call_string);

  rc = write(uwrr_fp, call_buf, strlen(call_buf) + 1);
  if (rc == -1) {
     fprintf (stderr, "uwrr error writing %s\n", uwrr_the_file);
     fflush(stderr);
     exit (-1);
  }

  rc = read(uwrr_fp, uwrr_resp_buf, sizeof(uwrr_resp_buf));
  if (rc == -1) {
     fprintf (stderr, "uwrr error reading %s\n", uwrr_the_file);
     fflush(stderr);
     exit (-1);
  }
}

