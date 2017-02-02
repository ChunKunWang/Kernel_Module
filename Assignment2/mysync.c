#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>

// Chun-Kun'Amos' Wang
// PID: 7204-86466
// E-mail: amos@cs.unc.edu

#include "mysync.h" /* used by both kernel module and user program 
                     * to define shared parameters including the
                     * debugfs directory and file used for emulating
                     * a system call
                     */

/* define four types of operation */
#define EVENT_CREATE  0
#define EVENT_WAIT    1
#define EVENT_SIGNAL  2
#define EVENT_DESTROY 3

/* define two types of task */
#define EXCLUSIVE     1
#define NON_EXCLUSIVE 0

int file_value; // no use, just for foramt
struct dentry *dir, *file;  // used to set up debugfs file name

struct __waitq_item { // wait queue information
    bool occupied; // denote whether work quere is available
    wait_queue_head_t queue;
};

struct __task_item { // task information
    pid_t pid;
    struct task_struct *call_tsk; // replace call_task
    char respbuf[MAX_RESP]; // return buffer belong to this task
    bool occupied; // denote whether task is available
};

struct __waitq_item event[100]; // 0~99 identifiers
struct __task_item task[MAX_TASK]; // 0~99 identifiers

int t_index = 0; // task index for figure out how many task right now

static ssize_t mysync_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc;
  int index; // keep the locak index of task
  char callbuf[MAX_CALL];  // local (kernel) space to store call string

  int operation = -1; 
  bool ERROR = false; // detect any kind of error
  char *param_1, *param_2; // parse command line
  int integer_1 = -1, integer_2 = -1; // denote two integer parameters

  if(count >= MAX_CALL)
    return -EINVAL;  // return the invalid error code

  preempt_disable();  // prevents re-entry possible if one process 
                      // preempts another and it also calls this module

 /* task initialize procedure */
 if (t_index < 100 ) {
     task[t_index].pid = task_pid_nr(current);
     task[t_index].call_tsk = current;
     strcpy(task[t_index].respbuf, "");
     task[t_index].occupied = false;
     index = t_index++;
     printk(KERN_DEBUG "Sync: task %d initialize\n", index);
  }
  else return -EINVAL;

  rc = copy_from_user(callbuf, buf, count);
  callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

  /* figure out the parameters */
  param_1 = strchr(callbuf, ' ');
  if (param_1 != NULL) {
      param_1[0] = '\0';
      param_1++;

      param_2 = strchr(param_1, ' '); 
      /* if we have two integer parameters */
      if (param_2 != NULL) {
          param_2[0] = '\0';
          param_2++;

          if (kstrtoint(param_1, 10, &integer_1) == 0) {
              printk(KERN_DEBUG "parameter 2.1: %d\n", integer_1);
          }
          else ERROR = true; 

          if (kstrtoint(param_2, 10, &integer_2) == 0) {
              printk(KERN_DEBUG "parameter 2.2: %d\n", integer_2);
          }
          else ERROR = true;
      }
      else { /* deal with only one parameter */
          if (kstrtoint(param_1, 10, &integer_1) == 0) 
              printk(KERN_DEBUG "parameter 1: %d\n", integer_1);
          else ERROR = true;
      }

      if (integer_1 < 0 || integer_1 > 99) ERROR = true; // integer-1 range is 0~99
  }
  else ERROR = true; // no parameters

  // parameters error
  if (ERROR) {
      strcpy(task[index].respbuf, "-1\n");
      preempt_enable();
      return count; 
  }

  /* confirm input operation */
  if (strncmp(callbuf, "event_create", 12) == 0) operation = EVENT_CREATE;
  else if(strncmp(callbuf, "event_signal", 12) == 0) operation = EVENT_SIGNAL;
  else if(strncmp(callbuf, "event_destroy", 13) == 0) operation = EVENT_DESTROY;
  else if(strncmp(callbuf, "event_wait", 10) == 0) {
      operation = EVENT_WAIT;
      if (integer_2 != 0 && integer_2 != 1) ERROR = true;
  }
  else 
      ERROR = true;

  // operation error
  if (ERROR) {
      strcpy(task[index].respbuf, "-1\n");
      printk(KERN_DEBUG "mysync: operation error\n");
      preempt_enable();
      return count;  
  }

  /* main body of operation */
  switch (operation) {
      case EVENT_CREATE:
          printk(KERN_DEBUG "operation name: event_create\n");
          if (!event[integer_1].occupied) {
              event[integer_1].occupied = true;
              init_waitqueue_head(&event[integer_1].queue);
              sprintf(task[index].respbuf, "%d\n", integer_1);
              printk(KERN_DEBUG "%d init succeed\n", integer_1);
          }
          else strcpy(task[index].respbuf, "-1\n"); // already exist
          break;
      case EVENT_WAIT:
          printk(KERN_DEBUG "operation name: event_wait\n");
          if (event[integer_1].occupied) { // event is created
              DEFINE_WAIT(wait);

              if (integer_2 == EXCLUSIVE) {
                  add_wait_queue_exclusive(&event[integer_1].queue, &wait);
                  printk(KERN_DEBUG "mysync: %d exclusive wait\n", integer_1);
                  prepare_to_wait_exclusive(&event[integer_1].queue, &wait, TASK_INTERRUPTIBLE);
              }
              else {
                  add_wait_queue(&event[integer_1].queue, &wait);
                  printk(KERN_DEBUG "mysync: %d non-exclusive wait\n", integer_1);
                  prepare_to_wait(&event[integer_1].queue, &wait, TASK_INTERRUPTIBLE);
              }

              preempt_enable();
              printk(KERN_DEBUG "mysync: %d wait is going to shcedule\n", integer_1);
              schedule();
              preempt_disable();

              printk(KERN_DEBUG "mysync: %d wait finish waiting\n", integer_1);
              finish_wait(&event[integer_1].queue, &wait);
              sprintf(task[index].respbuf, "%d\n", integer_1);
          }
          else strcpy(task[index].respbuf, "-1\n"); // event is not created
          break;
      case EVENT_SIGNAL:
          printk(KERN_DEBUG "operation name: event_signal\n");
          if (event[integer_1].occupied) { // event is created
              wake_up(&event[integer_1].queue); 
              sprintf(task[index].respbuf, "%d\n", integer_1);
          }
          else strcpy(task[index].respbuf, "-1\n"); // event is not created
          break;
      case EVENT_DESTROY:
          printk(KERN_DEBUG "operation name: event_destroy\n");
          if (event[integer_1].occupied) { // event is created
              wake_up_all(&event[integer_1].queue); 
              sprintf(task[index].respbuf, "%d\n", integer_1);
              event[integer_1].occupied = false;
          }
          else strcpy(task[index].respbuf, "-1\n"); // event is not created
          break;
      default:
          break;
  }

  printk(KERN_DEBUG "mysync: call %s will return %s", callbuf, task[index].respbuf);
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

