#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/mm.h>
#include "pflog.h"  

struct __task_item { // task information
    pid_t pid;
    struct task_struct *call_tsk; // replace call_task
    char respbuf[MAX_RESP]; // return buffer belong to this task
};

struct __my_vma {
    struct vm_area_struct *vma;
    struct task_struct *call_tsk;
    struct mm_struct *mm;
    struct vm_operations_struct *vm_ops;
    int (* orig_fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
};

struct __task_item task[MAX_TASK]; // 0~99 identifiers
int t_index = 0; // task index for figure out how many task right now

struct __my_vma *vma_lst[MAX_VMA];
int vm_index = 0;

int file_value; // no use, just for foramt
struct dentry *dir, *file;  // used to set up debugfs file name

static int my_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    int index = 0;
    int rval = 1000;
    unsigned long page = 0;
    unsigned long int pfn;
    struct __my_vma *vma_ptr = vma_lst[index];
    ktime_t start, end; 

    //printk(KERN_DEBUG "pflog: vma %p call my_fault\n", vma);
    while (vma_ptr != NULL && index < MAX_VMA) {
        if(vma == vma_ptr->vma) break;
        index++;
        vma_ptr = vma_lst[index];
    }

    //we have found the vma execute the original function
    page = (vmf->virtual_address != NULL) ? (((unsigned long)(vmf->virtual_address)) >> 12) : 0;

    start = ktime_get();
    rval  = vma_ptr->orig_fault(vma, vmf); //CALL THE OLD FUNCTION HERE 
    end   = ktime_get();

    printk(KERN_DEBUG "pflog: index %d NEW FAULT vma_info %p\n", index, vma_ptr); //KEEP THIS FOR DEGUGGING

    pfn = (vmf->page == NULL) ? (long unsigned int) 0 : page_to_pfn(vmf->page);

    printk(KERN_DEBUG "pflog: MM %p PAGE %lu PGOFF %lu PFN %lu TIME %llu\n"
            , vma_ptr->mm
            , page
            , vmf->pgoff
            , pfn
            , ktime_to_ns(ktime_sub(end, start)));

    return rval;
}

static ssize_t pflog_call(struct file *file, const char __user *buf,
        size_t count, loff_t *ppos)
{
    int rc;
    int index; // keep the locak index of task
    char callbuf[MAX_CALL];  // local (kernel) space to store call string
    char resp_line[MAX_LINE]; // local (kernel) space for a response
    struct vm_area_struct *my_vma;
    struct __my_vma *vma_ptr;
    //pid_t cur_pid = 0;

    if(count >= MAX_CALL)
        return -EINVAL;  // return the invalid error code
    preempt_disable();  // prevents re-entry possible if one process 

    if (t_index < 100 ) {
        task[t_index].pid = task_pid_nr(current);
        task[t_index].call_tsk = current;
        strcpy(task[t_index].respbuf, "");
        index = t_index++;
        //printk(KERN_DEBUG "pflog: task %d initialize\n", index);
    }
    else return -EINVAL;

    rc = copy_from_user(callbuf, buf, count);
    callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

    if (strcmp(callbuf, "pflog") != 0) { // only valid call is "getpid"
        strcpy(task[index].respbuf, "Failed: invalid operation\n");
        printk(KERN_DEBUG "pflog: call %s will return %s\n", callbuf, task[index].respbuf);
        preempt_enable();
        return count;  /* write() calls return the number of bytes written */
    }

    //sprintf(task[index].respbuf, "Success:\n"); // start forming a response in the buffer

    my_vma = task[index].call_tsk->mm->mmap;

    while (my_vma && vm_index < MAX_VMA ) {

        if (my_vma->vm_ops != NULL && my_vma->vm_ops->fault != NULL) {

            vma_ptr = kmalloc(sizeof(struct __my_vma), GFP_ATOMIC);
            if (vma_ptr == NULL) {
                strcpy(task[index].respbuf, "Failed: vma_ptr kmalloc\n");
                printk(KERN_DEBUG "pflog: vma_ptr kmalloc failed.\n");
                preempt_enable();
                return -ENOSPC;  
            }

            vma_ptr->vm_ops = kmalloc(sizeof(struct vm_operations_struct), GFP_ATOMIC);
            if (vma_ptr->vm_ops == NULL) {
                strcpy(task[index].respbuf, "Failed: vma_ptr->vm_ops kmalloc\n");
                printk(KERN_DEBUG "pflog: vma_ptr->vm_ops kmalloc failed.\n");
                preempt_enable();
                return -ENOSPC;  
            }

            //copy information from mm_struct
            memcpy(vma_ptr->vm_ops, my_vma->vm_ops, sizeof(struct vm_operations_struct));

            vma_ptr->call_tsk       = task[index].call_tsk;
            vma_ptr->mm             = task[index].call_tsk->mm;
            vma_ptr->vma            = my_vma;
            vma_ptr->orig_fault     = my_vma->vm_ops->fault;
            vma_ptr->vm_ops->fault  = my_fault;

            my_vma->vm_ops          = vma_ptr->vm_ops;
            vma_lst[vm_index]       = vma_ptr; 
            vm_index++;
        }
        //else printk(KERN_DEBUG "NULL\n");

        my_vma = my_vma->vm_next;
    }

    printk(KERN_DEBUG "# of VMAs: %d, vm_index: %d\n"
            , task[index].call_tsk->mm->map_count
            , vm_index);

    //cur_pid = task_pid_nr(current);
    //sprintf(resp_line, "     Current PID %d\n", cur_pid);
    sprintf(resp_line, "0\n");
    strcat(task[index].respbuf, resp_line); // finish the response

    /* Here the response has been generated and is ready for the user
     * program to access it by a read() call.
     */

    printk(KERN_DEBUG "pflog: call %s will return %s", callbuf, task[index].respbuf);
    preempt_enable();

    *ppos = 0;  /* reset the offset to zero */
    return count;  /* write() calls return the number of bytes, and unblock user program */
}

static ssize_t pflog_return(struct file *file, char __user *userbuf,
        size_t count, loff_t *ppos)
{
    int rc; 
    bool found = false;
    int index = 0;

    preempt_disable(); // protect static variables

    for (index = 0; index < 100; index++) {
        if (current == task[index].call_tsk) {
            //printk(KERN_DEBUG "pflog R: index(%d), PID[%d], respbuf:%s", index, task[index].pid, task[index].respbuf);
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
    .read = pflog_return,
    .write = pflog_call,
};

static int __init pflog_module_init(void)
{

    /* create an in-memory directory to hold the file */

    dir = debugfs_create_dir(dir_name, NULL);
    if (dir == NULL) {
        printk(KERN_DEBUG "pflog: error creating %s directory\n", dir_name);
        return -ENODEV;
    }

    /* create the in-memory file used for communication;
     * make the permission read+write by "world"
     */

    //0666 is file permission,
    file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
    if (file == NULL) {
        printk(KERN_DEBUG "pflog: error creating %s file\n", file_name);
        return -ENODEV;
    }

    printk(KERN_DEBUG "pflog: created new debugfs directory and file\n");

    return 0;
}

/* This function is called when the module is removed from the kernel
 * with rmmod.  It cleans up by deleting the directory and file and
 * freeing any memory still allocated.
 */

static void __exit pflog_module_exit(void)
{
    debugfs_remove(file);
    debugfs_remove(dir);
}

/* Declarations required in building a module */

module_init(pflog_module_init);
module_exit(pflog_module_exit);
MODULE_LICENSE("GPL");

