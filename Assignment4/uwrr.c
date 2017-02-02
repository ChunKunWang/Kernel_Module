#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

// Chun-Kun'Amos' Wang
// PID: 7204-86466
// E-mail: amos@cs.unc.edu

#include "SchedCl.h"
#include "uwrr.h" /* used by both kernel module and user program 
                   * to define shared parameters including the
                   * debugfs directory and file used for emulating
                   * a system call
                   */
#define UWRR 0

struct __uwrr_task {
  struct list_head mylist;
  struct task_struct *p;
  pid_t pid;
  unsigned int tick_count;
  u64 start;
  u64 end;
  int weight;
};
LIST_HEAD(uwrr_lst);

void (* task_tick_orig) (void *rq, struct task_struct *p, int queued);

struct task_struct *call_task = NULL;
struct sched_class *my_sched_class = NULL;
struct sched_param rt_param = {.sched_priority = 1}; 
bool FIRST_CALL = true;
char *respbuf;  // points to memory allocated to return the result

int file_value; // no use, just for foramt
struct dentry *dir, *file;  // used to set up debugfs file name

static struct __uwrr_task *uwrr_per_task(pid_t pid)
{
  struct __uwrr_task *task_ptr = NULL;
  
  list_for_each_entry (task_ptr, &uwrr_lst, mylist) {
      if (task_ptr->pid == pid) {
          return task_ptr;
      }
  }
  return NULL;
}

static void print_time(struct __uwrr_task *task_ptr) 
{
  u64 exe_time;

  task_ptr->end = (u64)ktime_to_us(ktime_get());
  exe_time = task_ptr->end - task_ptr->start;

  //printk(KERN_DEBUG "HZ %i; MY_TIMESLICE %d\n", HZ, MY_TIMESLICE);
  printk(KERN_DEBUG "p[%d],w[%d],t[%u],s[%d],exe %llu from %llu to %llu\n"
                            , task_ptr->pid
                            , task_ptr->weight
                            , task_ptr->tick_count
                            , task_ptr->p->rt.time_slice
                            , exe_time
                            , task_ptr->start
                            , task_ptr->end);

  return;
}

static void uwrr_task_tick(void *rq, struct task_struct *p, int queued)
{
  struct __uwrr_task *task_ptr = uwrr_per_task(p->pid);

  if (task_ptr == NULL) return;
  
  task_ptr->tick_count++;
 
  if (p->rt.time_slice) {
      //print_time(task_ptr);
      --p->rt.time_slice;
      return;
  }
  else {
      task_tick_orig(rq, p, queued); // call original task_tick
      p->rt.time_slice = 0;
      print_time(task_ptr);
      p->rt.time_slice = task_ptr->weight * MY_TIMESLICE;
  }

  return;
}

static ssize_t uwrr_call(struct file *file, const char __user *buf,
        size_t count, loff_t *ppos)
{
    int rc;
    int sched_change;
    int operation;
    char callbuf[MAX_CALL];  // local (kernel) space to store call string

    struct __uwrr_task *task_info;

    bool ERROR = false; // detect any kind of error
    char *param; // parse command line
    int integer = -1;

    if(count >= MAX_CALL)
        return -EINVAL;  // return the invalid error code

    preempt_disable();  // prevents re-entry possible if one process 
    // preempts another and it also calls this module

    // allocate some kernel memory for the response
    respbuf = kmalloc(MAX_RESP, GFP_ATOMIC);
    if (respbuf == NULL) {  // always test if allocation failed
        printk(KERN_DEBUG "uwrr: respbuf error\n");
        preempt_enable(); 
        return -ENOSPC;
    }

    strcpy(respbuf,""); /* initialize buffer with null string */

    /* current is global for the kernel and contains a pointer to the
     * task_struct for the running process 
     */
    call_task = current; // it's micro. can use anywhere, and return the pointer that task is executing this module 

    /* Use the kernel function to copy from user space to kernel space.
    */

    rc = copy_from_user(callbuf, buf, count);
    callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

    /* figure out the parameters */
    param = strchr(callbuf, ' ');
    if (param != NULL) {
        param[0] = '\0';
        param++;

        if (kstrtoint(param, 10, &integer) == 0)
            if ( integer > 0 && integer <= 20 ) ;//printk(KERN_DEBUG "integer: %d\n", integer);
            else ERROR = true;
        else ERROR = true;
    }
    else ERROR = true; // no parameters

    // parameters error
    if (ERROR) {
        strcpy(respbuf, "-1\n");
        preempt_enable();
        return count; 
    }

    /* confirm input operation */
    if (strncmp(callbuf, "uwrr", 4) == 0) operation = UWRR;
    else ERROR = true;

    // operation error
    if (ERROR) {
        strcpy(respbuf, "-1\n");
        printk(KERN_DEBUG "uwrr: operation error\n");
        preempt_enable();
        return count;  
    }

    /* main body of operation */
    switch (operation) {
        case UWRR:
            // change policy to SCHED_RR
            sched_change = sched_setscheduler(call_task, SCHED_RR, &rt_param);
            if (sched_change != 0) {
                printk(KERN_DEBUG "uwrr: PID %i change RR policy fails\n", call_task->pid);
                strcpy(respbuf, "-1\n");
                preempt_enable();
                return count; 
            }

            if (FIRST_CALL) {
                my_sched_class = kmalloc(sizeof(struct sched_class), GFP_ATOMIC);
                if (my_sched_class == NULL) {
                    preempt_enable();
                    return -ENOSPC;
                }
                memcpy(my_sched_class, call_task->sched_class, sizeof(struct sched_class)+1);
                my_sched_class->task_tick = uwrr_task_tick;
                task_tick_orig = call_task->sched_class->task_tick;
                FIRST_CALL = false;
            }

            task_info = kmalloc(sizeof(struct __uwrr_task), GFP_ATOMIC);
            if (task_info == NULL) {
                preempt_enable();
                return -ENOSPC;
            }

            task_info->p          = call_task;
            task_info->pid        = call_task->pid;
            task_info->tick_count = 0;
            task_info->start      = (u64)ktime_to_us(ktime_get());
            task_info->weight     = integer;
            task_info->p->rt.time_slice = task_info->weight * MY_TIMESLICE;
            list_add(&(task_info->mylist), &uwrr_lst);

            printk(KERN_DEBUG "uwrr: PID %i task_tick_orig: %p uwrr_task_tick: %p\n"
                    , call_task->pid
                    , task_tick_orig
                    , my_sched_class->task_tick);

            call_task->sched_class = my_sched_class;

            strcpy(respbuf, "0\n");
            break;
        default:
            break;
    }

    printk(KERN_DEBUG "uwrr: call %s will return %s", callbuf, respbuf);
    preempt_enable();

    *ppos = 0;  /* reset the offset to zero */
    return count;  /* write() calls return the number of bytes, and unblock user program */
}


