#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <omp.h>
#include <syscall.h>
#include <time.h>

#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>

#include "amos_perf.h" /* used by both kernel module and user program */

#define NUM_THREADS 4

void do_syscall(char *call_string);  // does the call emulation
long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
        int cpu, int group_fd, unsigned long flags);

// variables shared between main() and the do_syscall() function
int fp;
char the_file[256] = "/sys/kernel/debug/";
char call_buf[MAX_CALL];  /* no call string can be longer */
char resp_buf[MAX_RESP];  /* no response strig can be longer */

int fd[NUM_THREADS];
struct perf_event_attr pe[NUM_THREADS];
long long count[NUM_THREADS];
char input_line[NUM_THREADS][100];

void initial_perf_event(void);

void main (int argc, char* argv[])
{
    int i;
    int rc = 0;
    pid_t my_pid;  
    time_t start, elapsed;

    int tid = 0;
    int sid = 0;

    my_pid = getpid();
    fprintf(stdout, "System call getpid() returns %d\n", my_pid);

    strcat(the_file, dir_name);
    strcat(the_file, "/");
    strcat(the_file, file_name);


    if ((fp = open (the_file, O_RDWR)) == -1) {
        fprintf (stderr, "error opening %s\n", the_file);
        exit (-1);
    }
    do_syscall("PERF_REGISTER");

    initial_perf_event();

#pragma omp parallel num_threads(NUM_THREADS) private(tid, sid, start, elapsed, count)
{
    do_syscall("PERF_DETECT");

    sid = syscall(SYS_gettid);
    tid = omp_get_thread_num();

    ioctl(fd[tid], PERF_EVENT_IOC_RESET, 0);
    ioctl(fd[tid], PERF_EVENT_IOC_ENABLE, 0);

    start = time(NULL);
    elapsed = 0;
    while (elapsed < 4*(tid+1)) {
        crypt("This is my lazy password", "A1");
        elapsed = time(NULL) - start;
    }

    ioctl(fd[tid], PERF_EVENT_IOC_DISABLE, 0);
    read(fd[tid], &count[tid], sizeof(long long));

    sprintf(input_line[tid], "PERF_REPORT Time %3lld Page_Fault %lld"
            , (long long)elapsed
            , count[tid]);

    printf("tid: %d, sid:%d, Page_Fault %lld\n", tid, sid, count[tid]);
    do_syscall(input_line[tid]);
}

    //do_syscall("PERF_LIST");
    //fprintf(stdout, "amos-perf: %s", resp_buf);

    for (i = 0; i < NUM_THREADS; i++) close(fd[i]);

    close(fp);
} 

void initial_perf_event(void)
{
    int i;

    for (i=0; i < NUM_THREADS; i++) {
        memset(&pe[i], 0, sizeof(struct perf_event_attr));
        pe[i].type = PERF_TYPE_SOFTWARE;
        pe[i].size = sizeof(struct perf_event_attr);
        pe[i].config = PERF_COUNT_SW_PAGE_FAULTS;
        pe[i].disabled = 1;
        pe[i].exclude_kernel = 1;
        pe[i].exclude_hv = 1;
        fd[i] = perf_event_open(&pe[i], 0, -1, -1, 0);
        if (fd[i] == -1) {
            fprintf(stderr, "Error opening leader pe[%d]   %llx\n", i, pe[i].config);
            exit(EXIT_FAILURE);
        }
    }
}

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
        int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
            group_fd, flags);
    return ret;
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

