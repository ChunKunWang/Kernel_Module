#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/perf_event.h>
#include <asm/unistd.h>

#include "amos_perf.h" 

#define PERF_REGISTER   0
#define PERF_DETECT     1
#define PERF_REPORT     2
#define PERF_LIST       3
#define PERF_LIST_ALL   4
#define PERF_RESET      5

struct __task_item { // task information
    int op_s;
    pid_t pid;
    struct task_struct *call_tsk;  // replace call_task
    unsigned char tcomm[MAX_LINE];
    char respbuf[MAX_RESP];        // return buffer belong to this task
};
struct __task_item task[MAX_TASK]; // 0~200 identifiers
int t_index = 0;

struct __task_table {
    int op;
    int root;
    int num_thread;
    pid_t pid;
    pid_t tgid;
    u64 start;
    u64 end;
    unsigned char tcomm[MAX_LINE];
    struct task_struct *call_tsk;  // replace call_task
    char respbuf[MAX_RESP];        // return buffer belong to this task
};
struct __task_table table[MAX_TASK]; // 0~200 identifiers
int tab_index = -1;

int file_value; // no use, just for foramt
struct dentry *dir, *file;  // used to set up debugfs file name

static int Create_new_task(int operation);
static int Add_table_entry(int operation, char *report);
static int List_table_entry(int root_index);
static int Print_entry(int index);

static ssize_t amos_perf_call(struct file *file, const char __user *buf,
        size_t count, loff_t *ppos)
{
    int rc;
    int index;                 // keep the locak index of task
    char callbuf[MAX_CALL];    // local (kernel) space to store call string
    char *param;   // parse command line
    int root_index = -1;        // denote integer parameters

    int operation = -1;
    bool ERROR    = false;     // detect any kind of error

    if(count >= MAX_CALL) return -EINVAL;        // return the invalid error code

    preempt_disable();         // prevents re-entry possible if one process 

    rc = copy_from_user(callbuf, buf, count);
    callbuf[MAX_CALL - 1] = '\0'; /* make sure it is a terminated string */

    if (strncmp(callbuf, "PERF_REGISTER", 13) == 0) operation = PERF_REGISTER;
    else if(strncmp(callbuf, "PERF_DETECT", 11) == 0) operation = PERF_DETECT;
    else if(strncmp(callbuf, "PERF_REPORT", 11) == 0) operation = PERF_REPORT;
    else if(strncmp(callbuf, "PERF_LIST_ALL", 13) == 0) operation = PERF_LIST_ALL;
    else if(strncmp(callbuf, "PERF_LIST", 9) == 0) operation = PERF_LIST;
    else if(strncmp(callbuf, "PERF_RESET", 10) == 0) operation = PERF_RESET;
    else ERROR = true;

    index = Create_new_task(operation);
    switch (operation) {
        case PERF_REGISTER:
            printk(KERN_DEBUG "PID[%d]: PERF_REGISTER\n", task[index].pid);

            if (Add_table_entry(PERF_REGISTER, NULL) != 0) ERROR=true;
            strcpy(task[index].respbuf, "0\n");
            break;

        case PERF_DETECT:
            printk(KERN_DEBUG "PID[%d]: PERF_DETECT\n", task[index].pid);

            if (Add_table_entry(PERF_DETECT, NULL) != 0) ERROR=true;
            break;

        case PERF_REPORT:
            printk(KERN_DEBUG "PID[%d]: PERF_REPORT\n", task[index].pid);
            param = strchr(callbuf, ' ');
            if (param != NULL) {
                param[0] = '\0';
                param++;
                printk(KERN_DEBUG "PERF_REPORT says: %s\n", param);
            }

            if (Add_table_entry(PERF_REPORT, param) != 0) ERROR=true;
            break;

        case PERF_LIST:
            printk(KERN_DEBUG "PID[%d]: PERF_LIST\n", task[index].pid);
            param = strchr(callbuf, ' ');
            if (param != NULL) {
                param[0] = '\0';
                param++;
                if (kstrtoint(param, 10, &root_index) == 0)
                    printk(KERN_DEBUG "PERF_LIST root_index: %d\n", root_index);
                else ERROR = true;
            }

            if (List_table_entry(root_index) != 0) ERROR = true;
            break;

        case PERF_LIST_ALL:
            printk(KERN_DEBUG "PID[%d]: PERF_LIST_ALL\n", task[index].pid);
            root_index = -2;

            if (List_table_entry(root_index) != 0) ERROR = true;
            break;

        case PERF_RESET:
            printk(KERN_DEBUG "PID[%d]: PERF_RESET\n", task[index].pid);
            t_index = 0;    // not 0, need to contain this task
            tab_index = -1;
            strcpy(task[index].respbuf, "0\n");
            break;

        default:
            printk(KERN_DEBUG "PID[%d]: ERROR\n", task[index].pid);
            break;
    }

    if (ERROR) strcpy(task[index].respbuf, "-1\n");

    //printk(KERN_DEBUG "amos_perf: call %s will return %s", callbuf, task[index].respbuf);
    preempt_enable();

    *ppos = 0;     /* reset the offset to zero */
    return count;  /* write() calls return the number of bytes, and unblock user program */
}

static int List_table_entry(int root_index)
{
    int i, j;
    int ERROR = -1;

    if (tab_index >= MAX_TASK) return -1;

    for (i=0; i <= tab_index ;i++) {

        if (table[i].op == PERF_REGISTER) {
            if (root_index == -1) ERROR = Print_entry(i);
            else if (root_index == -2) {
                ERROR = Print_entry(i);
                for (j=i+1; j <= tab_index; j++)
                    if (table[i].pid == table[j].tgid) Print_entry(j);
            }
            else if (root_index == i) {
                ERROR = Print_entry(i);
                for (j=i+1; j <= tab_index; j++)
                    if (table[i].pid == table[j].tgid) Print_entry(j);
            }
            else ;
        }
    }

    return ERROR;
}