/* This function emulates the return from a system call by returning
 * the response to the user as a character string.  It is executed 
 * when the user program does a read() to the debugfs file used for 
 * emulating a system call.  The buf parameter points to a user space 
 * buffer, and count is a maximum size of the buffer space. 
 * 
 * The user space program is blocked at the read() call until this 
 * function returns.
 */

static ssize_t uwrr_return(struct file *file, char __user *userbuf,
        size_t count, loff_t *ppos)
{
    int rc; 

    preempt_disable(); // protect static variables

    if (current != call_task) { // return response only to the process making
        // the getpid request
        preempt_enable();
        return 0;  // a return of zero on a read indicates no data returned
    }

    rc = strlen(respbuf) + 1; /* length includes string termination */

    /* return at most the user specified length with a string 
     * termination as the last byte.  Use the kernel function to copy
     * from kernel space to user space.
     */

    /* Use the kernel function to copy from kernel space to user space.
    */
    if (count < rc) { // user's buffer is smaller than response string
        respbuf[count - 1] = '\0'; // truncate response string
        rc = copy_to_user(userbuf, respbuf, count); // count is returned in rc
    }
    else 
        rc = copy_to_user(userbuf, respbuf, rc); // rc is unchanged

    kfree(respbuf); // free allocated kernel space
    respbuf = NULL;
    call_task = NULL; // response returned so another request can be done

    preempt_enable(); // clear the disable flag

    *ppos = 0;  /* reset the offset to zero */
    return rc;  /* read() calls return the number of bytes */
} 

// Defines the functions in this module that are executed
// for user read() and write() calls to the debugfs file
static const struct file_operations my_fops = {
    .read = uwrr_return,
    .write = uwrr_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */

static int __init uwrr_module_init(void)
{

    /* create an in-memory directory to hold the file */

    dir = debugfs_create_dir(uwrr_dir_name, NULL);
    if (dir == NULL) {
        printk(KERN_DEBUG "uwrr: error creating %s directory\n", uwrr_dir_name);
        return -ENODEV;
    }

    /* create the in-memory file used for communication;
     * make the permission read+write by "world"
     */

    //0666 is file permission,
    file = debugfs_create_file(uwrr_file_name, 0666, dir, &file_value, &my_fops);
    if (file == NULL) {
        printk(KERN_DEBUG "uwrr: error creating %s file\n", uwrr_file_name);
        return -ENODEV;
    }

    printk(KERN_DEBUG "uwrr: created new debugfs directory and file\n");

    return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */

static void __exit uwrr_module_exit(void)
{
    if (respbuf != NULL) kfree(respbuf);
    if (my_sched_class != NULL) kfree(my_sched_class);

    debugfs_remove(file);
    debugfs_remove(dir);
}

/* Declarations required in building a module */

module_init(uwrr_module_init);
module_exit(uwrr_module_exit);
MODULE_LICENSE("GPL");

