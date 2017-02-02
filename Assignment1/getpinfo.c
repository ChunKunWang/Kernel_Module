/*
 * Example kernel loadable module.  It illustrates the 
 * module infrastructure used in programming assignments
 * in COMP 790.  The only function is to accept
 * an emulated "system call" to getpinfo from user space
 * and returns the character representation of the 
 * Linux process ID (pid) of the caller.
 */ 
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

#include "getpinfo.h" /* used by both kernel module and user program 
                     * to define shared parameters including the
                     * debugfs directory and file used for emulating
                     * a system call
                     */

#define ERROR_LEN 35 // length for error message

/* The following two variables are global state shared between
 * the "call" and "return" functions.  They need to be protected
 * from re-entry caused by kernel preemption.
 */
/* The call_task variable is used to ensure that the result is
 * returned only to the process that made the call.  Only one
 * result can be pending for return at a time (any call entry 
 * while the variable is non-NULL is rejected).
 */

struct task_struct *call_task = NULL;
char *respbuf;  // points to memory allocated to return the result

int file_value; // no use, just for foramt
struct dentry *dir, *file;  // used to set up debugfs file name

/* This function emulates the handling of a system call by
 * accessing the call string from the user program, executing
 * the requested function and preparing a response.
 *
 * This function is executed when a user program does a write()
 * to the debugfs file used for emulating a system call.  The
 * buf parameter points to a user space buffer, and count is a
 * maximum size of the buffer content.
 *
 * The user space program is blocked at the write() call until
 * this function returns.
 */

static ssize_t getpinfo_call(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
  int rc, i;
  char callbuf[MAX_CALL];  // local (kernel) space to store call string
  char resp_line[MAX_LINE]; // local (kernel) space for a response
  unsigned char tcomm[MAX_LINE];
  struct task_struct *tsk;

  pid_t par_pid = 0;
  pid_t sib_pid = 0;

  /* the user's write() call should not include a count that exceeds
   * the size of the module's buffer for the call string.
   */

  if(count >= MAX_CALL)
    return -EINVAL;  // return the invalid error code

  /* The preempt_disable() and preempt_enable() functions are used in the
   * kernel for preventing preemption.  They are used here to protect
   * state held in the call_task and respbuf variables
   */
  
  preempt_disable();  // prevents re-entry possible if one process 
                      // preempts another and it also calls this module

  if (call_task != NULL) { // a different process is expecting a return
     preempt_enable();  // must be enabled before return
     return -EAGAIN;
  }

  // allocate some kernel memory for the response
  respbuf = kmalloc(MAX_RESP, GFP_ATOMIC);
  if (respbuf == NULL) {  // always test if allocation failed
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

  if (strcmp(callbuf, "getpinfo") != 0) { // only valid call is "getpid"
      strcpy(respbuf, "Failed: invalid operation\n");
      printk(KERN_DEBUG "getpinfo: call %s will return %s\n", callbuf, respbuf);
      preempt_enable();
      return count;  /* write() calls return the number of bytes written */
  }

  sprintf(respbuf, "Success:\n"); // start forming a response in the buffer

  /* Use kernel functions for access to pid for a process 
   */
 
  /* Part 3: Extend getpinfo */
  /* Visit all sibling nodes, and show getpinfo */
  list_for_each_entry(tsk, &current->parent->children, sibling) {

      sib_pid = task_pid_nr(tsk);
      get_task_comm(tcomm, tsk);
      printk(KERN_DEBUG "[Amos] sibling: %s-%d\n", tcomm, sib_pid);

      sprintf(resp_line, "%s\n", tcomm);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP) // test buffer overflow
          strcat(respbuf, resp_line); // finish the response
      else goto END; // jump to the procedure for pop up error message

      sprintf(resp_line, "   Current PID %d\n", sib_pid);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    command caller\n"); // start forming a response in the buffer
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      par_pid = task_pid_nr(tsk->real_parent);
      sprintf(resp_line, "    parent PID %d\n", par_pid);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    state %lu\n", tsk->state);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    flags %08x\n", tsk->flags);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    priority %d\n", tsk->normal_prio);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    VM areas %d\n", tsk->mm->map_count);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    VM exec %lu\n", tsk->mm->exec_vm);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    VM stack %lu\n", tsk->mm->stack_vm);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      sprintf(resp_line, "    VM total %lu\n", tsk->mm->total_vm);
      if (strlen(respbuf) + strlen(resp_line) < MAX_RESP)
          strcat(respbuf, resp_line); // finish the response
      else goto END;

      //if (strlen(respbuf) > MAX_RESP) {
      if (strlen(respbuf) + ERROR_LEN >= MAX_RESP) {
END: // the procedure of dealing with buffer overflow
          printk(KERN_DEBUG "[Amos] error buf size: %d/%d\n", strlen(respbuf), MAX_RESP);
	      for (i = 0; i < ERROR_LEN; i++) { // erease space in buffer for error message
	          respbuf[MAX_RESP - i] = '\0';
          }
          sprintf(resp_line, "\nerror: respuf size overflow!\n"); // cannot exceed ERROR_LEN
          strcat(respbuf, resp_line); // add error message in the end of buffer
          break; // stop visiting other sibling and exist
      } 

  }

  printk(KERN_DEBUG "[Amos] length of respbuf: %d\n", (int)strlen(respbuf));

  /* Here the response has been generated and is ready for the user
   * program to access it by a read() call.
   */

  printk(KERN_DEBUG "getpinfo: call %s will return %s", callbuf, respbuf);
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

static ssize_t getpinfo_return(struct file *file, char __user *userbuf,
                                size_t count, loff_t *ppos)
{
  int rc; 

  preempt_disable(); // protect static variables

  if (current != call_task) { // return response only to the process making
                              // the getpinfo request
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
        .read = getpinfo_return,
        .write = getpinfo_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */

static int __init getpinfo_module_init(void)
{

  /* create an in-memory directory to hold the file */

  dir = debugfs_create_dir(dir_name, NULL);
  if (dir == NULL) {
    printk(KERN_DEBUG "getpinfo: error creating %s directory\n", dir_name);
     return -ENODEV;
  }

  /* create the in-memory file used for communication;
   * make the permission read+write by "world"
   */

//0666 is file permission,
  file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
  if (file == NULL) {
    printk(KERN_DEBUG "getpinfo: error creating %s file\n", file_name);
     return -ENODEV;
  }

  printk(KERN_DEBUG "getpinfo: created new debugfs directory and file\n");

  return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */

static void __exit getpinfo_module_exit(void)
{
  debugfs_remove(file);
  debugfs_remove(dir);
  if (respbuf != NULL)
     kfree(respbuf);
}

/* Declarations required in building a module */

module_init(getpinfo_module_init);
module_exit(getpinfo_module_exit);
MODULE_LICENSE("GPL");
