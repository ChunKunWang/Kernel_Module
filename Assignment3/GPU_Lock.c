#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/rtmutex.h>
// Chun-Kun'Amos' Wang
// PID: 7204-86466
// E-mail: amos@cs.unc.edu

#include "GPU_Locks_kernel.h" 

/* define four types of operation */
#define CE_LOCK    0
#define CE_UNLOCK  1
#define EE_LOCK    2
#define EE_UNLOCK  3

int file_value; // no use, just for foramt
struct dentry *dir, *file;  // used to set up debugfs file name

struct __task_item { // task information
    pid_t pid;
    struct task_struct *call_tsk; // replace call_task
    char respbuf[200]; // return buffer belong to this task
};

struct __task_item task[100]; // 0~99 identifiers

/* for real time mutex */
static DEFINE_RT_MUTEX(CE);
static DEFINE_RT_MUTEX(EE);
/* regular mutex */
//static DEFINE_MUTEX(CE);
//static DEFINE_MUTEX(EE);

struct task_struct *CE_task; // who occupy CE
int CE_who = -1;             // task index of who occupy CE
int CE_notice = 1;           // counter for how many times resource is occupied by lower prior task

struct task_struct *EE_task; // who occupy EE
int EE_who = -1;             // task index of who occupy EE
int EE_notice = 1;           // counter for how many times resource is occupied by lower prior task

int t_index = 0;             // task index for figure out how many task right now

static ssize_t GPU_Lock_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc, i=0;
  int index; // keep the locak index of task
  char callbuf[MAX_CALL];  // local (kernel) space to store call string
  char resp_line[MAX_CALL];

  int operation = -1; 
  bool ERROR = false; // detect any kind of error
  bool NEW = true;

  if(count >= MAX_CALL)
    return -EINVAL;  // return the invalid error code

  preempt_disable();  // prevents re-entry possible if one process 
                      // preempts another and it also calls this module

  /* task initialize procedure */
  if (t_index < 100 ) {
       while (i <= t_index) {
           if (task[i].call_tsk == current) { // old task id
               index = i;
               NEW = false;
               break;
           }
           i++;
       }

       if (NEW) {
               if(t_index == 0) printk(KERN_DEBUG "GPU_Lock: New_Start!\n");
               task[t_index].pid = task_pid_nr(current);
               task[t_index].call_tsk = current;
               strcpy(task[t_index].respbuf, "");
               index = t_index++;
               printk(KERN_DEBUG "GPU_Lock: task %d, pid %d, initialize\n", index, task[index].pid);
       }
   }
   else return -EINVAL;

  rc = copy_from_user(callbuf, buf, count);
  callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

  /* confirm input operation */
  if (strncmp(callbuf, "CE_Lock", 7) == 0) operation = CE_LOCK;
  else if(strncmp(callbuf, "CE_UnLock", 9) == 0) operation = CE_UNLOCK;
  else if(strncmp(callbuf, "EE_Lock", 7) == 0) operation = EE_LOCK;
  else if(strncmp(callbuf, "EE_UnLock", 9) == 0) operation = EE_UNLOCK;
  else ERROR = true;

  //printk(KERN_DEBUG "GPU_Lock: operation input is %s\n", callbuf);
  // operation error
  if (ERROR) {
      strcpy(task[index].respbuf, "-1\n");
      printk(KERN_DEBUG "GPU_Lock: operation error %s\n", callbuf);
      preempt_enable();
      return count;  
  }

  /* main body of operation */
  switch (operation) {
      case CE_LOCK:
          if( index < CE_who && CE_who > 0 ) printk(KERN_DEBUG "CE %d Notice!!!\n", CE_notice++);
          //printk(KERN_DEBUG "t[%d]:%d waiting for CE_Lock\n", index, task[index].pid);

          if (CE_task != current && current != EE_task) {
              rt_mutex_lock(&CE);
              //mutex_lock(&CE);
	          CE_who = index;
              CE_task = current;
              //printk(KERN_DEBUG "t[%d]:%d get CE_Lock\n", index, task[index].pid);
              sprintf(resp_line, "%d", 0);
              strcpy(task[index].respbuf, resp_line);
          }
          else {
              //printk(KERN_DEBUG "GPU_Lock: CE_LOCK double mutex, t[%d]:%d\n", index, task[index].pid);
              sprintf(resp_line, "%d\n", -1); // double mutex
              strcpy(task[index].respbuf, resp_line);
          }
          break;
      case CE_UNLOCK:
          //printk(KERN_DEBUG "t[%d]:%d attempt CE_UnLock\n", index, task[index].pid);

          if (CE_task == current && EE_task != current) {
              CE_task = NULL;
              //printk(KERN_DEBUG "t[%d]:%d get CE_UnLock\n", index, task[index].pid);
              sprintf(resp_line, "%d", 0);
              strcpy(task[index].respbuf, resp_line);
	          CE_who = -1;
              //mutex_unlock(&CE);
              rt_mutex_unlock(&CE);
          }
          else {
              //printk(KERN_DEBUG "GPU_Lock: CE_UNLOCK got wrong task, t[%d]:%d\n", index, task[index].pid);
              sprintf(resp_line, "%d", -1); // double mutex
              strcpy(task[index].respbuf, resp_line);
          }
          break;
      case EE_LOCK:
          if( index < EE_who && EE_who > 0 ) printk(KERN_DEBUG "EE %d Notice!!!\n", EE_notice++);
          //printk(KERN_DEBUG "t[%d]:%d---------------> waiting for EE_Lock\n", index, task[index].pid);

          if (EE_task != current && current != CE_task) {
              rt_mutex_lock(&EE);
              //mutex_lock(&EE);
	          EE_who = index;
              EE_task = current;
              //printk(KERN_DEBUG "t[%d]:%d---------------> get EE_Lock\n", index, task[index].pid);
              sprintf(resp_line, "%d\n", 0);
              strcpy(task[index].respbuf, resp_line);
          }
          else {
              //printk(KERN_DEBUG "GPU_Lock: EE_LOCK double mutex, t[%d]:%d\n", index, task[index].pid);
              sprintf(resp_line, "%d\n", -1); // double mutex
              strcpy(task[index].respbuf, resp_line);
          }
          break;
      case EE_UNLOCK:
          //printk(KERN_DEBUG "t[%d]:%d---------------> attempt EE_UnLock\n", index, task[index].pid);

          if (EE_task == current && CE_task != current) {
              EE_task = NULL;
              //printk(KERN_DEBUG "t[%d]:%d---------------> get EE_UnLock\n", index, task[index].pid);
              sprintf(resp_line, "%d", 0);
              strcpy(task[index].respbuf, resp_line);
	          CE_who = -1;
              //mutex_unlock(&EE);
              rt_mutex_unlock(&EE);
          }
          else {
              //printk(KERN_DEBUG "GPU_Lock: EE_UNLOCK got wrong task, t[%d]:%d\n", index, task[index].pid);
              sprintf(resp_line, "%d", -1); // double mutex
              strcpy(task[index].respbuf, resp_line);
          }
          break;
      default:
          break;
  }

  //printk(KERN_DEBUG "GPU_Lock: call %s will return %s", callbuf, task[index].respbuf);
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