static int Print_entry(int index)
{
    char resp_line[MAX_LINE];  // local (kernel) space for a response

    if( table[index].root == -1 ) {
        printk(KERN_DEBUG "[%d] %s pid %d, tgid %d, %s has %d threads\n"
                , index
                , (table[index].op == PERF_REGISTER) ? "R" : "D"
                , table[index].pid
                , table[index].tgid
                , table[index].tcomm
                , table[index].num_thread);

        sprintf(resp_line, "[%2d] pid %d, tgid %d, %s detects %d threads\n"
                , index
                , table[index].pid
                , table[index].tgid
                , table[index].tcomm
                , table[index].num_thread);
        strcat(task[t_index-1].respbuf, resp_line); // finish the response
    }
    else {
        printk(KERN_DEBUG "  ->[%d] %s pid %d, tgid %d, %s says %s\n"
                , index
                , (table[index].op == PERF_REGISTER) ? "R" : "D"
                , table[index].pid
                , table[index].tgid
                , table[index].tcomm
                , table[index].respbuf);

        sprintf(resp_line, "  -> pid %d, %s\n"
                , table[index].pid
                , table[index].respbuf);
        strcat(task[t_index-1].respbuf, resp_line); // finish the response
    }

    return 0;
}

static int Add_table_entry(int operation, char *report)
{
    int i = 0;
    u64 exe_time;            
    int ERROR = -1;

    if (tab_index >= MAX_TASK) return -1;

    switch (operation) {
        case PERF_REGISTER:
            table[tab_index+1].root = -1;
            ERROR = 0;
            break;
        case PERF_DETECT:
            for (i = tab_index; i >= 0; i--) {
                if (table[i].op == PERF_REGISTER && table[i].pid == task_tgid_nr(current)) {
                    table[tab_index+1].root = i;
                    table[i].num_thread++;
                    break;
                }
            }
            if (i < 0) return -1;

            table[tab_index+1].start = (u64)ktime_to_ms(ktime_get());
            strcpy(table[tab_index+1].respbuf, "Not ready...");
            ERROR = 0;
            break;
        case PERF_REPORT:
            for (i = tab_index; i >= 0; i--) 
                if (table[i].pid == task_pid_nr(current)) break;

            if (i < 0) return -1;
            table[i].end = (u64)ktime_to_ms(ktime_get());
            //strcpy(table[i].respbuf, report);
            exe_time = table[i].end - table[i].start;

            sprintf(table[i].respbuf, "EXE_T %llu, %s", exe_time, report);
            ERROR = 0;
            break;
        default:
            break;
    }

    if (operation == PERF_REGISTER || operation == PERF_DETECT) {
            tab_index++;
            table[tab_index].op       = operation;
            table[tab_index].pid      = task_pid_nr(current);
            table[tab_index].tgid     = task_tgid_nr(current);
            table[tab_index].call_tsk = current;
            get_task_comm(table[tab_index].tcomm, current);
    }
 
    return ERROR;
}

static int Create_new_task(int operation)
{
    int index = 0;

    if (t_index > MAX_TASK) return -EINVAL;

    task[t_index].op_s = operation;
    task[t_index].pid = task_pid_nr(current);
    task[t_index].call_tsk = current;
    index = t_index++;
    printk(KERN_DEBUG "amos-perf: task %d initialize, pid %d\n", index, task[index].pid);

    return index;
}

static ssize_t amos_perf_return(struct file *file, char __user *userbuf,
        size_t count, loff_t *ppos)
{
    int rc; 
    bool found = false;
    int index = 0;

    preempt_disable(); // protect static variables
    for (index = MAX_TASK-1; index >= 0; index--) {
        if (task_pid_nr(current) == task[index].pid) {
            printk(KERN_DEBUG "amos-perf R: index(%d), PID[%d], respbuf:%s", index, task[index].pid, task[index].respbuf);
            found = true;
            break;
        }
    }

    if (!found) {
        printk(KERN_DEBUG "amos-perf R: index(%d) Not found!\n", index);
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


    preempt_enable(); // clear the disable flag

    *ppos = 0;  /* reset the offset to zero */
    return rc;  /* read() calls return the number of bytes */
} 

// Defines the functions in this module that are executed
// for user read() and write() calls to the debugfs file
static const struct file_operations my_fops = {
    .read            = amos_perf_return,
    .write           = amos_perf_call,
};

/* This function is called when the module is loaded into the kernel
 * with insmod.  It creates the directory and file in the debugfs
 * file system that will be used for communication between programs
 * in user space and the kernel module.
 */

static int __init amos_perf_module_init(void)
{

    /* create an in-memory directory to hold the file */

    dir = debugfs_create_dir(dir_name, NULL);
    if (dir == NULL) {
        printk(KERN_DEBUG "amos_perf: error creating %s directory\n", dir_name);
        return -ENODEV;
    }

    //0666 is file permission,
    file = debugfs_create_file(file_name, 0666, dir, &file_value, &my_fops);
    if (file == NULL) {
        printk(KERN_DEBUG "amos_perf: error creating %s file\n", file_name);
        return -ENODEV;
    }

    printk(KERN_DEBUG "amos_perf: created new debugfs directory and file\n");

    return 0;
}

static void __exit amos_perf_module_exit(void)
{
    debugfs_remove(file);
    debugfs_remove(dir);
}


module_init(amos_perf_module_init);
module_exit(amos_perf_module_exit);
MODULE_LICENSE("GPL");