static ssize_t mysync_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
  int rc; 
  bool found = false;
  int index = 0;

  preempt_disable(); // protect static variables

  for (index = 0; index < 100; index++) {
      if (current == task[index].call_tsk) {
          printk(KERN_DEBUG "mysync R: index(%d), PID[%d], respbuf:%s", index, task[index].pid, task[index].respbuf);
          found = true;
          break;
      }
  }

  if (!found) { 
     printk(KERN_DEBUG "mysync R: index(%d) Not found!\n", index);
     preempt_enable();
     return 0;  // a return of zero on a read indicates no data returned
  }

  rc = strlen(task[index].respbuf) + 1; /* length includes string termination */

  /* return at most the user specified length with a string 
   * termination as the last byte.  Use the kernel function to copy
   * from kernel space to user space.
   */

  /* Use the kernel function to copy from kernel space to user space.
   */
  if (count < rc) { // user's buffer is smaller than response string
    task[index].respbuf[count - 1] = '\0'; // truncate response string
    rc = copy_to_user(userbuf, task[index].respbuf, count); // count is returned in rc
  }
  else 
    rc = copy_to_user(userbuf, task[index].respbuf, rc); // rc is unchanged

  task[index].call_tsk = NULL; // response returned so another request can be done

  preempt_enable(); // clear the disable flag

  *ppos = 0;  /* reset the offset to zero */
  return rc;  /* read() calls return the number of bytes */
} 

// Defines the functions in this module that are executed
// for user read() and write() calls to the debugfs file
static const struct file_operations my_fops = {
        .read = mysync_return,
        .write = mysync_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */

static int __init mysync_module_init(void)
{

  /* create an in-memory directory to hold the file */

  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
     printk(KERN_DEBUG "mysync: error creating %s directory\n", dir_name);
     return -ENODEV;
  }

  /* create the in-memory file used for communication;
   * make the permission read+write by "world"
   */

//0666 is file permission,
  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
  if (file == NULL) {
    printk(KERN_DEBUG "mysync: error creating %s file\n", file_name);
     return -ENODEV;
  }

  printk(KERN_DEBUG "mysync: created new debugfs directory and file\n");

  return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */

static void __exit mysync_module_exit(void)
{
  debugfs_remove(file);
  debugfs_remove(dir);
}

/* Declarations required in building a module */

module_init(mysync_module_init);
module_exit(mysync_module_exit);
MODULE_LICENSE("GPL");