static ssize_t GPU_Lock_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
  int rc; 
  bool found = false;
  int index = 0;

  preempt_disable(); // protect static variables

  for (index = 0; index < 100; index++) {
      if (current == task[index].call_tsk) {
          printk(KERN_DEBUG "GPU_Lock R: index(%d), PID[%d], respbuf:%s", index, task[index].pid, task[index].respbuf);
          found = true;
          break;
      }
  }

  if (!found) { 
     printk(KERN_DEBUG "GPU_Lock R: index(%d) Not found!\n", index);
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
        .read = GPU_Lock_return,
        .write = GPU_Lock_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */

static int __init GPU_Lock_module_init(void)
{

  /* create an in-memory directory to hold the file */

  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
     printk(KERN_DEBUG "GPU_Lock: error creating %s directory\n", dir_name);
     return -ENODEV;
  }

  /* create the in-memory file used for communication;
   * make the permission read+write by "world"
   */

//0666 is file permission,
  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
  if (file == NULL) {
    printk(KERN_DEBUG "GPU_Lock: error creating %s file\n", file_name);
     return -ENODEV;
  }

  printk(KERN_DEBUG "GPU_Lock: created new debugfs directory and file\n");

  return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */

static void __exit GPU_Lock_module_exit(void)
{
  debugfs_remove(file);
  debugfs_remove(dir);
}

/* Declarations required in building a module */

module_init(GPU_Lock_module_init);
module_exit(GPU_Lock_module_exit);
MODULE_LICENSE("GPL");

