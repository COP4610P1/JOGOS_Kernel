//references https://www.kernel.org/doc/htmldocs/kernel-api/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h> //file system calls
#include <linux/uaccess.h> //memory copy from kernel <-> userspace
#include <linux/time.h>

MODULE_LICENSE("Dual BSD/GPL");
//static void copyString(char * lString, char * rString);

static void timespec_subtract (struct timespec * result, struct timespec * x, struct timespec * y);

#define BUF_LEN 100 //max length of read/write message

static struct proc_dir_entry *proc_entry; //pointer to proc entry

static char msg[BUF_LEN];  //buffer to store read/write message
static int procfs_buf_len; //variable to hold length of message

static struct timespec last_time; 
static struct timespec elasped_time; 
//static  current_time; 

static ssize_t procfile_read(struct file *file, char *ubuf, size_t count, loff_t *ppos)
{
    struct timespec current_time = current_kernel_time();
    if((long)last_time.tv_sec == 0){
        sprintf(msg, "The current time is : %lld.%.9ld\n", (long long) current_time.tv_sec, current_time.tv_nsec);
        last_time = current_time;
    }else{
        timespec_subtract ( &elasped_time, &current_time, &last_time);
        sprintf(msg, "The current time: %lld.%.9ld\nelasped time: %lld.%.9ld\n", 
        (long long) current_time.tv_sec, current_time.tv_nsec, 
         (long long) elasped_time.tv_sec, elasped_time.tv_nsec );
   
    }

	printk(KERN_INFO "proc_read\n");
	procfs_buf_len = strlen(msg);

    

	if (*ppos > 0 || count < procfs_buf_len) //check if data already read and if space in user buffer
		return 0;
	


	if (copy_to_user(ubuf, msg, procfs_buf_len)) //send data to user buffer
		return -EFAULT;
	*ppos = procfs_buf_len; //update position
	printk(KERN_INFO "gave to user %s\n", msg);
	return procfs_buf_len; //return number of characters read
}

static ssize_t procfile_write(struct file *file, const char *ubuf, size_t count, loff_t *ppos)
{
	printk(KERN_INFO "proc_write\n");
	//write min(user message size, buffer length) characters
	if (count > BUF_LEN)
		procfs_buf_len = BUF_LEN;
	else
		procfs_buf_len = count;
	copy_from_user(msg, ubuf, procfs_buf_len);
	printk(KERN_INFO "got from user: %s\n", msg);
	return procfs_buf_len;
}

static struct file_operations procfile_fops = {
	.owner = THIS_MODULE,
	.read = procfile_read, //fill in callbacks to read/write functions
	.write = procfile_write,
};
static int hello_init(void)
{
	//proc_create(filename, permissions, parent, pointer to fops)
	proc_entry = proc_create("timer", 0666, NULL, &procfile_fops);
	if (proc_entry == NULL)
		return -ENOMEM;
	return 0;
}

static void hello_exit(void)
{
	proc_remove(proc_entry);
	return;
}



//reference : https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.5/html_node/Elapsed-Time.html
/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */

static void timespec_subtract (struct timespec * result, struct timespec * x, struct timespec * y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_nsec < y->tv_nsec) {
    int nsec = (y->tv_nsec - x->tv_nsec) / 1000000 + 1;
    y->tv_nsec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_nsec - y->tv_nsec > 1000000) {
    int nsec = (x->tv_nsec - y->tv_nsec) / 1000000;
    y->tv_nsec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  /* Return 1 if result is negative. */
//    if(x->tv_sec < y->tv_sec){
//        resul
//    }
}


module_init(hello_init);
module_exit(hello_exit);
